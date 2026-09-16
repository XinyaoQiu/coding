# Chapter 62 — Autocomplete / Typeahead

> **Prerequisites:** Chapters 02 (caching), 61 (the search index this sits in front of), 71 (top-K)
> **Patterns:** precomputation at every node, offline build with atomic swap, prefix sharding, latency budget as a hard architectural constraint

---

## 1. The problem

A user types into a search box. After each keystroke, ten suggested completions appear.

This looks like a smaller version of Chapter 61 and is not. It is a different problem with a different
data structure, for one reason: **the latency budget is set by human typing speed, not by human patience.**
A search result may take 200 ms; a suggestion list that takes 200 ms arrives after the next keystroke has
already invalidated it, and the user sees suggestions flickering behind their own typing. The budget is
roughly 50 ms end to end, including the network.

That single constraint eliminates most designs before they are considered. There is no time to query an
index, rank results, and merge across shards. There is barely time for one network round trip and one
memory lookup. Everything in this chapter follows from spending the budget on the wire and leaving almost
nothing for the server.

The second thing that makes it unlike search: the query rate is a multiple of the search rate, because
every search is preceded by several keystrokes. Autocomplete is the highest-QPS surface in a search
product, serving suggestions for queries that mostly will never be issued.

**The property that makes it hard:** a 50 ms budget against a request rate several times the search rate,
for a query type — prefix matching — that a general-purpose index handles badly. The saving grace, and the
thing the design exploits relentlessly, is that **the answer set is tiny, stable, and computable in
advance**: the ten best completions of "resta" were the same yesterday and will be the same tomorrow.

---

## 2. Requirements

### Functional

1. Given a prefix, return the top ten completions ranked by popularity.
2. Reflect newly popular queries within a stated delay.
3. Handle prefixes that match nothing, gracefully.

Defer, but name: personalization, spelling correction and fuzzy matching (§10 — genuinely harder, and the
right move is to scope it out), multi-word and mid-string matching, and entity suggestions mixed with
query suggestions.

### Non-functional

- **Latency** — p99 under 50 ms end to end, of which the server gets perhaps 10 ms. This is the
  requirement; everything else is negotiable.
- **Query rate** — several times the search rate. Assume 50,000 QPS at peak against 10,000 searches/sec.
- **Freshness** — hours is acceptable for the general case. This is the surprising requirement, and it is
  what licenses the entire offline-build design (§6, attempt 3). The exception is breaking news, handled
  separately in §7.4.
- **Availability** — degraded (no suggestions) is acceptable. A search box with no dropdown still works.
  This is a genuinely weak availability requirement and it should be exploited: it permits an aggressive
  design that fails open.
- **Consistency** — irrelevant. Two users seeing slightly different suggestions is not a defect.

### Explicitly out of scope

The search itself (Chapter 61), and query understanding. Also: returning *documents*. Autocomplete returns
*queries*, and conflating the two is a common early error that leads to designing the wrong index.

---

## 3. Estimation

Assume a search engine handling 10,000 searches/sec.

**Request rate — the constraining number**

```
average query length ≈ 20 characters
suggestions requested after each keystroke, debounced to ~1 per 2 chars
                     ≈ 10 requests per search... but many users select a
                       suggestion early, so call it 5

10,000 searches/sec × 5 = 50,000 autocomplete QPS
```

**Fifty thousand queries per second, each with a 10 ms server budget.** Together these say the per-request
work must be a single memory lookup. Not a scatter-gather, not an index scan, not a sort — a lookup.

**Corpus**

```
distinct queries seen in a month              ≈ 10^8
after filtering to those seen ≥ 5 times       ≈ 10^7
```

The filter is doing enormous work. The long tail of once-seen queries is typos, session IDs pasted into
the box, and noise; none of it should ever be suggested. **Discarding 90% of the vocabulary before
building anything is the first and cheapest optimization**, and it improves quality as well as size.

**Trie size**

```
10^7 queries × ~20 chars, with heavy prefix sharing
distinct nodes                                ≈ 2 × 10^7
per node: children map + 10 precomputed suggestions

naive (pointer-per-child map, string suggestions)
  ≈ 2e7 × (200 B children + 10 × 40 B strings) ≈ 12 GB
with suggestions stored as 4-byte IDs into a string table
  ≈ 2e7 × (200 B + 40 B)                       ≈ 4.8 GB
```

**Five gigabytes fits in memory on one machine**, which is the second decisive number. It means sharding
is a throughput and availability decision, not a capacity one — and that changes the design considerably,
because a full copy of the trie on every node is affordable. §7.3 returns to this.

**Build cost**

```
10^8 raw query log lines/month, aggregated daily
```

A daily batch job over a hundred million rows is minutes of work on a small cluster. Rebuilding from
scratch is cheap enough that incremental update is unnecessary complexity (§7.2).

---

## 4. API

```
GET /suggest?q=resta&limit=10
  -> 200 {suggestions: ["restaurants near me", "restaurant week", ...]}
```

That is the entire API, and its minimalism is deliberate.

**No pagination, no cursor, no filters.** Ten results, always. A user never scrolls a suggestion dropdown
past ten, so supporting it would be building for a use case that does not exist while adding state to the
hottest endpoint in the system.

**`GET`, cacheable, no authentication on the shared path.** The non-personalized response depends only on
the prefix, which means it can be cached at every layer including the CDN (§7.5). Requiring auth to get a
suggestion would forfeit that, and the personalized variant should be a separate, additive path rather
than a parameter that poisons cacheability for everyone.

**The response is strings, not objects.** No scores, no metadata, no IDs. Bytes on the wire matter at
50,000 QPS with a 50 ms budget, and a client that does not need a field should not receive it.

**Debouncing belongs on the client.** Firing a request per keystroke at typing speed produces requests
that are obsolete before they return. A ~100 ms debounce plus cancellation of in-flight requests removes a
large fraction of load and improves the *perceived* latency, because the user sees the response to their
final keystroke rather than a sequence of stale ones. This is the single most effective optimization in
the whole system and it is not on the server. Say so — it is not a dodge, it is where the win is.

---

## 5. Data model

The trie node, which is the entire data model:

```
node
  children     map[byte] -> node        (or a compact array; see §7.3)
  top_k        [10] uint32              -- IDs into a global string table
                                        -- PRECOMPUTED: the best completions
                                        --  of the prefix ending at this node
```

And two side structures:

```
string_table   uint32 -> "restaurants near me"     -- interned, deduplicated
query_stats    query  -> count, last_seen          -- input to the build, not serving
```

Three deliberate decisions:

**`top_k` is stored at *every* node, not just at leaves.** This is the whole design. A lookup walks the
prefix — five characters means five pointer hops — and reads a ten-element array. It does **not** traverse
the subtree below. The work is `O(len(prefix))`, independent of how many completions exist beneath, which
is what makes "a" and "restaurants near m" cost the same.

**Suggestions are 4-byte IDs into an interned string table, not inline strings.** The string
"restaurants near me" is the top completion of "r", "re", "res", "rest", … — eighteen nodes. Storing it
inline eighteen times is what turned 4.8 GB into 12 GB in the estimate. Interning is not a micro-
optimization here; it is a factor of 2.5 on the thing that must fit in memory.

**The serving structure is read-only.** It is built offline, loaded, and never mutated. Everything that
would require mutation — new queries, changing popularity — is handled by building a new one (§7.2). This
is what lets the node layout be compact and pointer-free rather than a general-purpose map.

---

## 6. Architecture, derived

### Attempt 1: `LIKE 'resta%'` against a query table

```sql
SELECT query, count FROM queries
WHERE query LIKE 'resta%'
ORDER BY count DESC LIMIT 10
```

A B-tree on `query` makes the prefix match a range scan — this part is fine, and it is worth noting,
because prefix matching is the one string operation a B-tree handles well.

The `ORDER BY` is what kills it. The range for "a" contains millions of rows, and the database must read
all of them to find the ten with the highest count. There is no index that is simultaneously ordered by
prefix and by popularity, because those are different orderings of the same rows. A composite index on
`(query, count)` does not help: within the prefix range, the rows are ordered by `query`, not `count`.

Measured, this is tens to hundreds of milliseconds for short prefixes — and short prefixes are the most
common requests. It exceeds the entire end-to-end budget on the server alone.

### Attempt 2: a trie, traversing the subtree at query time

Build a trie. Walk to the prefix node, then traverse everything below it collecting queries, sort by
count, take ten.

The walk is fast. The traversal is not: the subtree under "a" contains a large fraction of the corpus.

```
subtree under "a"  ≈ 10^6 leaf queries
collect + sort     ≈ tens of ms, and worse for shorter prefixes
```

The cost is inversely related to prefix length, so it is highest exactly where traffic is highest. Again
over budget.

The insight from the failure: the expensive part is *ranking the subtree*, and the subtree does not change
between requests. It is being recomputed 50,000 times a second to produce the same answer.

### Attempt 3: precompute top-K at every node

Store the ten best completions at each node when the trie is built. A query walks `len(prefix)` nodes and
reads an array.

```
lookup("resta") = 5 pointer hops + 1 array read ≈ microseconds
```

Four orders of magnitude below the budget. The cost moves entirely to build time, where there is no
latency requirement.

Computing `top_k` at build time is a single bottom-up pass: a node's top-K is the K-way merge of its
children's top-K lists plus its own terminal query, if any. Because each merge is over at most `K × (number
of children)` elements, the whole pass is linear in the number of nodes — the same min-heap-over-candidates
structure as Chapter 71, applied over a tree rather than a stream.

### Attempt 4: build offline, swap atomically

The trie is now read-only, which raises how it ever changes.

**Do not update it in place.** Incremental insertion into a trie whose every ancestor node holds a
precomputed top-K means that adding one popular query may require updating the `top_k` of every node on
its path — and worse, evicting an entry from a node's top-K requires knowing the eleventh-best completion,
which is not stored. Maintaining that would mean storing far more than K per node and doing real work on
every write.

Instead: **rebuild the whole trie offline and swap it in atomically.** The freshness requirement (hours)
permits this, the build is minutes, and the resulting serving structure is immutable and therefore
lock-free, compact, and trivially shareable across threads.

The swap: build into memory alongside the live trie, then flip a pointer. Memory usage doubles briefly,
which at 5 GB is affordable. Warm the new structure by running a sample of recent queries against it
before flipping, so the first real requests are not paying page faults.

### Final architecture

```
  BUILD (daily, offline)                    SERVE (50k QPS)

  query logs (10^8/month)                   Client (debounce ~100 ms,
      │                                             cancel in-flight)
      ▼                                             │
  aggregate: query -> count                         ▼
      │                                          CDN / edge cache
      ▼                                       (short prefixes, §7.5)
  filter: count >= 5, remove PII,                   │ miss
  profanity, and dead queries                       ▼
      │                                     ┌── Suggest service ──┐
      ▼                                     │  full trie in RAM   │
  build trie, compute top_k bottom-up       │  (~5 GB, read-only) │
      │                                     │  walk prefix,       │
      ▼                                     │  read top_k array   │
  serialize to object storage               └─────────┬───────────┘
      │                                               │
      └──── nodes download, build in                  ▼
            memory, warm, atomic swap ──────►   10 strings, ~1 ms
```

The serving path contains no database, no index, and no network hop beyond the load balancer. That is what
a 10 ms server budget buys you the right to demand.

---

## 7. Deep dives

### 7.1 Why top-K at every node is the whole design

Worth isolating, because it is the transferable idea.

The naive framing treats the trie as an *index* — a structure that locates data. The design treats it as a
**materialized view**: every node holds the precomputed answer to the question "what are the best
completions of this prefix?", and the query does not compute anything, it retrieves.

The trade is explicit: `K` extra slots per node — about 40 bytes with interned IDs — in exchange for
turning an `O(subtree)` computation into an `O(1)` read. At 2 × 10^7 nodes that is 800 MB, and it removes
the only expensive operation in the system.

This is affordable **because the read:write ratio is extreme and the answer is stable**. 50,000 QPS
against one rebuild per day is a ratio of roughly 4 × 10^9:1. When the ratio is that lopsided, precompute
everything; the only question is what "everything" is. The same reasoning appears in Chapter 51 (§3, at
500:1) and Chapter 20 (materialized timelines), and the ratio is what licenses it in each case.

The limit of the technique: it works because `K` is small and fixed. If the product needed the top *fifty*
suggestions, or a user-specified `K`, the storage grows linearly and at some point the subtree traversal
becomes competitive again. Knowing where a technique stops working is part of knowing it.

### 7.2 Offline rebuild versus incremental update

Argued in attempt 4; the reasons are worth stating in full because "just update it incrementally" is the
obvious objection.

**Incremental insertion is not the problem — eviction is.** Adding a query to a node's top-K when it
ranks is straightforward. Removing one requires knowing what should take its place, and the node stores
only ten entries. To support eviction you must store a deeper candidate list (say the top 100) at every
node, which multiplies memory tenfold, and even then a query falling out of the top 100 requires a subtree
scan to repair.

**Mutation forfeits the compact layout.** A read-only trie can be serialized as a flat array with implicit
offsets rather than pointers, packed contiguously for cache locality, and shared across all threads with
no synchronization. A mutable one needs real pointers, allocation, and either locking or a lock-free
protocol. The serving path gets measurably slower to support an operation the freshness requirement did
not ask for.

**The freshness requirement permits the simpler thing.** Hours of staleness is fine for the general case.
The one exception — genuinely new queries that must appear within minutes — is a *different* and much
smaller problem, and §7.4 solves it with a second structure rather than by making the main one mutable.

This is a recurring shape: when a requirement applies to 0.1% of the data, satisfy it with a small side
structure rather than by weakening the design for the other 99.9%.

### 7.3 Sharding, and why replication beats partitioning here

The trie fits in 5 GB, so a single node can hold all of it. Sharding is therefore about **throughput and
availability**, not capacity — and that changes the answer.

**Full replication (the choice).** Every serving node holds the complete trie. A request goes to any node
and is answered locally. Scaling out is adding nodes. No fan-out, so none of Chapter 61's tail-latency
amplification, which matters enormously against a 50 ms budget where a scatter-gather's slowest-of-N
would consume the whole thing.

**Prefix sharding (the alternative, and why it is worse).** Partition by first character or first two:
shard A serves prefixes starting "a", and so on. This would be necessary if the trie did not fit, and it
brings two problems.

The first is load skew, and it is severe: initial-letter frequency in English queries is wildly non-uniform
— "s", "t", "a", and "c" dominate, "x", "q", and "z" are nearly idle. Balancing requires grouping letters
by measured traffic rather than alphabetically, and rebalancing as language use shifts.

The second is worse and less obvious: **the single-character prefix "a" must be served by the shard
holding all of "a\*", and single-character prefixes are the most frequent requests of all.** The shard
boundaries land exactly on the hottest keys. You end up special-casing very short prefixes by replicating
them everywhere — which is an admission that replication was the right answer.

Given that 5 GB fits comfortably in a modern server's RAM, replicate. Revisit only if the corpus grows by
an order of magnitude, and note that the first response to growth should be a harder frequency filter
(§3), not sharding.

### 7.4 Freshness, and the trending exception

The daily rebuild handles the general case. Two things it does not:

**A genuinely new query — a breaking news term, a product launch — that did not exist yesterday.** Waiting
a day to suggest it is a visible product failure precisely at the moment of highest interest.

**A query whose popularity is spiking**, which should be promoted above its historical rank.

The wrong fix is to shorten the rebuild cycle, which addresses this at enormous cost to everything else.
The right fix is a **small, separate, frequently-updated overlay**:

```
suggest(prefix):
    base    = trie.top_k(prefix)              # ~1 µs, daily build
    trending = trending_index.top_k(prefix)   # small in-memory structure,
                                              # rebuilt every ~5 minutes
    return merge(trending, base)[:10]
```

The trending structure is built from a sliding window of the last hour's query stream (Chapter 71's
machinery: a Count-Min Sketch plus a min-heap, or simply an exact count over the small set of queries
seen more than N times in the window). It holds perhaps 10,000 entries, so it is small enough to rebuild
every five minutes and to hold as a plain hash map from prefix to a short list.

The merge policy is a product decision, and it should be stated as one: a trending query with a modest
absolute count should probably not displace the top historical suggestion, so promote on **rate of change
relative to baseline** rather than on raw count — which is the same distinction Chapter 71 draws between
"most frequent" and "trending".

### 7.5 Caching, and where it belongs

Prefix popularity is extraordinarily skewed — far more so than query popularity, because every long query
passes through every one of its own prefixes. The prefix "a" is requested by every user who types a query
beginning with "a", whatever it is.

```
short prefixes (1-3 chars) ≈ a large majority of all requests
distinct 1-3 char prefixes ≈ 36 + 1,296 + 46,656 ≈ 48,000
```

Forty-eight thousand entries covering the majority of traffic. That is a trivially small cache with an
extremely high hit rate.

**Cache at the edge.** Because the non-personalized response depends only on the prefix and changes daily,
it is CDN-cacheable with a TTL of minutes. This is the payoff for keeping the API unauthenticated and
parameter-free (§4): a large fraction of requests can be answered without reaching your infrastructure at
all, which is the only way the 50 ms budget is comfortable rather than tight for users far from your
datacenters.

**Cache in the client.** A user typing "rest" has already requested "r", "re", and "res". The client should
keep them; backspacing should never produce a network request.

**Do not bother with a server-side result cache.** The trie lookup is already microseconds; a cache in
front of it saves nothing and adds a failure mode. This is worth saying explicitly, because "add a Redis
cache" is a reflex, and here it is strictly worse than the thing it would cache.

### 7.6 What goes into the corpus, which is a quality problem

The suggestion list is a highly visible surface, and the build pipeline is where quality is determined.
Filters that are not optional:

- **Frequency threshold.** Discussed in §3: drop queries seen fewer than N times. Removes typos and noise,
  and cuts the corpus by an order of magnitude.
- **Profanity and safety filtering.** Suggestions are your product speaking in your voice, not the user's.
  A blocklist plus a classifier, applied at build time where it costs nothing.
- **Personally identifiable information.** People paste email addresses, phone numbers, and account
  numbers into search boxes. Suggesting them to other users is a data breach. Pattern-based filtering at
  build time, and it must be aggressive — a false positive costs one suggestion, a false negative is an
  incident.
- **Dead queries.** A query that was popular and now returns no results should not be suggested. Join
  against the search index's result counts during the build.
- **Adversarial promotion.** A sufficiently determined party can issue a query repeatedly to get it
  suggested. Count *distinct users*, not raw occurrences, and cap per-user contribution.

None of this affects the serving architecture, and all of it affects whether the feature is shippable.
Interviewers who have worked on search will ask about at least one of these, and the PII case is the one
that most cleanly separates people who have shipped a suggestion surface from people who have designed
one.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Build pipeline fails | Suggestions freeze at the last good build | Serve the previous trie indefinitely; alarm on build age, not on build failure — a failed build that nobody notices for a week is the real incident |
| Bad build (empty or corrupt trie) | Every suggestion is wrong or absent | Validate before swap: node count, sample query assertions, size within a band of the previous build; refuse the swap and alarm |
| Memory pressure during swap | Node OOMs while holding two tries | Size instances for 2.5× the trie; stagger swaps across the fleet so never more than a fraction are doubled |
| Serving node cold after restart | Page faults on first queries | Warm with a sample of recent prefixes before adding to the load balancer |
| Trending overlay stale or failed | Loses breaking-news suggestions; base suggestions unaffected | Fail open — drop the overlay from the merge; this is why it is a separate structure |
| PII appears in suggestions | Data exposure incident | Build-time filtering; a kill switch that can blocklist a specific string within minutes without a rebuild |
| Traffic spike | Stateless replicas, so simply add nodes | The edge cache absorbs most of it before it arrives |

**Monitoring:** suggestion p99 (should be single-digit milliseconds server-side, and an increase means the
trie has stopped fitting in memory); **build age**, which is the metric that catches the silent failure;
edge cache hit rate; empty-result rate by prefix length (a spike at short prefixes means a broken build);
and the fraction of searches that originated from a suggestion click, which is the product metric that
tells you whether any of this is working.

---

## 9. Common mistakes

1. **Traversing the subtree at query time.** The most common design, and it is over budget for exactly the
   short prefixes that dominate traffic.
2. **Not recognizing that the latency budget is the requirement.** Every rejected design in §6 is rejected
   on latency alone; a candidate who does not establish the budget first has nothing to reject them with.
3. **Making the trie mutable** to support incremental updates, forfeiting the compact read-only layout for
   a freshness requirement that was never stated.
4. **Prefix sharding a structure that fits in memory**, importing load skew and fan-out latency for no
   capacity benefit — and then discovering the shard boundaries sit on the hottest keys.
5. **Storing suggestion strings inline at every node**, roughly tripling memory for no reason.
6. **Forgetting the frequency filter**, indexing ten times more data than is useful and suggesting typos.
7. **Ignoring PII and profanity filtering.** This is not a nicety; it is the difference between a feature
   and an incident.
8. **Adding a server-side cache** in front of a microsecond lookup.
9. **Solving trending by shortening the rebuild cycle** rather than with a small overlay, paying full price
   across the whole corpus for a property 0.1% of it needs.

---

## 10. Variants

**Fuzzy and typo-tolerant autocomplete.** Substantially harder, and the correct interview move is to scope
it out and say why. Prefix matching over a trie has no notion of edit distance; supporting it requires
either a Levenshtein automaton intersected with the trie, or an n-gram index, or a separate correction
model applied before lookup. Each roughly changes the data structure. Naming the approach and deferring it
is a better answer than gesturing at it.

**Personalized suggestions.** Blend the global list with the user's own recent queries. Keep the global
trie shared and un-personalized so it stays cacheable, hold the per-user history client-side or in a small
per-user record, and merge on the client or in a thin server layer. **Do not personalize inside the shared
structure** — it destroys the edge caching that §7.5 depends on, which is most of the latency budget.

**Mid-string and multi-word matching.** Suggesting "new york restaurants" for the input "restaurants"
requires matching a suffix or an interior word, which a prefix trie cannot do. Standard answer: index
every suffix (or every word-boundary suffix) as its own trie entry, multiplying corpus size by the average
word count. Affordable at 10^7 queries, not at 10^9.

**Entity autocomplete** (products, people, places). Same structure, but ranking is not pure popularity —
it blends popularity with relevance to context (location, catalog availability), and the entity set changes
far faster than a query log. The daily rebuild may become hourly, and the trending overlay grows in
importance.

**Command palettes and IDE completion.** The same top-K-at-every-node structure over a much smaller corpus,
usually entirely client-side, where fuzzy subsequence matching (not prefix matching) is the expected
behavior — a genuinely different matching model on the same skeleton.

---

## 11. Further reading

- Chapter 61, for the search this fronts; Chapter 71, for the top-K machinery the build and the trending
  overlay both use; Chapter 02, §6, for edge caching
- Manning, Raghavan, and Schütze, *Introduction to Information Retrieval*, chapter 3, for tries,
  wildcard queries, and the Levenshtein automaton approach to §10's fuzzy variant
- The Apache Lucene `suggest` module documentation, for finite-state-transducer implementations that
  compress the trie substantially further than the layout described here
