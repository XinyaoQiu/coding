# Chapter 60 — Web Crawler

> **Prerequisites:** Chapters 01 (partitioning, LSM engines), 03 (queues, at-least-once),
> 04 (§5 Bloom filters and sizing, §6 consistent hashing)
> **Patterns:** two-layer priority/politeness queue, probabilistic membership, near-duplicate detection,
> adaptive scheduling, host-local invariants

---

## 1. The problem

Fetch a large fraction of the public web, repeatedly, and hand what you fetch to something downstream — a
search index (Chapter 61), a training corpus, a price monitor, an archive. Start from seeds, extract links
from every page, follow them, do not stop.

Described that way it is breadth-first search with an HTTP client attached, which is the trap: BFS with a
work queue is a correct crawler for a thousand pages and a catastrophic one for ten billion.

**The property that makes it hard:** throughput and politeness are constraints over two different keys.
Throughput is a global rate — pages per second across the whole crawl. Politeness is a per-host rate — at
most one request per second to any origin, because exceeding it is indistinguishable from an attack and
gets you blocked. A single ordering cannot express both. And almost every other hard sub-problem —
priority, freshness, traps, deduplication, distribution across machines — turns out to be a constraint on
the same structure: the **URL frontier**, the component that decides what to fetch next.

Get the frontier right and the rest is a fetch loop, an HTML parser, and a blob store. Get it wrong and no
amount of bandwidth helps. **Almost every hard part of a crawler lives in the frontier**, and this chapter
is organized around that claim.

---

## 2. Requirements

### Functional

1. Crawl from a seed set, extract outlinks, follow them.
2. Respect `robots.txt` and per-host rate limits.
3. Store fetched content plus enough metadata to serve a downstream indexer.
4. Re-crawl pages so the corpus stays current, and avoid fetching the same thing twice — by URL and,
   separately, by content.

### Non-functional

- **Throughput** — sustain the rate implied by corpus size and crawl period (§3 derives ~3,900
  pages/second). Everything is measured against this.
- **Politeness** — one request per second per host by default, longer when `Crawl-delay` asks. A hard
  correctness constraint, not a tuning knob: violating it removes hosts from the crawlable web permanently.
- **Robustness** — the web is adversarial and broken in equal measure. Malformed HTML, redirect loops,
  10 GB responses, servers that accept a connection and never send a byte, and deliberately infinite URL
  spaces are normal traffic, not exceptions.
- **Freshness** — a page's copy should be no older than its own rate of change warrants; **coverage** is
  approximate, since missing 1% of the web is normal and pretending otherwise over-constrains the design.

### Explicitly out of scope

Rendering JavaScript (a headless browser costs roughly 100× a raw fetch in CPU and is a separate
pipeline), authenticated content, ranking, and the search index itself.

---

## 3. Estimation

Target: 10 billion pages, fully re-crawled monthly.

**Throughput**

```
1e10 pages / (30 × 86,400 s)   ≈ 3,860 pages/sec sustained
peak (2×)                      ≈ 7,700 pages/sec
```

**Bandwidth and storage**

```
average page, HTML only   ≈ 500 KB uncompressed, ≈ 120 KB on the wire (gzip)
3,860 × 120 KB            ≈ 460 MB/s ≈ 3.7 Gbps sustained inbound
raw HTML, one crawl       1e10 × 120 KB = 1.2 PB; three generations ≈ 3.6 PB
extracted text only       1e10 × 5 KB   = 50 TB
```

Under 5 Gbps is unremarkable for a datacenter — bandwidth is not the constraint. The gap between 1.2 PB
and 50 TB is the argument for separating raw archival storage from the extracted text the indexer reads.

**URL volume**

```
outlinks per page       ≈ 60
1e10 × 60               = 6e11 URL sightings per crawl
6e11 / 2.592e6 s        ≈ 231,000 "have I seen this?" lookups/sec
distinct URLs ever seen ≈ 2e10 (most sightings repeat the same links)
```

231,000 membership lookups per second against twenty billion strings. An exact key-value store would hold
`2e10 × 80 B ≈ 1.6 TB` and, at typical per-request pricing, cost six figures per crawl to answer a yes/no
question. That forces a probabilistic structure (§7.2).

**Politeness converts throughput into host parallelism — THE constraining number**

```
politeness:  ≤ 1 request/sec/host
to sustain 3,860 pages/sec you need ≥ 3,860 distinct hosts in flight
at a 2 s crawl-delay:               ≥ 7,700 hosts
```

Read that again, because the design serves it. **Crawler throughput is not bought with machines or
bandwidth; it is bought with host diversity.** A crawler whose frontier has drained to five hundred
non-empty hosts runs at five hundred pages per second on a thousand machines. The frontier's job is not
merely to hold URLs but to *maintain tens of thousands of simultaneously ready, distinct hosts at all
times*. That is a scheduling requirement, and it is why the frontier looks the way it does.

Finally, the frontier itself holds 1e9–1e10 pending URLs at ~100 B each — up to a terabyte, which does not
sit in memory, so back queues are disk-backed with in-memory heads.

---

## 4. API

A crawler is internal machinery, so the interesting interfaces are between its own components.

```
POST /frontier/urls      {urls: [{url, priority, depth, sourceUrl}]}          -> 202
POST /frontier/lease     {workerId, count}
  -> 200 {leases: [{leaseId, url, host, ip, crawlDelayMs, leaseExpiresAt}]}
POST /frontier/complete  {leaseId, httpStatus, contentHash, changed, discoveredUrls} -> 204

GET  /robots/{host}      -> 200 {allow[], disallow[], crawlDelayMs, sitemaps[], ttl}

PUT  /content/{contentHash}   (body: raw bytes)  -> 201
GET  /pages/{urlHash}    -> 200 {url, contentHash, simhash, httpStatus, fetchedAt, etag}
```

**Lease, not pop.** A worker that pops a URL and dies takes it with it; a lease with an expiry lets the
frontier re-offer unacknowledged work — at-least-once (Chapter 03), exactly the right guarantee here,
since its failure mode is fetching a page twice, wasting a politeness slot but corrupting nothing. **The
lease is also the politeness mechanism:** the frontier issues at most one outstanding lease per host and
withholds the next until `crawlDelayMs` after the previous completes, so politeness is enforced by
*withholding work* and a buggy fetcher cannot violate it — it is never handed the means to.

**`complete` carries `discoveredUrls`,** so acknowledging the fetch and submitting its links is atomic:
either the page counts as fetched and its links are queued, or the lease expires and both happen again.

---

## 5. Data model

```
host_state                                   # the politeness ledger, one row per host
  host              TEXT       partition key
  ip, next_fetch_at, crawl_delay_ms
  robots_fetched_at, robots_rules
  pages_crawled, budget                      # the per-domain cap (§7.4)
  error_streak, host_score

pages                                        # one row per URL ever fetched
  url_hash          BYTES(16)  partition key
  url, content_ref (archive_id, offset, length)
  content_hash      BYTES(16)
  simhash           BIGINT
  http_status, etag, last_modified
  fetched_at, prev_fetched_at
  change_count, next_recrawl_at              # drives adaptive recrawl (§7.6)
```

`pages` is 2e10 rows at ~200 B ≈ 4 TB, written at 3,860 rows/second and read only by exact key: a
write-heavy, point-lookup, no-join workload, so a wide-column store on an LSM engine (Chapter 01 §1),
partitioned by `url_hash`. `host_state` is the hottest read-modify-write path in the system — every fetch
touches it — so its working set lives in memory on the frontier nodes and persists asynchronously; it is
derived state, and losing it costs a `robots.txt` re-fetch, not data.

Raw content goes to object storage, but *not* one page per object: ten billion 120 KB objects will
bankrupt you on per-request cost and destroy the store's metadata layer. Pages are batched into ~1 GB
WARC-style archive files, and `content_ref` points into one — 1.2 million objects instead of ten billion.

---

## 6. Architecture, derived

### Attempt 1: one FIFO queue and a worker pool

Textbook BFS: seeds go in, workers pop, fetch, parse, push the outlinks.

It breaks on politeness immediately. A page yields ~60 outlinks and the overwhelming majority point at the
same host — internal navigation, not external references. The queue is host-clustered by construction:
contiguous runs of hundreds of URLs for one host. With 500 workers pulling from the head you issue 500
concurrent requests to one origin server. That is a denial of service, and you will be blocked within
minutes by exactly the operators who notice fastest. It also has no notion of importance: a calendar
widget eight links deep fills the queue with `?month=2031-07` and the crawler works through it forever.

### Attempt 2: one queue per host, round-robin

Politeness is now structural: take one URL from a host's queue, wait the delay, take the next.

**Priority disappears.** Round-robin spends the same budget on a parked domain with four pages as on a
news site with four million, with no way to say "fetch this front page hourly and this forum's
printer-friendly views never." **And the rotation is the wrong structure:** almost all of the ~2×10^8
domains' queues are empty at any instant, so round-robin does work proportional to the number of hosts
rather than *ready* hosts. The question the scheduler needs answered is "which host is due next," which is
a min-heap keyed by time.

### Attempt 3: one priority queue, skipping hosts that are busy

The tempting single-structure repair, and wrong in an instructive way. Order all pending URLs by
importance; to fetch, pop the head, and if its host is not due, set it aside and try the next.

Now count the work. Priority and host-readiness are not merely uncorrelated, they are adversarially
correlated: high-priority URLs cluster on high-value hosts, because a site with a high link score has
thousands of high-scoring pages. The head of the ordering is dominated by a handful of hosts, and those
are precisely the hosts that are busy — you just fetched from them. Expected scan depth is proportional to
the number of queued URLs belonging to currently-busy hosts, which at the head is nearly all of them. You
have turned an `O(log n)` pop into an `O(n)` scan and you run it 3,860 times per second.

The general statement carries to other chapters: **a single ordering cannot satisfy two constraints
defined over different keys.**

### Attempt 4: two layers — the answer

Split the requirements into two layers with a router between them.

**Front queues express priority and nothing else.** There are `f` of them (say 10), one per priority band.
A prioritizer assigns each URL a band from host score, depth, observed change rate, and whether this is
discovery or re-crawl. A **biased selector** picks the next front queue — band 1 chosen roughly ten times
as often as band 10. This is the only place importance is expressed, and its only effect is *when a URL
enters the back layer*.

**Back queues express politeness and nothing else.** Each holds URLs for exactly one host. Beside them
sits a min-heap of `(next_fetch_at, back_queue_id)`. A worker pops the earliest-due entry, sleeps if it is
in the future, takes that queue's head URL, fetches it, and pushes the queue back at `now + crawl_delay`.
Exactly one worker holds a back queue at a time, so exactly one request is in flight per host — the
politeness invariant as a property of a data structure rather than a rule someone must remember.

**The router keeps the back layer full.** When a back queue drains, the router pulls from the front queues
to refill it, maintaining a `host → back_queue` table so all of a host's URLs land in one place; the
invariant it protects is §3's. Routing is **consistent hashing on the host**, not on the URL — that is
what makes the design distributable (§7.1), and hashing the full URL scatters a host across every queue
and destroys the politeness invariant.

### Attempt 5: everything else the web does to you

Bolt on what exists only because the web is hostile: a **URL seen-set** ahead of the prioritizer (§7.2), a
**content seen-set** behind the fetcher (§7.3), **normalization and per-domain budgets** (§7.4), a **DNS
cache** (§7.5), and a **recrawl scheduler** feeding the front queues like any other source (§7.6).

### Final architecture

```
  seeds ──►┌─────────────┐◄── URL seen-set (Bloom, 16 shards) ◄── discovered URLs
           │ Prioritizer │
           └──────┬──────┘
                  ▼
  front queues  F1 F2 ... F10                    (priority; biased selection)
                  │  router: consistent-hash(host)
                  ▼
  ┌──────────── frontier node (one of N) ───────────┐
  │ host → back queue table                         │
  │ back queues B1 B2 ... Bk  (one host each, disk) │
  │ due-time min-heap (next_fetch_at, queue_id)     │
  │ host_state: delay, budget, robots, error streak │
  └───────────────────────┬─────────────────────────┘
                          ▼  lease: one per host at a time
   Fetchers (async I/O, ~15k conns; DNS cache, robots cache)
                          ▼
   content hash + simhash ──► WARC batches ──► object storage
                          ▼
   Parser ──► pages (wide-column) ──► Recrawl scheduler ──► front queues
        └──► outlinks ──► URL seen-set ──► Prioritizer
```

---

## 7. Deep dives

### 7.1 Sizing and distributing the frontier

**How many back queues.** From §3 the total `k` must exceed 3,860 at a one-second delay, and since hosts
are not uniformly available a working figure is 5–10× that, `k ≈ 30,000` — a due-time heap over which has
a 15-step pop, against 200 million mostly-empty entries for one queue per host.

**How long each back queue may be** is the coupling between the layers, and a real trade. Too short and
the router becomes a synchronous dependency of every fetch, reintroducing Attempt 3's scan. Too long and
priority is stale by the time a URL is fetched — a URL marked urgent sits behind a thousand ordinary URLs
for the same host, seventeen minutes at one request per second. Bound back queues at a few hundred entries
and give urgent re-crawls a bypass inserting at the *head* of a host's queue; without it, "crawl this page
now" cannot be expressed at all.

**Partition by host.** All URLs for `example.com` live on exactly one frontier node, and the consequence
is the point: **the politeness invariant becomes a single-node data structure.** No distributed lock, no
lease service, no consensus — "at most one outstanding fetch per host" is a local map and a local heap at
memory speed. Any other partitioning (URL hash, priority, round-robin) puts a coordination service on the
hot path of 3,860 fetches per second for no benefit. The transferable lesson: **partition on the key your
invariant is defined over, and the invariant stops being distributed.**

Use consistent hashing rather than `hash(host) mod N`, because modulo remaps nearly every host when a node
is added while consistent hashing moves about `1/N` (Chapter 04 §6), and virtual nodes are not optional
since host workload is heavily skewed. What must not happen when a node dies is two nodes believing they
own one host, doubling its request rate, so hold ownership as a short coordination-service lease and stop
serving a host the moment it lapses. Unfetched pages are recoverable; a blocked host is not.

### 7.2 URL deduplication at twenty billion

Every discovered link is tested against the set of URLs ever seen: 231,000 lookups per second against
2×10^10 members. An **exact key-value store** holds 1.6 TB of keys and puts a network round trip on the
crawler's hottest path — correct, expensive, a remote dependency where you least want one. **Batch
sort-merge** uses sequential I/O only and is very throughput-efficient, at the cost of latency. The
recommended answer is a **sharded Bloom filter**: shard by `hash(url)` across 16 nodes, each holding a bit
array for its slice. Sizing from Chapter 04 §5, with `FPR ≈ 0.6185^(m/n)`:

```
m/n = 10  →  FPR ≈ 0.8%    →  2e10 × 10 bits = 25 GB   (1.6 GB per shard)
m/n = 16  →  FPR ≈ 0.05%   →  2e10 × 16 bits = 40 GB   (2.5 GB per shard)
```

Roughly **1.25 GB per billion URLs** at the 1% setting — the number that makes this feasible at all, and
both configurations are trivially memory-resident. The optimal hash count is `k ≈ 0.693 × m/n`, so 7 at
ten bits; each hash is an independent random access into a multi-gigabyte array, so `k` is a latency knob
as much as an accuracy one.

**The error direction is the whole justification.** A Bloom filter has no false negatives: it never
reports "new" for a URL already crawled, so the crawler cannot be made to re-crawl by filter error and
cannot be made to loop. It reports "seen" for an unseen URL at rate `p`, and that page is then silently
never fetched — at `m/n = 10`, about 160 million of them. That is the tolerable direction. Coverage is a
soft target: the web is redundant and no crawler is complete. The opposite error would consume the
genuinely scarce resource, which is not storage or compute but *host politeness budget* — each host grants
roughly 86,400 fetches a day and no more, so wasting a slot re-fetching a page you have is strictly worse
than never fetching one you never knew about. And the loss is purchasable: 15 GB more memory takes it to
9 million pages.

**Checkpoint every shard, or the filter is unoperable.** Its contents exist only as bits in RAM, so a
restart leaves it empty, and an empty filter reports "new" for everything — a full re-crawl of the corpus,
1.2 PB and thirty days of work, triggered by a rolling deploy. Rebuilding from `pages` instead means
scanning 4 TB. So snapshot each shard to object storage every ten minutes (4 MB/s) and append inserted
URLs to a log in between; restart loads the snapshot and replays the log. This is an LSM-tree's memtable
plus write-ahead log (Chapter 01 §1) applied elsewhere: an in-memory index is usable in production only if
it has a cheap path back from empty. One consequence people miss is that the filter answers "have I ever
seen this URL," not "should I fetch it now," and cannot delete — so re-crawling *must* be driven by a
separate scheduler reading `pages.next_recrawl_at`.

### 7.3 Content deduplication is a different problem

URL dedup asks "have I fetched this address." Content dedup asks "have I already got these bytes." The web
serves identical content under many URLs constantly — session identifiers, tracking parameters, `www` and
bare-domain variants, print views, mobile subdomains, mirrors, articles syndicated to fifty outlets — and
URL dedup catches none of it, because the URLs genuinely differ. **Exact duplicates** fall to a 128-bit
hash of the normalized body: the index is `2e10 × 16 B = 320 GB`, sharded, fronted by its own Bloom filter
so the common "definitely new" case is free.

**Near-duplicates need simhash.** Two copies of an article differing only in an advertisement or a
timestamp produce completely unrelated cryptographic hashes — that is what a cryptographic hash is *for*,
and it is exactly the wrong property here. You need a fingerprint where similar inputs produce similar
outputs. Simhash does that: tokenize into weighted features (word shingles), hash each to 64 bits, and for
each of the 64 positions sum `+weight` where the feature's bit is 1 and `−weight` where it is 0; the sign
of each sum is that bit of the fingerprint. Documents sharing most weighted features agree in most bit
positions, so near-duplicates land within a small Hamming distance — empirically 3 bits out of 64 is a
good threshold on web content.

Finding whether any of 2×10^10 stored fingerprints lies within distance 3 is the hard part; brute force is
2×10^10 comparisons per page. The standard construction uses the pigeonhole principle: split the 64 bits
into `b > 3` blocks, and two fingerprints differing in at most 3 bits must agree exactly on at least one
block. Keep `b` copies of the table, each sorted by a different block permuted to the front, probe each by
exact prefix, then check the candidates by full Hamming distance. More tables mean more memory and more
probes but shorter candidate lists; at four, storage is 640 GB. **MinHash with LSH** is the alternative,
better when you need an actual Jaccard estimate; simhash's single fingerprint suffices for a yes/no
verdict at a fixed threshold.

**What it buys, and what it cannot.** Content dedup runs *after* the fetch, so it cannot save bandwidth —
that asymmetry is exactly why it is separate from URL dedup, a pre-fetch filter that saves the fetch
itself. What it buys is storage, index size, and downstream quality: an index holding forty copies of one
article ranks badly and costs forty times what it should. The cost: simhash false positives suppress
genuinely distinct pages sharing most of their tokens, like two product pages differing only in a model
number — bound the damage by requiring candidates to share a host.

### 7.4 Traps, normalization, and budgets

Large parts of the URL space are infinite, and some of it is infinite on purpose. An **infinite calendar**
generates an unbounded chain of distinct, valid, useless URLs from a "next month" link, and URL dedup does
not help because every URL genuinely is new. **Session identifiers** (`;jsessionid=`, `?PHPSESSID=`, and
hundreds of tracking parameters) make the same page appear under a new URL on every visit, so you pay for
the fetch before discovering it was worthless. **Unbounded path depth** comes from a relative-link bug
producing `/a/b/a/b/…` forever, and **faceted navigation** with 10 filters of 5 values each exposes
`5^10 ≈ 10^7` URLs over a thousand products; nobody wrote those pages, the combinatorics did.

Mitigations, in increasing order of how much they buy:

- **URL normalization**, applied *before* the seen-set lookup so it collapses variants rather than merely
  labeling them: lowercase scheme and host, strip the default port and fragment, resolve `.` and `..`,
  sort query parameters, strip known session and tracking parameters.
- **A learned per-host parameter policy** — track whether varying a parameter ever changes a page's
  simhash on that host, and if it never has, strip it there. This catches host-specific session-ids.
- **Depth limits.** Cap link depth from the seed at ~20 — blunt, and it converts an infinite space into a
  finite one with a single integer.
- **Per-domain page budgets**, the sharpest tool: a cap proportional to host score, a hundred pages for an
  unknown domain and ten million for a major one. This bounds *every* trap at once, including ones nobody
  has enumerated, because a trap is by definition a way to consume unbounded budget on one domain. Add
  **yield-based decay** — if a domain's last thousand pages produced under 5% new content hashes, halve
  its budget — and you catch the traps you have not thought of.

**robots.txt** is a separate fetch per host, cached ~24 hours, and two details matter more than they
sound. A host with *no* `robots.txt` returns 404, and failing to negative-cache that means re-requesting
it before every URL from that host — a 100% overhead on exactly the hosts that never asked you for
anything. And a `robots.txt` fetch failing with 5xx should mean "disallow temporarily," not "allow."

### 7.5 DNS is a real bottleneck, and the fix is scheduling

A cold recursive resolution takes 50–200 ms and may need several round trips through the delegation chain
— comparable to the fetch itself, so resolving before each fetch roughly doubles per-URL latency and
halves per-worker throughput. Worse, the standard `getaddrinfo` interface is synchronous, and in many
runtimes resolutions serialize through a small thread pool, making DNS a global bottleneck invisible in
application metrics.

**Cache by host.** Host locality is extreme, so hit rates above 95% come free, and an LRU over the two
million active hosts at ~100 B each is 200 MB. Honor the record TTL but clamp it to a floor of ~60 s and a
ceiling of ~1 hour: many CDN records carry browser-oriented 30–60 second TTLs, and respecting those
literally means re-resolving constantly for no benefit at crawler timescales. The ceiling is a genuine
correctness trade — after a site migrates you may fetch a stale address — so pair it with a rule that an
error streak on a host flushes its DNS entry first.

**Resolve ahead of time — the actual fix.** The frontier knows minutes in advance which hosts will become
due, because the due-time heap literally *is* that schedule. Resolve a host when it is routed into the
back layer, not when a fetcher is about to use it: that takes DNS off the critical path entirely and turns
a latency problem into a background throughput one. It is available only because the frontier is a
scheduler rather than a queue — one more argument for §6. Run your own recursive resolvers in-datacenter,
with an asynchronous client so a pending resolution holds a file descriptor rather than a thread.

**The fetcher's connection budget** follows from Little's Law:

```
concurrency = throughput × mean latency
3,860 pages/s × 2 s mean fetch          ≈ 7,700 concurrent connections
hung fetches: 2% × 3,860 × 30 s timeout ≈ 2,300 sockets held by nothing
budget                                  ≈ 15,000 concurrent sockets
```

Over 30 fetcher machines that is 500 each: nothing for an event-driven client, expensive for
thread-per-connection. Three limits bite in production — file descriptors, TLS handshake CPU (3,860
handshakes per second at ~1 ms each is four cores of cryptography), and response size, since some of the
web will happily stream you gigabytes, so cap the body and abort.

### 7.6 Freshness: recrawl adaptively, not on a schedule

A crawled page decays from the moment it is stored, and the naive fixed interval does not survive contact
with the arithmetic: `1e10 / (7 × 86,400) ≈ 16,500 pages/sec` for a weekly recrawl of everything, four
times the discovery budget from §3, spent overwhelmingly on pages that did not change.

Pages change at wildly different rates, and the rate is observable. Model changes as a Poisson process of
rate `λ`, estimate `λ` from history you already store (`change_count` over the observation window), and
set the recrawl interval proportional to `1/λ`, clamped to roughly one hour to thirty days. Suppose the
corpus resolves to 1% daily-changing, 10% weekly, 89% monthly:

```
0.01 × 1e10 / 86,400     ≈ 1,157 pages/sec
0.10 × 1e10 / 604,800    ≈ 1,653 pages/sec
0.89 × 1e10 / 2,592,000  ≈ 3,434 pages/sec
                   total ≈ 6,244 pages/sec
```

2.6× cheaper than the fixed schedule, *and* the volatile pages are refreshed daily rather than weekly —
better on both axes, which is the shape of a good adaptive policy: not a compromise, a reallocation.
**Conditional requests make it cheaper still**: with `If-Modified-Since` and `If-None-Match` an unchanged
page returns `304` with essentially no body, so a wasted recrawl costs a politeness slot and a few hundred
bytes instead of 120 KB. Bandwidth stops being the reason to recrawl less, and the per-host request budget
becomes the binding constraint — which is what the frontier already manages. Finally, weight by importance
and not only by change rate: the objective is *expected staleness weighted by importance*, which is why
recrawls flow through the same prioritizer as discovery rather than a parallel path. There is one budget,
and both kinds of work must compete for it.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Frontier node loss | Its hosts stop being crawled; pending URLs unavailable | Consistent hashing moves ~1/N of hosts; disk-backed queues recover; ownership lease so no two nodes own a host |
| Bloom shard restarts empty | Everything looks new; full corpus re-crawl | Snapshot + insert-log replay; alert on a spike in the "new URL" rate |
| DNS resolver saturation | Latency doubles, throughput halves, no error surfaces | In-DC resolvers, host-keyed cache with clamped TTL, resolve at routing time |
| A host starts erroring | Wasted budget, load on a struggling server | Error-streak counter with exponential backoff on `next_fetch_at` |
| Trap generating unbounded URLs | One domain consumes the crawl | Per-domain budget, yield decay, depth limit, normalization |
| Slowloris / endless response | Sockets held indefinitely | Connect, read, and total-request timeouts; body-size cap |
| Back queues drain | Throughput collapses while machines look idle | Alert on *count of non-empty back queues* against the §3 requirement |
| Duplicate lease after expiry | A page fetched twice | Harmless by design; at-least-once is the chosen semantics |
| Content store hot-spotting | Object store throttles writes | Batch into large WARC files; key archives randomly, not by timestamp |

The back-queue row is the one to internalize. In most systems the alarming metric is a queue growing; in a
crawler it is the *number of distinct ready hosts* shrinking, because §3 showed throughput is proportional
to it. A crawler can sit at 20% of target with every machine at 10% CPU, every queue deep, and no errors
anywhere — because all the pending work belongs to four hundred hosts.

**Monitoring:** pages/second against target, count of non-empty back queues, lease-to-complete latency,
DNS cache hit rate and resolution p99, fetch status distribution, seen-set insertion rate (a spike means a
trap opened; a collapse means the filter is over-full), near-duplicate rate per host, and per-domain
budget consumption.

---

## 9. Common mistakes

1. **Presenting the frontier as a queue.** It is a scheduler with two indexes and a due-time heap; drawing
   one box labeled "queue" skips the question, because every hard requirement lands on that box.
2. **Politeness as a per-worker sleep.** Sleeping after a fetch throttles the *worker*, not the *host*;
   with 500 workers a host still receives 500 concurrent requests. Politeness must be a property of the
   structure that hands out work.
3. **Consistent hashing on the URL instead of the host.** Scatters a host across every node and makes the
   politeness invariant distributed for no reason.
4. **Assuming URL deduplication also handles duplicate content.** Session IDs and mirrors defeat it
   completely; the fix is a different structure at a different point in the pipeline.
5. **Reaching for exact hashing for near-duplicates.** A cryptographic hash is designed so similar inputs
   produce dissimilar outputs, which is precisely wrong here.
6. **Not checkpointing the Bloom filter**, so a routine deploy triggers a full re-crawl of the corpus.
7. **Stating the Bloom filter's error rate without its direction.** The design is defensible only because
   false positives skip pages and false negatives cannot occur; that sentence is the argument.
8. **Ignoring DNS**, then being unable to explain why fetchers sit at 15% CPU and the crawl at 30% of
   target.
9. **A fixed recrawl schedule**, spending most of the budget confirming that pages did not change.

---

## 10. Variants

**Focused / vertical crawler.** Crawl only pages matching a topic or domain list. The frontier is
unchanged; the prioritizer gains a classifier scoring a URL's likely relevance from anchor text and the
source page's topic. Volume drops and coverage requirements rise, which inverts §7.2's trade — at a
hundred million URLs an exact seen-set is affordable and you should prefer it, because losing 1% of a
focused crawl is a product defect rather than rounding.

**News crawler.** Freshness dominates and the corpus is small, so the recrawl scheduler becomes the
primary component and RSS and sitemap `lastmod` replace much of link discovery. Politeness gets *harder*,
because the hosts you want are few and you want them constantly.

**Internal / enterprise crawler.** No politeness constraint worth the name, no adversarial traps, and
authenticated access. The frontier collapses to a priority queue — the design rejected in Attempt 3, and
correct here, which demonstrates that the two-layer structure exists to serve one constraint and nothing
else.

**Crawling for a training corpus.** Content dedup and quality filtering dominate; near-duplicate detection
becomes the main event, since duplicated documents in a training set are actively harmful.

---

## 11. Further reading

- Chapter 04 §5 for Bloom filter sizing and §6 for consistent hashing; Chapter 61 for what consumes the
  crawler's output
- Allan Heydon and Marc Najork, "Mercator: A Scalable, Extensible Web Crawler" (World Wide Web, 1999) —
  the origin of the two-layer frontier
- Gurmeet Singh Manku, Arvind Jain, Anish Das Sarma, "Detecting Near-Duplicates for Web Crawling"
  (WWW 2007) — simhash and the permuted-table Hamming search
- Junghoo Cho and Hector Garcia-Molina, "Effective Page Refresh Policies for Web Crawlers" (ACM TODS,
  2003) — the Poisson change model behind adaptive recrawl
- Hsin-Tsang Lee, Derek Leonard, Xiaoming Wang, Dmitri Loguinov, "IRLbot: Scaling to 6 Billion Pages and
  Beyond" (WWW 2008) — budget enforcement and trap resistance at scale
- Sergey Brin and Lawrence Page, "The Anatomy of a Large-Scale Hypertextual Web Search Engine" (WWW 1998),
  §4 — the original crawler and its DNS problem; RFC 9309, "Robots Exclusion Protocol"; Manning, Raghavan,
  and Schütze, *Introduction to Information Retrieval*, Chapter 20
