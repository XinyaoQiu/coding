# Chapter 02 — Caching and Delivery

Caching is the cheapest performance win available and the richest source of subtle bugs. This chapter
covers where caches go, how they fail, how they evict, and how bytes get from your servers to a user's
screen — including the choice of protocol for pushing data the user did not ask for.

---

## 1. Where caches live

A request passes through several possible caches before it reaches your database. Naming the layer you
mean is the first step to a coherent answer.

| Layer | Example | Invalidation | Good for |
|---|---|---|---|
| Client | Browser cache, mobile app store | Hard — you don't control the client | Static assets, user's own data |
| CDN / edge | CloudFront, Fastly | Purge API, TTL | Static assets, media segments, cacheable API responses |
| Reverse proxy | Nginx, Varnish | TTL, purge | Whole-response caching |
| Application-local | In-process LRU map | TTL, process restart | Tiny, extremely hot, tolerant of staleness |
| Distributed cache | Redis, Memcached | Explicit, TTL | The general case — shared across the fleet |
| Database | Buffer pool, query cache | Automatic | Free; you don't design it, but know it exists |

**Application-local caches deserve a specific warning.** They are the fastest option and they have no
coherence between processes: with fifty servers you have fifty different versions of the truth, and the
staleness window is a function of TTL, not of any invalidation you perform. They are correct for data
that is small, extremely hot, and changes rarely — a feature-flag table, a currency-rate table. They are
wrong for anything a user can change and immediately observe.

---

## 2. Caching patterns

### Cache-aside (lazy loading)

The default, and what you should say unless you have a reason not to.

```
read(key):
    v = cache.get(key)
    if v is not None: return v
    v = db.get(key)
    cache.set(key, v, ttl)
    return v

write(key, value):
    db.write(key, value)
    cache.delete(key)          # delete, not update — see below
```

- **Pros:** only requested data is cached; a cache failure degrades to a slow system, not a broken one;
  the cache and the database can hold different shapes of data.
- **Cons:** every cache miss costs an extra round trip; there is a window between the database write and
  the cache delete during which readers get stale data.

**Delete, don't update.** Updating the cache on write invites a race: two concurrent writers can apply
their database writes in one order and their cache updates in the other, leaving the cache permanently
inconsistent with the database. Deleting means the next reader repopulates from the authoritative source.
The remaining race (a reader repopulating with a stale value it read just before another writer's commit)
is much narrower and is usually bounded by making the TTL modest.

### Read-through

The cache sits inline; the application only talks to the cache, which fetches from the database on a
miss. Simplifies application code, moves the loading logic into the cache layer, and makes the cache a
hard dependency — if it is down, everything is down.

### Write-through

Write to cache and database synchronously. The cache is never stale. Every write pays both latencies,
and you cache data that may never be read.

### Write-behind (write-back)

Write to cache, acknowledge, flush to the database asynchronously. Excellent write throughput,
and **you will lose data if the cache node dies before the flush**. Acceptable for view counters,
unacceptable for anything transactional. If you propose it, say the durability cost in the same breath.

### Refresh-ahead

Proactively refresh entries that are about to expire and are being actively read. Hides the miss latency
from users; wastes work on entries that would not have been requested again.

---

## 3. The three failure modes

Every serious cache discussion in an interview should touch these by name. They are what separates
"we'll add a cache" from a design.

### Stampede (thundering herd, dog-piling)

A hot key expires. The next thousand concurrent requests all miss, all query the database, and all
recompute the same value. The database, which was comfortably serving a 99% hit rate a millisecond ago,
receives its full uncached load in a burst.

Mitigations:

1. **Request coalescing / single-flight.** Per key, allow exactly one in-flight recomputation; other
   requests wait for its result. A few lines of code, and the most effective fix.
2. **Probabilistic early expiration.** Each reader independently decides, with a probability that rises
   as the TTL approaches, to refresh the value early. Spreads the refresh over time with no coordination.
3. **Lock on recompute.** A short-lived distributed lock around the recomputation; losers serve the stale
   value rather than blocking. Serving stale during a refresh is almost always better than serving slow.
4. **Never expire the hot set;** refresh it out of band on a schedule.

### Penetration

Requests for keys that **do not exist** miss the cache by definition and hit the database every time.
Ordinary in a system with user-supplied identifiers, and a trivial denial-of-service vector.

Mitigations: **cache the negative result** with a short TTL, and/or put a **Bloom filter** of existing
keys in front — a filter that says "definitely not present" ends the request without a database read.

### Avalanche

Many keys expire at the same moment — typically because they were all populated at the same moment, such
as after a deployment or a cache flush — and the database absorbs a synchronized wave of misses.

Mitigation: **jitter the TTL.** `ttl = base + random(0, base * 0.1)`. One line, and it removes the
synchronization entirely. Also: warm the cache before taking traffic after a restart.

---

## 4. Eviction

A cache is bounded, so something must be discarded.

**LRU (least recently used).** The default, and correct for most workloads because of temporal locality.
The implementation is worth knowing precisely, because interviewers ask for it: a **hash map plus a
doubly-linked list**. The map gives O(1) lookup from key to node; the list maintains recency order, with
the most-recently-used at the head. On access, unlink the node and move it to the head — O(1) because the
map gave you the node directly, so you never traverse. Evict from the tail. Both operations are O(1),
which is the whole point of the pairing.

**LFU (least frequently used).** Keeps items by access count. Better for workloads with a stable hot set;
suffers from cache pollution, where an item that was popular once resists eviction forever. Fixed with
aging — periodically halve all counts.

**FIFO.** Simple, ignores access patterns, rarely the right answer.

**TTL-based.** Not really an eviction policy — it is a staleness bound. Combine it with LRU: TTL decides
when data is too old to trust, LRU decides what to drop when memory is full.

**Random / sampled.** Redis's `allkeys-lru` is in fact approximate: it samples a handful of keys and
evicts the least recently used among them. Exact LRU requires bookkeeping that costs more than the
accuracy is worth. Knowing that production LRU is usually approximate is a nice detail.

---

## 5. Invalidation

> There are only two hard things in computer science: cache invalidation and naming things.

The three honest strategies:

**TTL.** Accept bounded staleness. Simplest, self-healing, and correct far more often than people admit.
Choose the TTL from how stale the data may be, not from how often it changes.

**Explicit invalidation on write.** Delete the key when the underlying data changes. Requires that every
writer knows every cache key derived from the data it touched — which is the part that breaks, especially
when a second service starts writing to the same table. This is a real production failure mode: the cache
was correct until someone added a writer that did not know about it.

**Versioned keys.** Include a version or a content hash in the key: `user:123:v7`. Writers bump the
version; readers naturally miss and repopulate; old entries age out on their own. No delete needed, no
race, at the cost of temporarily holding both versions. Excellent when a single logical change
invalidates many derived keys.

---

## 6. Content delivery networks

A CDN is a cache with geography. Points of presence near users serve content from local storage,
reducing latency (physics: a cross-continent round trip is ~150 ms and nothing you do in software
improves it) and offloading origin bandwidth.

**Pull CDN.** The edge fetches from your origin on first request and caches. Simple, self-managing; the
first user in each region pays the miss.

**Push CDN.** You upload content to the CDN ahead of time. Right for a known, bounded catalog and for
launch moments where the first-request penalty is unacceptable.

What matters in a design:

- **Cache key.** By default the URL. If you vary responses on a header (language, device), that header
  must be part of the key, or you serve one user's variant to another. Getting this wrong is a real
  incident, not a theoretical one.
- **TTL and purge.** Purges are eventually consistent across the edge — expect seconds to minutes.
  Design for it: version your asset URLs (`app.a3f9c1.js`) and you never need to purge at all.
- **What is cacheable.** Static assets, always. Media segments, always. Personalized API responses,
  usually not — though a response that varies only by a few buckets can be cached per bucket.
- **Origin shield.** An intermediate cache layer between the edge and your origin, so a global purge or
  a cold cache produces one origin request rather than one per point of presence.

---

## 7. Load balancing

**Layer 4** balances on TCP/UDP connection information. Fast, protocol-agnostic, cannot make decisions
based on the content of a request.

**Layer 7** parses the application protocol and can route on path, header, or cookie, terminate TLS, and
retry idempotent requests. More expensive, far more useful.

Algorithms: round-robin (default), least-connections (better with heterogeneous request costs),
consistent hashing on a key (routes the same user or key to the same backend — necessary when backends
hold state or local caches), weighted variants (for heterogeneous hardware or gradual rollout).

Two properties to mention when they matter:

- **Health checking.** Passive (observe failures) and active (probe an endpoint). The subtlety is that a
  health check which merely proves the process is alive will keep sending traffic to a server that is
  alive and useless.
- **Connection draining.** On deploy, stop sending new connections and let existing ones finish. Without
  it, every deployment is a small outage.

---

## 8. Pushing data to clients

When the server has something the client did not request, four options. Choosing correctly, and saying
why, is a recurring deep dive in Part II.

### Short polling

The client asks every N seconds. Trivially simple, works through every proxy and firewall, wastes a
request per interval per client, and has a latency floor of N.

Correct for: low-frequency updates, small client counts, and when simplicity genuinely matters.

### Long polling

The client sends a request; the server holds it open until there is data or a timeout, then responds; the
client immediately re-requests. Near-realtime delivery over ordinary HTTP.

Costs: a held connection per client (so connection count, not request count, becomes the constraint), and
a reconnect gap after every message during which events must be buffered server-side.

### Server-Sent Events (SSE)

A single long-lived HTTP response that the server writes to incrementally. **Unidirectional**
(server → client) and text-only. The browser reconnects automatically and can resume from a `Last-Event-ID`,
which is genuinely useful and often forgotten.

Correct for: notification streams, live dashboards, progress updates, and **LLM token streaming**
(Chapter A3) — anywhere the client has nothing to say back on the same channel.

### WebSocket

A full-duplex connection established by upgrading an HTTP request. Both sides can send at any time, with
low per-message overhead.

Costs: the connection is **stateful**, which means your fleet becomes stateful — you now need a registry
mapping user to connection server, a routing mechanism between servers, a plan for reconnection, and load
balancers configured for long-lived connections. Sizing is by concurrent connections (roughly 100k per
server as a planning number), not by request rate.

Correct for: chat (Chapter 30), multiplayer games (A1), collaborative editing (A0) — anywhere the client
sends as often as the server does.

### Native push (APNs, FCM, Web Push)

The only option that reaches a client whose app is **not running**. Delivery is best-effort, the payload
is small, and you inherit the platform's semantics and error codes (Chapter 32).

### Choosing

| Requirement | Choice |
|---|---|
| Client also sends frequently | WebSocket |
| Server-to-client only, browser client | SSE |
| Must work through hostile proxies, simplicity paramount | Long polling |
| App may be closed | Native push |
| Updates every few minutes and correctness of timing doesn't matter | Short polling |

The mistake to avoid is reaching for WebSocket reflexively. If the data flows one way, SSE gives you
auto-reconnect, resumption, and plain HTTP semantics for free — and it does not turn your stateless
service into a stateful one.

---

## 9. Compression and payload

- **Transport compression** (gzip, Brotli) on text responses: large win, small CPU cost, on by default.
- **Payload format:** JSON is the default; a binary format (Protocol Buffers, MessagePack) reduces size
  and parse cost meaningfully on mobile and in service-to-service calls, at the cost of debuggability and
  a schema-evolution discipline.
- **Do not compress what is already compressed** — images, video, and archives gain nothing and cost CPU.
- **Pagination is compression.** The largest payload win is usually not returning data nobody scrolls to.

---

## 10. Checklist for a caching decision

1. What is the hit rate you expect, and what is the read:write ratio that produces it?
2. How stale may this be? (This determines TTL, not how often the data changes.)
3. What happens on a miss — is the origin able to serve the miss rate?
4. What is the hot key, and what happens when it expires? (Stampede.)
5. Who else writes this data, and do they know the cache exists? (Invalidation.)
6. If the cache is entirely down, does the system degrade or fail?

Question 6 is the one that most often reveals an accidental hard dependency, and it is worth asking
yourself before the interviewer does.

---

## Further reading

- [system-design-primer — Caching](https://github.com/donnemartin/system-design-primer#cache)
- Redis documentation on eviction policies and `maxmemory-policy`
- Facebook's "Scaling Memcache at Facebook" (NSDI 2013) — the definitive treatment of stampede, leases,
  and cache consistency at scale
