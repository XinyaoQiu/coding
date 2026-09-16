# Chapter 91 — Distributed Cache

> **Prerequisites:** Chapters 01 (partitioning, replication), 02 (cache patterns, the three failure modes, eviction), 04 §5 (Count-Min Sketch), 04 §6 (consistent hashing with virtual nodes)
> **Patterns:** key placement without a directory, O(1) eviction, approximate algorithms in the data plane, derived state

---

## 1. The problem

Build the thing every other chapter reaches for. A fleet of application servers needs a shared, in-memory
key-value store that answers `GET` in well under a millisecond, holds far more than one machine can, evicts
sensibly when full, and survives a node dying without taking the origin down with it. You are designing
Redis or Memcached, not calling one.

Chapter 02 covers how to *use* a cache. This chapter is what is underneath: how a client finds the node
holding a key without asking anyone, what structures make eviction constant-time, why the memory you
bought is not the memory you get, and what replication buys when the data is disposable by definition.

**The property that makes it hard:** a cache has almost no correctness requirements and absolute
performance requirements, and those two facts pull the design in the same direction until the membership
changes. It may lose every byte it holds — the source of truth still has them. But it must be an order of
magnitude faster than that source of truth at every percentile, and it must stay that way while nodes are
added, removed, and lost. **A cache that redistributes its keyspace has not lost data; it has turned
itself into a load generator aimed at the database it was protecting.** Every structural decision here
exists to keep the fraction of keys that move small.

---

## 2. Requirements

### Functional

1. `GET`, `SET` with a required TTL, `DELETE`, and `MGET` for batched reads.
2. `INCR` and compare-and-swap, because a cache that can only get and set forces every counter into the
   database.
3. Evict when memory is full, by an approximation of least-recently-used.
4. Add and remove nodes without downtime and without invalidating the whole keyspace.

### Non-functional

- **Latency** — p99 under 1 ms server-side, p99 under 2 ms as seen by the client. If the cache's p99
  approaches the database's p50 it has stopped being a cache.
- **Throughput** — 1,000,000 operations/second across the cluster.
- **Capacity** — a working set of roughly 500 GB, well beyond one machine.
- **Availability** — losing one node degrades the hit rate; it does not fail requests. The client falls
  through to the origin, so the cache is a performance dependency, never a correctness one (Chapter 02 §2).
- **Durability — none, deliberately.** Everything here is derived. Say it early: it licenses storing in
  RAM, skipping the write-ahead log, and treating a node loss as a warm-up rather than an incident.

### Explicitly out of scope

Durability, cross-key transactions, secondary indexes, range and prefix scans, and being anyone's source
of truth. Each, if added, forces a different design: a cache that must not lose data is a database with
extra steps.

---

## 3. Estimation

Assume one million cache operations per second, 90% reads, average value 1 KB.
Network first, because it is the cheapest way to rule out a design, then CPU. A single-threaded in-memory
server sustains roughly 100,000–200,000 simple operations per second; the limit is per-request syscall and
protocol overhead, not the hash lookup, which costs tens of nanoseconds.

```
1,000,000 ops/s × 1 KB  = 1 GB/s = 8 Gbit/s aggregate
  over 10 nodes         = 0.8 Gbit/s each — comfortable on 10 GbE
1,000,000 ÷ 150,000/node ≈ 7 nodes for throughput
```

Memory is what actually sizes the cluster:

```
500,000,000 keys × (40 B key + 1,024 B value + ~80 B entry overhead) ≈ 570 GB of live data
fragmentation and allocator overhead at 1.3×                          ≈ 740 GB
÷ 64 GB usable per node                                               ≈ 12 nodes
```

Twelve nodes, not seven: **capacity binds before throughput**, which is typical and worth saying, because
it means headroom costs RAM rather than CPU. The 1.3× factor is not a fudge — §7.5 derives it, and it is
the difference between 20% headroom and being killed by the kernel while your metrics report free space.

**The constraining number: what a membership change costs.** Suppose ten nodes become eleven.

```
hash(key) mod N:      a key stays put only if hash % 10 == hash % 11
                      → roughly 1/11 stay, 10/11 ≈ 91% of keys move
consistent hashing:   only the arc claimed by the new node moves
                      → 1/11 ≈ 9% of keys move
```

At a 95% hit rate the origin normally sees 50,000 requests per second. Immediately after a `mod N` resize
it sees `0.91 × 1,000,000 = 910,000` per second — an 18× spike, sustained until the cache refills.

That is not a cache miss; that is an outage of the database caused by an operator adding capacity. With
consistent hashing the same event produces 90,000 requests per second — under twice steady state, absorbed
without incident. **A factor of ten between "we added a node" and "we took the site down" is the entire
justification for §7.1**, and it should be derived, not asserted.

---

## 4. API

```
GET    key                     -> value | MISS
MGET   key[]                   -> {key: value}          # batching primitive
SET    key value ttl [flags]   -> OK
CAS    key value ttl version   -> OK | CONFLICT         # version from a prior GET
DELETE key                     -> OK                     # idempotent
INCR   key delta ttl           -> newValue
```

**`MGET` is not a convenience, it is the throughput story on the client side.** Hydrating fifty items one
at a time costs fifty round trips — 15 ms of pure waiting at a 0.3 ms RTT, larger than the whole page
budget in most of Part II. Batched, it is one round trip per participating node, so fifty keys over twelve
nodes is twelve parallel calls finishing in roughly the latency of the slowest. Every chapter that
hydrates IDs into bodies (20 §7.2, 22, 61) depends on this operation existing.

**TTL is mandatory, not optional.** Requiring it rather than defaulting it forces the caller to answer
"how stale may this be?", the question Chapter 02 §5 says determines the TTL. An API permitting `SET`
without expiry accumulates keys nobody remembers writing, and the eviction policy then quietly decides
your retention for you.

**`CAS` earns its place** because without it the read-modify-write race of Chapter 02 §2 has no fix inside
the cache. The version is an opaque counter returned by `GET` and bumped on every write; a `CAS` whose
version no longer matches fails and the caller retries — the one concession to concurrency control in an
otherwise fire-and-forget interface.

**No `SCAN`, no `KEYS`, no wildcards.** A scan over a hundred million keys blocks a single-threaded server
for seconds — an outage delivered by a well-meaning operator. Do not build the gun.

---

## 5. Data model

Per node, three cooperating structures:

```
hash table: key -> *Entry              open addressing or chaining; O(1) average
Entry { key, value: bytes; expiry: uint32; version: uint32 (CAS);
        lru_prev, lru_next: *Entry     intrusive list, exact LRU
        last_access: uint24 }          coarse clock, approximate LRU
free space: slab classes, or a general allocator (§7.5)
```

The value is opaque bytes: the cache does not parse, index, or understand it, which keeps the server
simple enough to be fast and is why a cache is not a database.

Cluster-wide there is exactly one piece of shared metadata: **the ring**, the ordered map from hash
positions to nodes with each node's virtual positions and weight. It changes only on membership change, is
a few kilobytes for hundreds of nodes, and is the one thing every client must agree on. Everything else is
per-node and independent — no cross-node coordination on any request path — which is what makes the
cluster scale horizontally. Note what is absent: no write-ahead log, no durability replication, no on-disk
representation. A node that restarts comes back empty by design; §7.6 covers why that is still dangerous.

---

## 6. Architecture, derived

### Attempt 1: one big cache node

A single machine with 768 GB of RAM holds the working set and every application server connects to it. It
fails on both axes: throughput caps at 150,000 operations per second against a requirement of a million,
so 85% of traffic falls through to the database, and the failure domain is total — losing the node sends
the full million operations per second at an origin sized for 50,000.

### Attempt 2: shard with `hash(key) mod N`

Now the client hashes and divides. Capacity and throughput scale linearly, and this is genuinely correct —
until N changes, at which point 91% of keys relocate and the origin takes the 18× spike derived in §3.
Since N changes every time a node dies, is added, or is replaced by an autoscaler, the design's stable
state is "about to have an incident." And **a node that fails and returns is also a change of N**, so a
flapping node causes repeated whole-keyspace remaps: the scheme is a failure amplifier.

### Attempt 3: consistent hashing with virtual nodes

Place nodes and keys on the same 2^32 ring; a key belongs to the first node clockwise. Adding a node
claims one arc and moves only the keys in it — 1/N of the keyspace, per §3 — and removing one hands its arc
to a single successor. Both need virtual nodes to be true in practice, which §7.1 derives. The result:
membership changes cost a small bounded miss burst instead of a full flush.

### Attempt 4: replication for read scale and failover

The cache is now shardable and stable under membership change, and still loses 1/N of its contents when a
node dies — 8% of the keyspace at twelve nodes, a visible latency step and, for a hot shard, worse. Give
each shard a **primary and one or more read replicas** fed by asynchronous streaming. Two distinct
benefits, not to be conflated:

- **Failover.** A replica is promoted when the primary is declared dead, so the shard's contents survive a
  node loss and the miss burst never happens. This needs a failure detector and agreement about who is
  primary — the one place a cache needs consensus, usually delegated to a small quorum service rather than
  built.
- **Read scaling.** Replicas serve reads, multiplying a shard's read throughput. This matters only for
  skewed workloads; for uniform ones, adding shards is strictly better because it adds capacity too.

Asynchronous replication means a replica can lag and serve a value the primary has overwritten. For a
cache that is acceptable — bounded staleness on top of data that is already a stale copy — but say it,
because a `SET` on the primary followed by a `GET` from a replica is not read-your-writes. Applications
that need that pin reads to the primary.

### Attempt 5: bound the memory and evict

Memory fills; something must go, and the policy is LRU because of temporal locality (Chapter 02 §4). The
implementation must be O(1), and in production it is not exactly LRU at all. Both halves are §7.2.

### Final architecture

```
app servers
    │  ring: hash positions → nodes; a few KB, changes only on membership change
    ├── thin client ── consistent hash ──┐   (client-side routing, §7.3)
    └── proxy tier ─── consistent hash ──┤   (proxy routing, §7.3 — recommended at scale)
                                         ▼
        ┌───────────┬───────────┬───────────┐
        │ shard 0   │ shard 1   │ ...  11   │
        │ primary   │ primary   │           │  async replication to
        │  +2 repl  │  +2 repl  │           │  replicas within each shard
        └───────────┴───────────┴───────────┘
              ▲                    │ miss
   membership │                    ▼
    ┌─────────┴────────┐     origin database
    │ topology / quorum│
    └──────────────────┘
```

---

## 7. Deep dives

### 7.1 Consistent hashing, and why virtual nodes are not optional

The mechanism is Chapter 04 §6 and §3 quantified the payoff: 9% of keys move instead of 91%. Naive
placement — one ring position per node — has two defects, and both are quantitative.

**Load imbalance.** With n points placed uniformly at random on a circle the expected largest gap is about
`H_n / n`, where `H_n` is the nth harmonic number, against a mean gap of `1/n`. For ten nodes `H_10 ≈ 2.93`,
so the largest arc is roughly **2.9× the average**: one node holds three times its share of keys and
traffic and runs out of memory first while another sits idle. Usable capacity is set by the unluckiest
node.

**Uneven failure.** A dead node's entire arc goes to exactly one successor, which now holds double its
share and, having no room, immediately evicts half of what it had. One failure becomes two degraded nodes,
and cascades from there.

**Virtual nodes fix both.** Place each physical node at V ring positions. Each node's total share is the
sum of V arcs, and by the law of large numbers the relative spread of that sum shrinks as `1/√V`:

```
V = 1 → spread on the order of ±100% (the 2.9× above);  V = 100 → ≈ ±10%;  V = 200 → ≈ ±7%
```

A hundred to two hundred virtual nodes each is the standard range, and this arithmetic is why it is that
number rather than "many." Failure spreads too: a dead node's V arcs go to up to V different successors, so
the load lands as V small increments rather than one doubling.

Two further consequences. Virtual node counts can be **weighted** — twice the RAM, twice the positions —
which is how a heterogeneous fleet stays balanced without a separate placement system. And **a newly added
node is entirely cold**: the 9% of keys it claims all miss until it fills, so if that burst is still too
large, introduce its virtual positions gradually and the origin sees a ramp instead of a step.

### 7.2 O(1) eviction, and why production LRU is approximate

Exact LRU is a **hash map plus an intrusive doubly-linked list**, and the pairing is what makes it O(1)
(Chapter 02 §4). The map takes a key to the entry's address; the list holds entries in recency order, most
recent at the head. On a hit you already have the node's address from the map, so unlinking it and
relinking it at the head is a constant number of pointer writes — **you never traverse the list, which is
the whole reason for the pairing.** Eviction pops the tail. Nothing in the structure is proportional to
the number of entries.

That is the answer to give when asked for the implementation. Now the part most candidates do not know:
**real caches do not do this, because at scale it costs more than the accuracy is worth.** Three costs.

**Every read becomes a write.** A `GET` mutates the list head, which on a multi-threaded server is a
contended cache line needing a lock — so a purely read-only workload, the entire point of a cache,
serializes on LRU bookkeeping. At a million operations per second this is the bottleneck, and it is
absurd: the metadata about the data costs more than the data.

**The pointers cost real memory.** Two 8-byte pointers per entry across 500 million entries is 8 GB, over
1% of the cluster's RAM, spent recording an ordering used only to answer "which is oldest," approximately.
And following those pointers is chasing randomly located heap objects, so every step is a likely CPU cache
miss.

**Approximate LRU is what production does.** Store a coarse last-access timestamp per entry — 24 bits is
plenty — and on eviction **sample a handful of keys at random and evict the least recently used among
them.** Redis samples five by default (`maxmemory-samples`), and carrying a small candidate pool between
rounds gets very close to true LRU. The costs invert: a read becomes one non-atomic timestamp store with
no lock and no list, per-entry metadata drops from 16 bytes to 3, and eviction costs work proportional to
the sample size rather than to anything global.

The accuracy loss is real and unimportant: sampling five occasionally evicts something that was not the
very oldest, costing a fraction of a percent of hit rate against a throughput difference measured in
multiples. Memcached takes a related route — segmented LRU with a bump that fires only if the entry has
not been touched in 60 seconds, so a hot key is relinked once a minute rather than a million times.

The transferable lesson: **when the exact version of a bookkeeping structure costs more than the thing it
bookkeeps, sample.** The same reasoning gives Count-Min Sketch (Chapter 04 §5) and approximate percentiles
(Chapter 73).

### 7.3 Routing: client-side, proxy, or server-side redirect

Something must turn a key into a node. Three places to put it.
**Client-side.** Every application process embeds the ring and connects directly to every node. One
network hop, the theoretical minimum, and no extra tier to operate. Three costs, all of which appear at
scale. You need a correct, identical client in every language you use, and the hashing must agree bit for
bit — a client whose hash seed differs silently sees a 100% miss rate. Topology changes must reach
thousands of processes, so removing a node is a fleet-wide rolling config change measured in minutes,
during which two clients disagree about who owns a key. And connections multiply: `5,000 app processes ×
12 nodes = 60,000 connections`, at ~16 KB of socket and read buffers each, about 1 GB per node spent on
connections alone. At 100 nodes it is half a million connections, and the buffers eat a meaningful
fraction of the RAM you bought to cache with.

**Proxy tier** (mcrouter, twemproxy, Envoy). Clients speak the cache protocol to a nearby proxy that owns
the ring. Adds a hop — 0.2–0.5 ms, real but small against a database's 5–50 ms. In exchange: topology lives
in one place and changes take effect immediately; clients are trivial and language-agnostic because they
need no hashing at all; connections collapse, since the proxy multiplexes thousands of client connections
onto a small pool per node; and the proxy is the natural home for request coalescing, shadow traffic during
a migration, mirrored writes to a new cluster, and per-key limits. **This is the recommended answer at any
real scale**, for an operational rather than architectural reason: it is the only design where changing the
topology is one action instead of thousands.

**Server-side redirect** (Redis Cluster's `MOVED`/`ASK`). The client holds a cached slot map and asks any
node; if wrong, the node replies with a redirect and the client updates and retries. Clients need only
bootstrap knowledge and correctness does not depend on every client being current, because the cluster is
authoritative. The cost is two round trips per misrouted key, concentrated during a rebalance when the
cluster is already stressed — and the protocol complexity moves into the client anyway.

### 7.4 Hot keys

Sharding balances the *keyspace*, not the *traffic*. One key — a celebrity profile, a feature flag read on
every request, the front page — can exceed a single node's entire throughput.

```
node capacity ≈ 150,000 ops/s      one key's read rate ≈ 400,000 ops/s
```

No amount of resharding helps — a key lives on one node by definition. Three responses.

**A client-side or proxy-side local cache with a short TTL.** The most effective by a wide margin, because
it converts a network read into a memory read and the *entire* excess disappears: 400,000 reads per second
across 5,000 clients with a one-second local TTL becomes at most 5,000 reads per second to the node. The
cost is staleness bounded by that TTL, plus the incoherence of Chapter 02 §1. For immutable values it is
free (Chapter 20 §7.7, where the hot key is a published tweet); for mutable ones, keep the TTL to a second
or two, or push invalidations — RESP3 client-side caching has the server track which clients hold which
keys, which is this mechanism with the staleness window closed.

**Replicate the key across nodes.** Write it as `key:0` … `key:9` and have readers pick a random suffix,
spreading reads across ten nodes. Effective and blunt: writes and invalidations now cost ten operations,
and a partial invalidation leaves shards disagreeing, so a reader can see the value flip between old and
new depending on the suffix drawn. Use it when the value is immutable or the inconsistency is harmless.

**Detect hotness automatically.** The server keeps a Count-Min Sketch of key frequencies (Chapter 04 §5)
and reports keys above a threshold; the proxy then caches those locally on its own. This is the version
that scales operationally, because nobody has to notice the hot key first — and noticing is what humans
are bad at here, since the symptom is one node's p99 rising while the cluster looks fine in aggregate.

### 7.5 Memory: why usable is less than allocated

A node with 128 GB of RAM does not store 128 GB of cached data, and the gap is larger than people expect.
Four sources, in decreasing order of surprise.

**Internal fragmentation from size classes.** Memcached allocates from **slabs**: fixed-size chunk classes
growing by 1.25× — 96 B, 120 B, 152 B — with each item placed in the smallest class that fits and the
remainder wasted. Worst case 20%, average about 11%. That is the price of never having external
fragmentation and never needing compaction, since allocation and free are O(1) pointer moves.

**Slab calcification**, which is the memcached failure people actually hit. Memory is assigned to a class
when first needed and stays there. A workload that spent a week storing 1 KB values and then shifts to
100 KB values finds all its pages committed to the 1 KB class, so the new class starves and evicts
constantly while gigabytes sit idle in a class nothing uses. The hit rate collapses with no change in
traffic and no obvious cause. The fix is automatic slab rebalancing, which must be enabled deliberately.

**External fragmentation from a general allocator.** Redis uses jemalloc over variable-size objects, so
freed regions do not necessarily coalesce into usable space. The signal is RSS ÷ `used_memory`: 1.0–1.1 is
healthy, above 1.5 means a third of resident memory is unusable. It worsens with size variance and after
mass deletion — "we cleaned up, why is memory still high" — and active defrag trades CPU to reclaim it.

**Per-entry overhead.** Roughly 50–100 bytes per entry for headers, expiry, hash slots, and LRU metadata.
At 1 KB values that is 5–10%; at 100 B values it is **50–100%** — a cache of small values spends as much
memory on bookkeeping as on data.

The operational consequence is the point of this dive: **provision 1.3–1.5× your computed data size and
alert on the fragmentation ratio, because the eviction policy counts `used_memory` while the kernel's OOM
killer counts RSS.** A node can believe it has headroom, decline to evict, and be killed anyway — which is
how a cache cluster dies with its own dashboards saying it is fine.

### 7.6 Stampede, penetration, avalanche — from the server's side

Chapter 02 §3 gives these three as client-side problems. Building the cache lets you solve them *in the
cache*, which is strictly better because the fix applies to every caller for free.
**Stampede.** A hot key expires and a thousand concurrent readers all miss and all recompute. The
server-side fix is a **lease**: on a miss the server hands exactly one client a token authorizing it to
recompute and tells everyone else to wait briefly or take the stale value it still holds. One origin query
instead of a thousand, with no application coordination — the single-flight of Chapter 02 §3 hoisted to
where it belongs. Serving stale during a refresh beats serving slow.

**Penetration.** Keys that do not exist miss every time and reach the origin every time, trivially
weaponized by anyone who can supply an identifier. Support **caching a negative result** as a first-class
small value with its own short TTL, so callers need not encode "absent" as a magic string. A Bloom filter
in front (Chapter 04 §5) is the complementary defense when the key universe is known.

**Avalanche.** Keys populated together expire together — after a deploy, a flush, a bulk warm — and the
origin takes a synchronized wave. The server should **jitter every TTL it is given** by a uniform 0–10% at
`SET` time. One line, and it removes a class of incident no individual caller would think to prevent.

A fourth belongs to building the cache rather than using it: **a restarted node is cold and must not take
full traffic immediately**, since an empty node claiming its 8% of the keyspace turns 8% of a million
operations per second into origin load. Warm it from a peer or from shadowed live traffic before it joins,
or ramp its virtual nodes in as in §7.1.

### 7.7 Consistency with the source of truth, and multi-region

Cache-aside with delete-on-write (Chapter 02 §2) is the default, and the residual race is
worth stating precisely: a reader misses, reads the database, is descheduled, a writer commits and deletes
the key, and the reader then writes its now-stale value. The fixes are `CAS` on the repopulating write, a
lease taken at miss time that a concurrent delete invalidates, or versioned keys so the stale value lands
under a key nobody will read. The TTL is the backstop under all of them, which is why an unbounded TTL is
a correctness question and not only a memory one.

A subtler failure belongs to this chapter specifically: **a delete sent to a primary that then fails over
may be lost**, because replication is asynchronous and the promoted replica never saw it, so the stale
value resurrects. Either route deletes through a durable invalidation stream rather than a best-effort
call, or keep TTLs short enough that resurrection self-heals. Deletes are idempotent, so replaying the
stream is harmless — an unusually clean fit for at-least-once delivery (Chapter 03 §3).

**Across regions, do not try to make the caches coherent.** The arithmetic settles it: a cross-region round
trip is 70–150 ms against a local hit of 0.5 ms, so synchronous cross-region invalidation adds two to three
orders of magnitude to every write in order to reduce staleness in a layer that is already a stale copy of
the database. A globally replicated cache is a full consistency problem — conflicts, ordering, partitions —
with none of a database's payoff, because the data is disposable.

**Per-region caches with short TTLs win.** Each region runs an independent cluster in front of its own
database replica, the TTL is the staleness the product tolerates, and cross-region cache traffic is zero.
When better than TTL-bounded staleness is needed, publish invalidations best-effort over a per-region
pub/sub topic — cheap, idempotent, harmless when it fails, because the TTL still bounds the error. The
reference implementation is Facebook's, where invalidations are derived from the database's own
replication stream so they cannot diverge from the write that caused them; and even there the regional
pools are independent caches, not one coherent global one.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Single node lost | Its share of keys gone; miss rate rises by ~1/N | Replica promotion; origin sized for the burst; virtual nodes keep the share small |
| Node flapping | Repeated arc reassignment and repeated cold starts | Damp the failure detector; require sustained failure before removal |
| Cold node takes traffic | Its full share of requests hits the origin at once | Warm from a peer, or ramp virtual nodes in gradually (§7.1, §7.6) |
| Hot key saturates a node | One node's p99 climbs while the cluster looks healthy | Local caching at the proxy; key replication with suffixes; automatic detection via a frequency sketch (§7.4) |
| Memory pressure and fragmentation | Evictions rise, hit rate falls, or the kernel kills a node that believes it has room | Provision 1.3–1.5×; alert on RSS ÷ `used_memory`; enable slab rebalancing or active defrag (§7.5) |
| Whole cluster lost | Full traffic to the origin, 20× normal | Origin must be sized or shed-capable; coalesce misses; this is the one true cache outage |
| Topology disagreement between clients | Two clients route one key to two nodes; both stale and inconsistent | Proxy tier, or server-side redirect, so the topology has one owner (§7.3) |

**Monitoring:** hit rate per shard — the most informative single number, since a slow decline predicts an
incident days out and a sudden drop localizes to a node or a deploy; p99 per node, which is the hot-key
detector; eviction rate, which separates "working set grew" from "traffic changed"; the fragmentation
ratio; connections per node; and origin QPS, because the cache's real output metric is load *not* sent to
the database.

---

## 9. Common mistakes

1. **`hash(key) mod N`.** Correct until the first membership change, at which point 91% of keys move and
   the origin takes an 18× spike. The derivation in §3 is the argument, and it should be volunteered.
2. **Consistent hashing without virtual nodes.** The ring alone is not enough: with one position per node
   the largest arc is about 2.9× the mean at ten nodes, capacity is set by the unluckiest node, and one
   failure doubles one successor.
3. **Describing exact LRU as what production runs.** Hash map plus doubly-linked list is the right answer
   to "implement LRU in O(1)" and the wrong answer to "what does a cache actually do" — sampling is
   cheaper on memory, locking, and cache lines, for a fraction of a percent of hit rate.
4. **Treating the cache as durable.** The moment a design assumes a value exists only in the cache it has
   acquired a data-loss bug and given up the freedom that made every other decision here cheap.
5. **Making the cache a hard dependency.** Read-through and write-behind both do this (Chapter 02 §2);
   cache-aside degrades to slow rather than broken, which is the entire availability requirement.
6. **Ignoring cold start.** Restarting a node, or the cluster, without a warm-up plan converts a routine
   deploy into an origin overload.
7. **Sizing memory from the data size**, ignoring per-entry overhead and fragmentation, then being
   surprised by evictions at 70% of nominal capacity or an OOM kill at "80% used."
8. **Chasing cross-region cache coherence** — a hard consistency problem undertaken for data that is
   disposable and already stale — or **adding `SCAN`**, which blocks a single-threaded server for seconds
   and is an outage with an operator's name on it.

---

## 10. Variants

**Memcached versus Redis**, which is the same question as "how simple should the server be." Memcached is
multi-threaded, slab-allocated, and stores only opaque bytes, which makes it excellent at exactly one
thing. Redis is single-threaded per shard with rich data types — sorted sets, hashes, streams — which
Chapters 20, 71, and 72 depend on. Choose Memcached for a pure lookaside cache and maximum simplicity;
Redis when the data structures are the point, accepting that a slow command blocks the shard.

**Near-cache / two-tier.** An in-process L1 in front of the distributed L2: nanoseconds and incoherent in
front of microseconds and shared. Correct for a small, extremely hot, staleness-tolerant set, and it is
the hot-key answer of §7.4 generalized. **Write-through cache-as-database** is the opposite move — the
cache in the write path so it is never stale, at the cost of making it a correctness dependency and paying
both write latencies. Almost always wrong, the exception being a write-heavy counter workload where
write-behind's durability loss is genuinely acceptable.

**Function-result caching.** The same infrastructure keyed by a hash of the arguments rather than an
entity ID. Everything here applies; the only new question is choosing a key that captures every input,
because a missed input is a wrong answer served fast — the worst possible failure. **Persistent-tier
hybrids** backed by NVMe are the other easy extension, trading a 100 µs read for far more capacity per
node, and worth it when the working set is enormous and the origin is slower still.

---

## 11. Further reading

- Chapter 02 for how to use a cache, Chapter 04 §6 for the ring in general form
- Rajesh Nishtala et al., "Scaling Memcache at Facebook" (NSDI 2013) — leases, regional pools, cold-cluster
  warmup, invalidation from the replication stream; the most useful single paper on this chapter
- Karger et al., "Consistent Hashing and Random Trees" (STOC 1997), for the original result
- Redis documentation on `maxmemory-policy`, `maxmemory-samples`, and the fragmentation ratio; memcached's
  documentation on slab allocation, slab rebalancing, and the segmented LRU
- [facebook/mcrouter](https://github.com/facebook/mcrouter), for what a production proxy tier does
