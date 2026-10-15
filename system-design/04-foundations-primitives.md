# Chapter 04 — Primitives

Small, reusable mechanisms that appear across many systems. Each is short; each is the kind of thing an
interviewer asks you to produce on demand.

---

## 1. Distributed unique IDs

The requirement is usually: unique across many machines, generated without coordination, and — very
often — **roughly sortable by time**, because a sortable ID doubles as a cursor and lets you range-scan
recent items.

### Snowflake

A 64-bit integer, partitioned into fields:

```
| 1 bit unused | 41 bits timestamp (ms since epoch) | 10 bits machine id | 12 bits sequence |
```

- 41 bits of milliseconds ≈ 69 years from a chosen epoch.
- 10 bits ≈ 1,024 generator instances.
- 12 bits ≈ 4,096 IDs per millisecond per instance, i.e. ~4M IDs/second/instance.

No coordination is needed on the hot path. IDs are k-sorted: monotonic within an instance, and globally
ordered to within clock skew between instances.

**The failure modes, which are what gets asked:**

- **Clock skew between machines.** Two instances a few milliseconds apart produce interleaved IDs. This
  is fine for a cursor and not fine if you claimed strict global ordering — so don't claim it.
- **The clock moving backwards** (NTP correction, VM migration). If you generate an ID for a timestamp
  already used, you may collide. Two acceptable responses: refuse to generate until the clock catches
  up (correct, briefly unavailable), or borrow from the sequence bits and continue. Silently trusting the
  clock is the bug.
- **Machine ID assignment.** Must be unique. Static configuration, ZooKeeper ephemeral node, or derived
  from the pod ordinal in a StatefulSet. Duplicated machine IDs produce duplicate IDs, which is the worst
  possible outcome and the reason this is worth a sentence.

### Alternatives

- **UUIDv4** — 122 random bits, no coordination, no sortability, 16 bytes. Poor as a clustered primary
  key because random insertion fragments B-tree pages.
- **UUIDv7** — timestamp-prefixed and random-suffixed. Sortable, standardized, and the modern default
  when you don't need Snowflake's compactness.
- **Database auto-increment** — perfectly ordered, a single point of contention, does not shard.
- **Ticket server / segment allocation** — a central service hands out blocks of IDs (say 10,000 at a
  time) that each application instance consumes locally. No per-ID coordination, IDs are dense, and the
  ticket server is only on the path once per block. This is the pattern Chapter 10 uses for short codes.

**A note on leakage:** sequential IDs disclose your volume and let anyone enumerate your objects. If IDs
are user-visible, either randomize them or accept that competitors can count your orders.

---

## 2. Distributed locks

### The naive version and why it is wrong

```
SET lock:resource <owner> NX PX 30000     # acquire, 30s expiry
... do work ...
DEL lock:resource                          # release
```

Two bugs, both common:

1. **Releasing someone else's lock.** If your work exceeded the TTL, the lock expired and someone else
   holds it; your `DEL` releases *theirs*. Fix: store a unique owner token as the value and release with
   a compare-and-delete (a small Lua script, because the check and the delete must be atomic).
2. **Believing the lock guarantees mutual exclusion.** It does not. A process can be paused — garbage
   collection, a hypervisor stall, a network partition — past its TTL, wake up believing it holds the
   lock, and write. Two writers, both convinced they are alone.

### Fencing tokens

The correct fix for (2): the lock service issues a **monotonically increasing token** with each grant. The
resource being protected records the highest token it has seen and **rejects any write with a lower one**.
A delayed writer's stale token is refused by the storage layer.

The key insight, worth stating plainly in an interview: **a lock alone cannot make a system correct;
the resource must participate.** This generalizes to the pattern used throughout Part II's contention
chapters — a fast mechanism (lock, hold, queue) to make conflicts rare, and an authoritative constraint
in the database (unique index, version check, fencing token) to make them impossible.

### Redlock

The multi-node Redis locking algorithm. Contested in the literature — Martin Kleppmann's critique and
Salvatore Sanfilippo's response are both worth reading, and the disagreement is fundamentally about
whether you can build a correctness-grade lock on unsynchronized clocks. Safe position in an interview:
"I'd use Redis locking as an efficiency optimization and put the correctness guarantee in the database.
For a lock I actually needed to be correct, I'd use ZooKeeper or etcd, which are built on consensus."

---

## 3. Rate limiting algorithms

Five algorithms. Know the failure of each.

### Fixed window

Count requests per fixed interval; reset at the boundary.

- O(1) memory, trivial.
- **Flaw:** a client can send the full limit at the end of one window and again at the start of the next,
  admitting 2× the limit across a boundary-spanning interval. This is not theoretical; it is exactly what
  a client with a retry timer aligned to the minute does.

### Sliding window log

Store a timestamp per request; on each request, evict entries older than the window and count.

- Exactly correct, no boundary effect.
- **Flaw:** O(n) memory per client, where n is the limit. At a limit of 10,000 requests/hour across
  millions of clients this is untenable.

### Sliding window counter

Keep the current and previous fixed-window counts, and estimate:

```
count = current_count + previous_count * (fraction of previous window still in range)
```

- O(1) memory, no boundary spike, approximate (it assumes a uniform rate within the previous window).
- **The best default for API rate limiting**, and the answer to give unless something specific argues
  otherwise.

### Token bucket

Tokens accumulate at a fixed rate into a bucket of fixed capacity; each request consumes one; an empty
bucket means rejection.

- O(1) memory, and it **permits bursts** up to the bucket size while bounding the sustained rate.
- Correct when bursts are desirable — a user opening an app and firing twenty requests should not be
  throttled if their sustained rate is low.

### Leaky bucket

Requests enter a queue drained at a constant rate.

- **Smooths output completely** — no bursts reach the downstream.
- Adds latency for queued requests, and is the right choice when the thing being protected is fragile
  and cares about instantaneous rate rather than average.

### Distributed implementation

State must be shared across gateway instances, which means a race between read and write.

- Put it in **Redis**, keyed by `client + rule`.
- Perform the read-modify-write in a **Lua script**, which Redis executes atomically. This is the entire
  correctness argument; without it, concurrent gateways interleave and the limit leaks.
- Set a **TTL** on the key so idle clients cost nothing.
- **Shard by client** using consistent hashing so all of a client's requests reach the same Redis shard
  and no cross-shard coordination is needed. This is how the design reaches ~1M requests/second.

**Failure policy** is a design decision you should state: if Redis is unreachable, do you fail open (serve
the request, risk overload) or fail closed (reject, risk an outage caused by your own protection)? Fail
open is the usual answer for a public API — the rate limiter exists to protect against abuse, not to be
a single point of failure — but say it deliberately.

A **local token bucket synced periodically** with the central store is the standard latency optimization:
each gateway enforces approximately, and reconciles every few hundred milliseconds. Accepts small
overshoot in exchange for removing a network hop from every request.

---

## 4. Geospatial indexing

The problem: given millions of points, find the ones near a query point, fast. A B-tree on
`(latitude, longitude)` cannot do this — it can range-scan one dimension efficiently, not two.

### Geohash

Interleave the bits of latitude and longitude and base-32 encode the result. Each character adds
precision; **a shared prefix means physical proximity**, so proximity search becomes a string prefix
query that any ordinary index can serve.

- Simple, works in any database, human-readable.
- **Two flaws.** Cells are rectangles that vary in physical size with latitude. And points on opposite
  sides of a cell boundary can be metres apart with completely different prefixes — so a correct query
  must also examine the eight neighbouring cells.

### Quadtree

Recursively subdivide space into four quadrants, splitting a node only when it exceeds a point capacity.
Adapts to density: dense cities get deep subdivision, oceans stay shallow. Held in memory; rebuilding or
rebalancing under heavy movement is the cost.

### H3 (Uber's hexagonal hierarchical index)

Tiles the globe with **hexagons** across **16 resolutions**, each resolution's cells being about
one-seventh the area of the next coarser one, with each cell addressed by a single **64-bit index** that
encodes both location and position in the hierarchy.

The reason for hexagons, and the thing to say: **every hexagon has six neighbours, all at the same
distance from its centre.** A square grid has four edge neighbours at one distance and four corner
neighbours at another, so "the cells within one ring" is not a constant radius and proximity comparisons
are distorted. Hexagons make ring searches uniform, which is exactly what a "drivers near me" query is.

Query pattern: compute the query point's cell, take its k-ring, look up the drivers indexed in those
cells, then filter by exact distance. Resolution is chosen so a typical query needs one or two rings.

### Choosing

Geohash if you must store the index in an ordinary database and simplicity matters. Quadtree if density
varies enormously and the index lives in memory. **H3 if the workload is proximity queries over moving
objects** — which is Chapter 50. Also acceptable, and worth mentioning: PostGIS with an R-tree/GiST index,
or Elasticsearch's geo queries, when the data is static and you want it alongside other filters
(Chapter 51).

---

## 5. Probabilistic data structures

Trade exactness for a large constant-factor reduction in memory. Each has a specific error direction —
knowing which way it errs is the point.

### Bloom filter

A bit array plus k hash functions. Insert sets k bits; query checks them.

- **No false negatives.** "Not present" is certain. "Present" may be wrong, at a tunable rate.
- Cannot delete (counting Bloom filters can, at the cost of more space).
- Uses: skipping a database read for keys that certainly don't exist (Chapter 02's cache penetration);
  URL deduplication in a crawler at ten billion URLs (Chapter 60); "have I shown this user this item
  before" (Chapter 52).

Sizing: roughly 10 bits per element gives a ~1% false-positive rate. That is 1.25 GB for a billion
elements — the number that makes the crawler feasible.

### HyperLogLog

Estimates the **number of distinct elements** using the position of the leftmost set bit across hashed
values, in a fixed ~12 KB regardless of cardinality, with roughly 2% relative error. Sketches merge, so
per-shard sketches combine into a global estimate. Uses: unique visitors, unique viewers, distinct IPs.

### Count-Min Sketch

A two-dimensional array of counters with one hash function per row. Increment adds to one counter per
row; the estimate is the **minimum** across rows. Memory is fixed and independent of the number of
distinct items.

- **Over-estimates, never under-estimates**, because collisions only add. Knowing the error direction
  matters: it means "this item is below the threshold" is trustworthy and "above" may be a collision.
- Uses: frequency estimation for heavy hitters (Chapter 71), where it is paired with a size-K min-heap so
  that the "is this in the current top K" check is O(1) and eviction is O(log K).

### t-digest / DDSketch

Streaming quantile estimation with good accuracy at the tails, which is what percentile latency needs.
Mergeable across shards. This is how a metrics system (Chapter 73) reports a p99 without storing every
observation.

---

## 6. Consistent hashing

Covered in Chapter 01 as a partitioning strategy; restated here as a primitive because it recurs.

Map both nodes and keys onto a ring using the same hash function. A key belongs to the first node
clockwise from it. Adding or removing a node reassigns only the keys between it and its predecessor —
about `1/N` of the keyspace — instead of the near-total remapping that `hash(key) mod N` produces.

**Virtual nodes** are not optional in practice: each physical node is placed at many ring positions
(typically 100–200). Without them, a handful of nodes produce badly uneven arc lengths and therefore
badly uneven load, and removing a node dumps its entire share onto one successor instead of spreading it.

Appears in: distributed caches (91), key-value stores (94), rate limiter sharding (90), per-host queue
assignment in a crawler (60).

---

## 7. Observability

Not decoration. At senior level, "how would you know this is broken" is a question you should expect.

**The three signals.** Metrics (cheap, aggregate, alertable), logs (expensive, detailed, searchable),
traces (request-scoped causality across services). Use metrics to detect, traces to localize, logs to
diagnose.

**RED**, for request-driven services: **R**ate, **E**rrors, **D**uration.
**USE**, for resources: **U**tilization, **S**aturation, **E**rrors.

**Percentiles, not averages.** An average latency hides the tail entirely, and the tail is what users
experience — at a hundred requests per page load, a p99 is hit by most page loads. Also: percentiles do
not average. Averaging the p99 of ten shards does not give you the p99 of the system; you need mergeable
sketches (§5).

**Cardinality is the failure mode of metrics systems.** A label with unbounded values — user ID, URL with
an embedded identifier, request ID — multiplies the number of stored time series without limit. This is
the number one way a monitoring system falls over, and it is Chapter 73's central deep dive.

**Alert on symptoms, not causes.** Alert on "checkout error rate above 1%", not "CPU above 80%". Users
experience symptoms; causes are what you find afterwards, and most cause-based alerts fire when nothing
is wrong.

---

## Further reading

- [Uber Engineering — H3: A Hexagonal Hierarchical Spatial Index](https://www.uber.com/us/en/blog/h3/) · [uber/h3](https://github.com/uber/h3)
- [Redis — Rate limiting patterns](https://redis.io/tutorials/howtos/ratelimiting/)
- [Redis — Top-K (HeavyKeepers implementation)](https://redis.io/docs/latest/develop/data-types/probabilistic/top-k/)
- Martin Kleppmann, "How to do distributed locking" — and Salvatore Sanfilippo's reply, for the Redlock
  debate in the participants' own words
- Twitter's Snowflake announcement, for the original bit layout and its rationale
