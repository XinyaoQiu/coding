# Chapter 61 — Search and the Inverted Index

> **Prerequisites:** Chapters 01 (§1 B-trees vs LSM, §4 partitioning), 02 (caching), 60 (the crawler that feeds this)
> **Patterns:** inverted index, document vs term sharding, immutable segments with background merge, retrieval then ranking, tail latency under scatter-gather

---

## 1. The problem

Given a corpus of documents and a query of a few words, return the most relevant documents, ranked, in
well under a second.

The naive statement hides which half is hard. *Finding* the documents containing a set of words is a
solved problem with a data structure sixty years old, and it is fast. *Ordering* them by relevance is not
solved and never will be, because relevance is a property of the user's intent rather than of the text.
This chapter is mostly about the first half — the index — because that is the part with a right answer,
and because the retrieval design determines what the ranking stage is even able to consider.

The structure at the center is the **inverted index**: rather than mapping documents to the words they
contain, map each word to the list of documents containing it. A query for two words becomes an
intersection of two lists. That inversion is the entire idea, and everything else is engineering around
its consequences.

**The property that makes it hard:** the index must be simultaneously enormous, updated continuously, and
queried with a latency budget measured in tens of milliseconds — and those three requirements pull in
opposite directions. Making it fast to query wants immutable, densely-packed, compressed structures.
Making it fast to update wants mutable ones. Making it enormous forces it across many machines, at which
point every query's latency becomes the *slowest* of many parallel requests rather than the average.

---

## 2. Requirements

### Functional

1. Index documents (text plus structured attributes) as they arrive.
2. Serve a keyword query, returning ranked, paginated results.
3. Reflect new and updated documents within a bounded, stated delay.

Defer, but name: spelling correction, query understanding and synonyms, personalization, and semantic
retrieval via embeddings (which is a genuinely different index — see §10).

### Non-functional

- **Query latency** — p99 under 200 ms server-side. The p99, not the mean, and §7.6 is about why that
  distinction dominates the architecture.
- **Indexing delay** — new documents searchable within seconds to a minute. State the number; it is the
  requirement that forces the segment design in §7.2 and it varies enormously by product (a news index and
  a legal archive have nothing in common here).
- **Corpus size** — assume 10^9 documents. Below ~10^7 this is a single-node problem and most of the
  chapter is unnecessary; say so.
- **Query rate** — 10,000 QPS at peak.
- **Availability** — degraded results (fewer shards responding) are better than an error. This is a
  requirement with teeth: it licenses returning partial results, and §7.6 depends on it.
- **Consistency** — eventual. A document appearing in results thirty seconds after it was written is
  invisible to users.

### Explicitly out of scope

Crawling and acquisition (Chapter 60), and the ranking *model* itself — this chapter designs where ranking
happens and what it can see, not what it computes.

---

## 3. Estimation

Assume 10^9 documents averaging 1,000 words, with 200 unique terms each after normalization.

**Index size**

```
postings = 1e9 docs × 200 unique terms/doc = 2 × 10^11 postings
```

Each posting is a document ID plus a term frequency and, if you want phrase queries, a position list.

```
naive:  8 B docid + 4 B freq                = 12 B × 2e11 = 2.4 TB
with positions (~1.5 per posting avg)       ≈ 4.8 TB
compressed (delta + variable-byte, §7.5)    ≈ 0.6 – 1.2 TB
```

**Compression buys a factor of four to six**, and that is the difference between the index fitting in the
aggregate memory of a reasonable cluster and not. This is the number that justifies spending design effort
on posting-list encoding, which otherwise looks like premature optimization.

**Vocabulary**

```
distinct terms in a large web corpus ≈ 10^7 – 10^8 (dominated by junk: typos, IDs, hashes)
```

The term dictionary itself is a few gigabytes and must be in memory. Note the shape: **term frequency
follows Zipf's law**, so the top ~100 terms appear in a large fraction of all documents while the long
tail appears once. This skew is the reason term sharding fails (§7.1) and the reason posting lists need
skip structures (§7.5).

**Shard count**

```
1.2 TB index ÷ ~50 GB per node of resident index = ~24 shards
with 3 replicas each                             = ~72 index-serving nodes
```

**Query fan-out — the constraining number**

```
10,000 QPS × 24 shards = 240,000 shard-queries/sec
```

Every user query becomes twenty-four internal queries. This is the number that shapes the architecture,
and it has a consequence that is not obvious from the arithmetic: with a fan-out of 24, **the p99 of the
user query is roughly the p99.96 of a single shard**, because the slowest of 24 parallel calls determines
the answer. Tail latency is amplified, not averaged. §7.6 is entirely about this.

**Indexing rate**

```
1% of the corpus changes daily = 10^7 docs/day ≈ 116 docs/sec
```

Modest in aggregate — but bursty, and each document touches ~200 posting lists, so it is 23,000 posting
updates per second spread across every shard. This is what makes in-place mutation untenable (§7.2).

---

## 4. API

```
GET /search
  ?q=distributed+systems
  &filters=lang:en,type:article
  &sort=relevance|date
  &cursor=<opaque>&limit=10
  -> 200 {results:[{docId, title, snippet, score}], nextCursor, took_ms, partial:bool}

POST /index      {docId, fields:{...}, version}   -> 202
DELETE /index/{docId}                              -> 202
```

Four decisions:

**`partial: true` is part of the contract.** With a fan-out of 24 and a hard latency budget, some queries
will return before every shard has answered (§7.6). Making this visible in the response — rather than
silently returning incomplete results, or waiting indefinitely — is the honest design, and it lets the
client decide whether to retry or annotate.

**`took_ms` is returned.** Search latency varies by two orders of magnitude across queries, and exposing it
makes the client's behavior (and your dashboards) able to distinguish "slow query" from "slow system".

**Cursor pagination, and the cursor is expensive.** Deep pagination in a distributed index is genuinely
costly: to return results 1000–1010 ranked globally, every shard must return its top 1010, because any
shard could hold all of them. The work grows linearly with the page depth on *every* shard. Cap the depth
(most engines cap around 10,000) and say so; products that need unbounded scrolling need a different
access pattern, such as a `search_after` cursor keyed on the sort value, which resumes without re-ranking
the prefix.

**Indexing is `202`, not `200`.** The document is queued; it becomes searchable after the next refresh
(§7.2). Pretending otherwise in the API contract creates a false expectation that leads clients to write
read-after-write tests that flake.

---

## 5. Data model

The index is not a table. Its logical shape:

```
term dictionary            term -> (df, pointer into postings file)
  "distributed"  -> df=2.1e6, offset 0x4A3F...
  "systems"      -> df=8.9e6, offset 0x91B2...

postings file (per term, delta-encoded, compressed, with skip list)
  "distributed": [ (docid=17,  tf=3, pos=[4,88,201]),
                   (docid=204, tf=1, pos=[12]), ... ]

document store             docid -> stored fields (title, url, snippet source)
norms / doc lengths        docid -> length, for length normalization in scoring
deletion bitset            docid -> deleted?     (see §7.3)
```

Three deliberate decisions:

**Internal document IDs are dense, monotonically assigned integers local to a segment,** not the external
`docId` string. Dense small integers are what make delta encoding effective (§7.5) and what let the
deletion bitset be a bitset rather than a hash set. A separate map handles external-to-internal
translation. Choosing dense internal IDs is a decision that pays off in three different places, and it is
easy to get wrong by using the external ID directly.

**Postings are sorted by document ID, not by score.** Sorted-by-ID is what makes intersection a linear
merge with skip-ahead. Sorted-by-score would make single-term queries trivially fast and multi-term
queries impossible to intersect efficiently — a trade almost never worth making, though "impact-ordered"
indexes that do exactly this exist for specific workloads.

**The document store is separate from the index.** The index answers "which documents", the store answers
"what do they say". They have different access patterns (sequential scan vs random point read) and
different sizes, and conflating them means the hot index competes for memory with cold stored text.

---

## 6. Architecture, derived

### Attempt 1: scan every document

`grep` over the corpus. For 10^9 documents this is obviously hopeless, and it is worth one sentence only
to establish the baseline the index improves on: linear in corpus size per query, versus linear in the
number of *matching* documents.

### Attempt 2: a forward index — document to terms

Store each document's term list; to answer a query, check each document.

Still linear in the corpus. The forward index is genuinely useful — it is what you need to *delete* a
document's postings, and what a re-index reads — but it does not answer queries.

### Attempt 3: invert it

Map term to document list. A single-term query is a lookup and a scan of one list. A two-term AND is an
intersection of two sorted lists, which is a linear merge in the length of the shorter one, accelerated by
skip pointers.

```
"distributed" ∩ "systems"
  df 2.1e6 and 8.9e6 → walk the shorter, skip ahead in the longer
  ≈ O(shorter list) with skips, not O(sum)
```

This works, and at 10^9 documents it does not fit on one machine. Which leads to the real question.

### Attempt 4: shard it — and the choice of *how* is the chapter

Two ways to split an inverted index across N machines, and they are not close in practice even though they
look symmetric on paper. §7.1 argues it in full; the conclusion is **shard by document**.

Each shard holds a complete inverted index over its own subset of documents. A query goes to all shards,
each returns its local top-K, and a coordinator merges.

### Attempt 5: make segments immutable

Updating a posting list in place means a random write into a large compressed file for each of the ~200
terms in every changed document — 23,000 such writes per second across the fleet, each read-modify-write
on a compressed structure. Untenable.

Instead: **buffer new documents in a small in-memory mutable index, periodically flush it to disk as a new
immutable segment, and merge segments in the background.** A query searches every segment and merges the
results. Deletes are tombstones in a bitset, applied at query time and physically removed at merge.

This is precisely the LSM-tree structure from Chapter 01, §1, arrived at independently from a different
requirement — and noticing that is worth saying out loud, because the trade-off profile transfers exactly:
excellent write throughput, read amplification proportional to segment count, and background merging that
competes for I/O with serving.

### Attempt 6: separate retrieval from ranking

Scoring every matching document with an expensive model is impossible: a two-word query can match tens of
millions of documents, and a modern ranking model costs milliseconds per document.

```
retrieval (cheap, BM25 over posting lists) → top ~1,000 per shard
   → merge across shards                   → top ~1,000 globally
      → rerank (expensive model, features) → top 10
```

The cheap first stage must be *recall-oriented*: it does not need to order well, it needs to not lose the
right answer. The expensive second stage orders. This is the same two-stage structure as Chapters 22, 51,
and A4, and it appears every time an expensive scorer meets a large candidate set.

### Final architecture

```
  INDEXING                                     QUERY

  documents                          Client ──► Coordinator
     │                                              │  (parses, plans, sets deadline)
     ▼                                              │
  analysis: tokenize, lowercase,        ┌───────────┼───────────┐   scatter to 24 shards
  stem, remove stopwords,               ▼           ▼           ▼
  extract fields                     Shard 1     Shard 2  ...  Shard 24
     │                                  │           │           │
     ▼                              ┌───┴────┐  (each: in-memory segment
  route by hash(docId) % 24         │ segments│   + N immutable segments
     │                              │ 0..N    │   + deletion bitset)
     ▼                              │ merge   │
  shard's in-memory buffer          └───┬────┘
     │  (refresh every ~1 s)            │ local top-K by BM25
     ▼                                  ▼
  flush → immutable segment      Coordinator: merge K-lists, take global top-K
     │                                  │
     ▼                                  ▼
  background merge                Reranker (expensive model, ~1,000 docs)
  (tiered, see §7.2)                    │
                                        ▼
                                  Document store: hydrate titles + snippets
                                        │
                                        ▼
                                    10 results
```

---

## 7. Deep dives

### 7.1 Document sharding versus term sharding

The central architectural decision, and the one worth arguing rather than asserting.

**Term sharding (partition the vocabulary).** Shard 1 holds every posting list for terms A–F, shard 2 for
G–L, and so on. A query for "distributed systems" touches exactly two shards.

**Document sharding (partition the corpus).** Every shard holds a complete index over 1/24 of the
documents. Every query touches all 24 shards.

On paper term sharding looks superior: two shard-queries instead of twenty-four, which by the fan-out
arithmetic in §3 would be a 12× reduction in internal load and would eliminate the tail-amplification
problem entirely. It is nonetheless almost universally wrong, for four reasons that compound.

**Zipf's law makes load catastrophically skewed.** Term frequency follows a power law. The shard holding
"the", "and", and "data" serves a large fraction of all queries; the shard holding terms starting with "q"
and "z" is idle. You cannot rebalance by moving terms, because the skew is in the *terms themselves* —
splitting a single hot term's posting list across machines means reintroducing a scatter for that term,
which is document sharding wearing a disguise.

**Intersection requires shipping posting lists across the network.** To intersect "distributed" (2.1M
postings) with "systems" (8.9M postings) when they live on different machines, one list must travel. At
~1 MB compressed for the shorter list, per query, at 10,000 QPS, that is 10 GB/s of internal traffic for a
single two-term query pattern. Document sharding ships only the top-K results — a few kilobytes.

**Every document update touches many shards.** A document with 200 unique terms updates posting lists on,
in expectation, most of the shards. Indexing becomes a distributed transaction across the fleet.
Under document sharding, a document's update is entirely local to one shard.

**Adding capacity requires redistributing the vocabulary,** which means rewriting posting lists.
Under document sharding, adding a shard means routing new documents to it and optionally rebalancing
whole documents — coarse, movable units.

**The verdict:** document sharding wins because it makes writes local and load even, and it pays for this
with query fan-out — a cost that is real, that §7.6 addresses, and that is bounded and predictable in a
way that term sharding's skew is not.

The one setting where term sharding is defensible: a small, controlled vocabulary with uniform frequency
and a read-only corpus. If you can honestly claim all three, say so; otherwise this is a trap the
symmetry of the two options is designed to spring.

### 7.2 Immutable segments and the refresh interval

New documents go into an in-memory buffer. Periodically — the **refresh interval** — the buffer is
converted into an immutable on-disk segment and becomes visible to queries. Segments accumulate; a
background process merges small ones into larger ones.

**Why immutability.** Writing into an existing compressed, delta-encoded posting list requires decoding,
inserting, re-encoding, and rewriting — for 200 lists per document. Immutability turns that into a
sequential append of a whole new segment, and defers the cost to a merge that batches many documents'
worth of work.

**The refresh interval is the central knob**, and it is a three-way trade-off that candidates usually
present as two-way:

- **Shorter interval → fresher results, more segments.** More segments means every query consults more
  posting lists for the same term, so **read amplification is linear in segment count.**
- **Longer interval → fewer segments, staler results.**
- **And: every refresh costs a flush**, so a very short interval spends real I/O producing tiny segments
  that will immediately be merged away.

One second is the common default. A search-as-you-type product may need 100 ms and pay for it in query
latency; a document archive can use minutes and enjoy a much cleaner segment structure. **Ask which
product it is** — this is the requirement from §2 that has the largest architectural consequence, and it
is the one most likely to be left unstated in the prompt.

**Merge policy.** Tiered merging (merge segments of similar size, as in an LSM-tree) minimizes write
amplification. Leveled merging keeps segment count lower, improving read latency, at the cost of more
rewriting. The failure mode to name: **merge storms**, where a large merge saturates disk I/O and query
latency spikes for its duration. Mitigate by throttling merge I/O and by scheduling large merges away from
peak, and accept that a search cluster's p99 has a sawtooth in it.

**Segment warming.** A freshly-merged segment's data is not in the page cache, so the first queries against
it are slow. Pre-warm by touching the new segment before swapping it in.

### 7.3 Deletion and update in an immutable structure

You cannot remove a posting from an immutable segment. Instead, maintain a **deletion bitset** per
segment: one bit per internal document ID, marking whether it is deleted.

Queries consult the bitset and skip deleted documents after retrieving them from the posting lists — so a
deleted document still costs the work of being found. Physical removal happens at merge time, when the
segment is rewritten anyway and deleted documents are simply not copied forward.

**An update is a delete plus an insert.** The old version is tombstoned, the new version is added to the
in-memory buffer. Two consequences worth stating:

- **A high-churn corpus accumulates tombstones**, and read amplification grows with them. A document
  updated hourly leaves 24 tombstones a day in segments that have not yet been merged. Merge policy must
  account for deletion density, not just segment size — a segment that is 60% deleted should be merged
  even if it is large.
- **Term statistics drift.** Document frequency (`df`) used in scoring counts postings including deleted
  ones, so scores are slightly wrong between merges. This is universally tolerated and worth mentioning
  because it demonstrates you know what the bitset does and does not fix.

### 7.4 Scoring: BM25 and where the expensive model goes

The cheap first-stage scorer is almost always **BM25**, and knowing roughly what it does is expected:

```
score(d, q) = Σ_{t in q}  IDF(t) · ( tf(t,d) · (k1+1) )
                          ─────────────────────────────────────
                          tf(t,d) + k1 · (1 - b + b · |d|/avgdl)
```

Three ideas, each correctable to a plain-English sentence:

- **IDF** — a term appearing in few documents is more informative than one appearing in many. This is why
  "the" contributes nothing and "photosynthesis" contributes a lot.
- **Term frequency saturation** (the `k1` term) — a document mentioning a word twenty times is more
  relevant than one mentioning it twice, but not ten times more. Raw term frequency, used directly, is
  trivially gamed by repetition; saturation is the fix.
- **Length normalization** (the `b` term) — a long document contains more words by accident, so matches in
  it are weaker evidence.

BM25 is computed from data already in the posting list (`tf`) and the dictionary (`df`), which is why it is
cheap enough to run over millions of candidates. Anything requiring features outside the index — user
context, click history, a neural model over the text — belongs in the rerank stage over ~1,000 candidates.

**The retrieval stage's job is recall, not precision.** If the right document is not in the top 1,000 from
retrieval, no reranker can recover it. Tuning retrieval for precision is a common and wasted effort;
tuning it for recall at the candidate-set size is the right objective.

### 7.5 Posting list compression and skip lists

The §3 arithmetic said compression is worth a factor of four to six. The mechanism:

**Delta encoding.** Posting lists are sorted by document ID, so store gaps rather than values:
`[17, 204, 209, 1150]` becomes `[17, 187, 5, 941]`. Gaps are small, especially for common terms whose
postings are dense.

**Variable-byte or SIMD-friendly integer encoding.** Small gaps need one byte, not eight. Frame-of-
reference and PForDelta encode blocks of 128 values with a shared bit width plus exceptions, which
decodes with vectorized instructions rather than branches — and decode speed matters as much as size,
because a query decodes millions of postings.

**Skip lists.** Delta encoding destroys random access: you cannot jump to document 50,000 without decoding
everything before it. Intersection needs exactly that jump — when walking the short list and the long one,
you want to skip ahead in the long list rather than decode through it. Store a skip structure every N
postings holding an absolute document ID and an offset. Intersection cost becomes roughly proportional to
the *shorter* list rather than the sum.

The interaction is the point: **delta encoding makes the index small, skip lists give back the random
access it removed, and the pair together is what makes multi-term queries fast.** Either alone is
insufficient.

### 7.6 Tail latency, which is the real operational problem

From §3: with 24 shards, the user-visible p99 is approximately the single-shard p99.96. Concretely, if
each shard has a 1% chance of taking longer than 100 ms, then

```
P(at least one of 24 shards is slow) = 1 - 0.99^24 ≈ 21%
```

**One in five queries hits a slow shard.** A per-shard tail that would be unremarkable in isolation becomes
the dominant term in user-visible latency. This is the defining operational characteristic of any
scatter-gather system, and it is why the availability requirement in §2 was written to permit partial
results.

Four mitigations, in the order they are usually applied:

**Hedged requests.** If a shard has not responded by the 95th-percentile latency, send a duplicate request
to a replica and take whichever returns first. This costs ~5% extra load and collapses the tail, because
it converts "wait for the slow one" into "wait for the faster of two independent draws". It is the single
highest-leverage technique here.

**A deadline, and returning partial results.** The coordinator sets a hard budget. Shards that miss it are
excluded and the response is flagged `partial: true`. Losing 1/24 of the corpus on 1% of queries is
imperceptible; a 3-second query is not.

**Shard-level load balancing and replica choice.** Route to the replica with the shortest queue rather
than round-robin. Slow shards are usually slow for a transient reason — a merge, a GC pause, a cold cache
— and queue depth detects that faster than health checks do.

**Reduce the fan-out.** Fewer, larger shards means a smaller tail multiplier. This trades against per-shard
latency and rebuild time, and it is why shard sizing is a latency decision rather than purely a capacity
one.

The general lesson transfers well beyond search: **in any scatter-gather system, the tail of the component
is the mean of the whole.** A candidate who says this and computes the `1 - 0.99^N` is demonstrating
something more valuable than knowledge of inverted indexes.

### 7.7 Caching

Query distribution is Zipfian: a small number of queries account for a large fraction of traffic.

- **Result cache**, keyed on the normalized query plus filters plus page. Very high hit rate on the head.
  Invalidated by time (a short TTL) rather than by content change, because tracking which cached results
  a new document would affect is intractable — and a few seconds of staleness on a search result is
  already implied by the refresh interval.
- **Posting list cache** — the operating system's page cache does most of this for free, which is why
  index nodes should be sized so the hot index fits in RAM and why "how much of the index is resident" is
  the metric that predicts latency.
- **Do not cache the reranked result if the ranker is personalized**, or you will serve one user's
  personalization to another. Cache the retrieval stage, rerank per user. Splitting the cache at the stage
  boundary is the reason to keep the stages separate in the first place.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| One shard slow (merge, GC, cold cache) | 21% of queries slow, per the fan-out arithmetic | Hedged requests; deadline plus partial results; queue-depth-based replica choice |
| One shard down | 1/24 of corpus missing from every result | Replicas; serve `partial: true` rather than failing the query |
| Merge storm | Disk saturated, latency spikes cluster-wide | Throttle merge I/O; stagger merges across shards; schedule large merges off-peak |
| Indexing pipeline stalled | Results go stale silently — no error anywhere | Alarm on indexing lag, not on error rate; this failure is invisible without it |
| Refresh interval too short | Segment explosion, read amplification, latency creep | Alarm on segment count per shard |
| Deep pagination request | Every shard computes and ships thousands of results | Cap page depth in the API; offer `search_after` cursors instead |
| Hot query (a breaking news term) | One posting list dominates cache and CPU | Result cache absorbs it; this is the failure the cache is actually for |
| Coordinator overload | Whole search down | Coordinators are stateless — scale horizontally, and keep merge logic cheap |

**Monitoring:** query p99 *and* p99.9 (the mean is nearly useless here); per-shard latency distribution,
because the cluster p99 is a shard tail and you need to see which shard; segment count per shard;
deletion density per segment; indexing lag; result cache hit rate; and the rate of `partial: true`
responses, which is the direct measure of whether the deadline is set correctly.

---

## 9. Common mistakes

1. **Choosing term sharding** because the fan-out arithmetic looks better, without confronting Zipfian
   load skew, cross-network intersection, and distributed writes.
2. **Not knowing why the index must be immutable**, and proposing in-place posting list updates at 23,000
   updates per second.
3. **Treating the refresh interval as a constant** rather than the product-defining knob it is, and not
   asking how fresh results must be.
4. **Forgetting that a deleted document still costs retrieval work** until merge, and that high-churn
   corpora accumulate tombstones.
5. **Scoring every match with the expensive model**, rather than retrieving cheaply and reranking a
   bounded candidate set.
6. **Ignoring tail-latency amplification.** With a fan-out of 24, per-shard p99 is the wrong metric to
   optimize; the compounding is the architecture's defining cost.
7. **Promising unbounded deep pagination**, which forces every shard to rank the full prefix on every page.
8. **Sorting posting lists by score** for faster single-term queries, breaking intersection for everything
   else.
9. **Caching personalized results**, leaking one user's ranking to another.

---

## 10. Variants

**Log and observability search.** Time-dominated: almost every query has a time range, so shard by time
window rather than by document hash. Old shards become read-only and can be merged aggressively, moved to
cheaper storage, or dropped wholesale by retention — none of which is available when sharding by hash.
The write rate is orders of magnitude higher and relevance ranking is largely irrelevant, so the trade-offs
invert.

**Product and catalog search.** Small corpus (10^6–10^7), so it fits on one node and fan-out disappears.
The hard part moves entirely into ranking, faceting, and business rules — which is Chapter 51's shape more
than this one's.

**Semantic / vector retrieval.** Replaces the inverted index with an approximate nearest-neighbor index
over embeddings (HNSW, IVF-PQ). It retrieves by *meaning* rather than by term overlap, and it fails
differently: it cannot guarantee that a document containing the exact query string is returned. The
current mainstream answer is **hybrid retrieval** — run both, fuse the two ranked lists — precisely because
their failure modes are complementary. Vector indexes are also much more expensive to update, which
re-raises §7.2's problem in a harder form.

**Autocomplete (Chapter 62).** Not this index. Prefix matching over a trie with precomputed top-K, because
the latency budget is ~50 ms and the query is a prefix rather than a set of terms.

---

## 11. Further reading

- Chapter 60, for the crawler that produces the corpus; Chapter 51, for geo filters composed into this
  index; Chapter 22, for the retrieval/ranking split in its general form
- Chapter 01, §1, for the LSM-tree that §7.2 rediscovers
- Manning, Raghavan, and Schütze, *Introduction to Information Retrieval* — the standard reference for
  everything in §7.1 through §7.5, freely available online
- Jeffrey Dean and Luiz André Barroso, "The Tail at Scale" (CACM, 2013) — the definitive treatment of
  §7.6, including hedged requests
- The Apache Lucene documentation on segments, merge policies, and codecs, for how these ideas are
  actually implemented
