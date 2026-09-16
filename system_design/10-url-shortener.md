# Chapter 10 — URL Shortener

> **Prerequisites:** Chapters 01 (partitioning, store selection), 02 (cache-aside, CDN), 04 (ID generation)
> **Patterns:** read-heavy key-value, coordination-free key generation, asynchronous analytics

---

## 1. The problem

A user submits a long URL and receives a short one. Anyone who visits the short URL is redirected to the
original. Bit.ly, TinyURL, and every link in a marketing email work this way.

It is the standard first system design question, and it deserves that position: the functional
requirements fit in one sentence, so nothing distracts from the two things actually being tested —
whether you can generate identifiers without coordination, and whether you understand what a 100:1
read-to-write ratio does to an architecture.

**The property that makes it interesting:** the write path is small, occasional, and can afford to be
slow. The read path is enormous, latency-critical, and must never be down, because a dead redirect breaks
every link ever printed on every piece of paper. These two paths have almost nothing in common and should
be designed as if they were different systems.

---

## 2. Requirements

### Functional

1. Create a short URL from a long URL.
2. Redirect a short URL to its original.
3. *(Common extension)* Custom aliases — the user proposes their own short code.
4. *(Common extension)* Expiring links.
5. *(Common extension)* Click analytics.

Scope 1 and 2 as core; offer 3–5 and let the interviewer pick. Analytics is the most common follow-up and
is where the design gets interesting a second time.

### Non-functional

- **Read latency** — redirect p99 under 100 ms, because it sits in front of a page load the user is
  already waiting for.
- **Availability** — the read path must be effectively always up. Creation can be down for minutes with
  no lasting harm; redirection cannot be down at all, because the links are already in the world.
- **Read:write ratio** — assume 100:1 at minimum; for links in published content, far higher.
- **Uniqueness** — short codes must never collide. A collision means one user's link silently sends
  traffic to another user's destination, which is both a bug and a security incident.
- **Durability** — a lost mapping is a permanently broken link. Links are expected to outlive the
  services that created them.
- **Consistency** — read-your-writes on creation (the user must be able to test the link immediately).
  Everything else can be eventually consistent.

### Explicitly out of scope

Link preview generation, malware scanning beyond a blocklist check, and user account management. Name
them so the interviewer knows you saw them.

---

## 3. Estimation

Assume 100 million new links per day.

**Write rate**

```
100M / 86,400 s ≈ 1,160 writes/sec
```

Peak, at 3×: ~3,500 writes/sec. This is small. A single database node handles it. Say so — the temptation
to over-engineer the write path is the first trap in this question.

**Read rate**

```
1,160 × 100 ≈ 116,000 reads/sec average
peak ≈ 350,000 reads/sec
```

This is not small, and it is the number that shapes the design. It rules out serving redirects from disk
on every request and makes caching mandatory rather than an optimization.

**Storage**

```
100M/day × 365 × 5 years   ≈ 180 billion records
per record: short_code (7B) + long_url (~200B) + user_id (8B) + timestamps (16B) + overhead ≈ 500 B
180e9 × 500 B              ≈ 90 TB
```

90 TB does not fit on one machine, so the store is sharded. Note that this conclusion is the *only*
reason we did the storage arithmetic; had it come out at 900 GB, the design would be materially simpler.

**Keyspace**

With base62 (`a–z`, `A–Z`, `0–9`) and a 7-character code:

```
62^7 ≈ 3.5 × 10^12
```

Against 180 billion links over five years, that is about 5% keyspace utilization — comfortable. Six
characters gives 5.7 × 10^10, which is *less* than our five-year volume, so 7 is the minimum. This is a
genuine derivation, not a memorized constant, and doing it out loud is worth the thirty seconds.

**Cache sizing**

Link popularity is a steep power law: a small fraction of links serve the overwhelming majority of
requests. Caching the hottest 20% of one day's links:

```
100M × 0.2 × 500 B ≈ 10 GB
```

Ten gigabytes buys a hit rate well above 90%. This is the single most important number in the design, and
it is small enough to be reassuring.

---

## 4. API

```
POST /urls
  body:  {longUrl, customAlias?, expiresAt?}
  auth:  Bearer token
  ->     201 {shortUrl, shortCode, expiresAt}
  ->     409 if customAlias is taken

GET /{shortCode}
  ->     302 Location: <longUrl>
  ->     404 if unknown, 410 if expired

GET /urls/{shortCode}/stats
  auth:  Bearer token (owner only)
  ->     200 {clicks, byDay[], byCountry[], byReferrer[]}
```

Three points worth making explicitly:

**The owner comes from the token, not the body.** Accepting `userId` in the create request would let any
caller create links attributed to anyone.

**The redirect endpoint sits at the root path**, not under `/api/`. Short codes compete with every other
route, so reserved words (`about`, `login`, `api`, `health`) must be excluded from the generated
keyspace and from custom aliases.

**Creation is authenticated; redirection is not.** These are different security postures, which reinforces
that they are different systems.

---

## 5. Data model

```
urls
  short_code    VARCHAR(10)  PRIMARY KEY / partition key
  long_url      TEXT
  user_id       BIGINT
  created_at    TIMESTAMP
  expires_at    TIMESTAMP    NULL
```

Access is always by exact primary key. There are no joins, no range scans, and no secondary access
patterns on the read path. That is a key-value workload, and the store should be one: **Cassandra or
DynamoDB, partitioned on `short_code`**.

Hash partitioning on the code distributes both reads and writes evenly by construction, because the codes
are effectively random. There is no hot partition problem at the storage layer — the hot *key* problem is
real, and it is solved by the cache, not by the partitioning scheme.

A second table, if links are listed per user:

```
user_urls
  user_id       BIGINT       partition key
  created_at    TIMESTAMP    clustering key, descending
  short_code    VARCHAR(10)
```

This is a deliberate denormalization: the same fact stored twice, partitioned two ways, because the two
queries have different keys. In a wide-column store this is normal and expected rather than a compromise.

---

## 6. Architecture, derived

### Attempt 1: the naive design

```
Client → App server → Database
```

Create: insert a row. Read: select by code, return a 302.

At 350,000 reads/second this fails immediately. Every redirect is a database round trip; the database is
sized for the read path rather than the data; and the app servers are doing nothing but forwarding.

### Attempt 2: add a cache

```
Client → LB → App server → Redis → (miss) → Database
                              ↓
                          302 redirect
```

With a 90%+ hit rate, the database sees ~35,000 reads/second at peak instead of 350,000 — a load a
sharded cluster handles without strain. The cache holds `short_code → long_url`, which is a tiny value,
so ten gigabytes covers the working set as computed above.

Cache-aside, with a long TTL: the mapping is immutable for the life of the link, so there is nothing to
invalidate except expiry and deletion. This is the rare case where cache invalidation is genuinely easy,
and it is worth saying why — immutability is what makes it easy, and it is a property of the data model,
not a lucky accident.

**Negative caching matters here more than usual.** A short-code space that is 5% occupied means most
random guesses miss, and scanners do guess. Cache 404s for a few minutes, or the miss path becomes an
amplification vector.

### Attempt 3: solve key generation

This is the real content of the question, and it is treated in full in §7.1.

### Attempt 4: get analytics off the read path

The redirect must not wait for a click to be recorded. Emit the click event asynchronously to a log and
aggregate downstream:

```
GET /{code} → cache lookup → 302 (returns here, ~5 ms)
                  └──→ Kafka → stream aggregation → OLAP store → /stats
```

The aggregation pipeline is Chapter 70's; nothing about it is specific to URLs. What matters here is the
boundary: the redirect path performs one cache read and one non-blocking publish, and nothing else.

### Final architecture

```
                          ┌── Redis (short_code → long_url) ──┐
Client ──► LB ──► Redirect service ──────────────────────► Cassandra (urls)
   │                       └──► Kafka ──► Flink ──► ClickHouse ──► Stats API
   │
   └────► LB ──► Creation service ──► Key service (block allocation)
                                 └──► Cassandra (urls, user_urls)
```

Two services, deployed and scaled independently, because their load profiles differ by two orders of
magnitude and their availability requirements differ qualitatively.

---

## 7. Deep dives

### 7.1 Generating short codes

Four approaches. The choice is the heart of the question.

#### (a) Hash the URL and truncate

`base62(md5(long_url))[:7]`.

Deterministic, so the same URL always produces the same code — which is either a feature (deduplication)
or a bug (two users cannot have separately-tracked links to the same destination, and one user can
discover that another has shortened a given URL).

**Collisions are certain.** By the birthday bound, in a 3.5 × 10^12 keyspace, collisions become likely
around ~10^6 links — far below our 180 billion. So every insert must be a **conditional write**
(`INSERT ... IF NOT EXISTS`), and on conflict you append a salt and retry. As the keyspace fills, the
retry rate climbs.

Workable, and the least elegant of the four.

#### (b) Global counter, base62-encoded

Maintain a monotonically increasing counter; encode its value.

No collisions, ever, by construction. Codes are dense, so short codes are used before long ones.

Two problems. The counter is a **coordination point** on every write — a single row, or a service, that
every creation must touch. And the codes are **sequentially guessable**: anyone can enumerate every link
in the system, including private ones, and can measure your creation rate precisely.

#### (c) Pre-generated key service with block allocation — **the recommended answer**

A key generation service owns the counter. Application instances request a **block** of keys — say 10,000
— and serve from it locally, returning to the service only when the block is exhausted.

- **No coordination on the hot path.** One remote call per 10,000 creations.
- **No collisions**, because blocks are disjoint by construction.
- **Survives key-service downtime** for as long as the current blocks last — at 1,160 writes/second across
  ten instances, a 10,000-key block lasts about ninety seconds per instance, so a brief outage is
  invisible.
- **Cost:** an instance that dies loses its unused block. At 3.5 × 10^12 codes, leaking blocks is free.
- **Guessability:** solved by encoding the counter value through a fixed keyed permutation (a Feistel
  network over the integer range, or simply XOR-ing with a secret before encoding) so that consecutive
  counter values produce non-adjacent codes. Uniqueness is preserved because the permutation is a
  bijection.

That last point is worth making: it gives you the collision-freedom of a counter and the unguessability
of a hash, which is the best of both, and most candidates do not think of it.

#### (d) Random generation with a uniqueness check

Generate 7 random base62 characters; conditionally insert; retry on conflict.

At 5% keyspace utilization, the expected number of retries is about 1.05 — negligible. Simple,
unguessable, and stateless. Its weakness is that as utilization grows the retry rate grows with it, and
it needs a conditional write on every insert.

Perfectly defensible, and a good answer to give if the interviewer wants simplicity.

#### Summary

| Approach | Collisions | Coordination | Guessable | Verdict |
|---|---|---|---|---|
| Hash + truncate | Certain, must handle | None | No | Workable, inelegant |
| Global counter | None | Every write | **Yes** | Rejected |
| **Block allocation** | None | 1 per 10k writes | No, with a permutation | **Recommended** |
| Random + check | Rare, must handle | Conditional write | No | Good, simpler |

### 7.2 The redirect: 301 versus 302

**301 Moved Permanently** is cached by browsers and intermediaries. Subsequent visits skip your servers
entirely — the cheapest possible outcome for infrastructure, and the reason it is tempting.

**302 Found** is not cached by default. Every visit reaches you.

The trade-off is stark:

- With a 301 you lose **all analytics after the first visit per browser**, and you can **never change or
  disable the destination**, because clients that cached it will never ask again. For a link shortener,
  where the ability to disable a link that turns out to point at malware is a safety requirement, this is
  disqualifying.
- With a 302 you pay for every click and retain both capabilities.

**Choose 302** for any product with analytics or moderation. Choose 301 only for a permanent, unmonitored
redirect — a domain migration, for example. Being able to explain *why* the more expensive option is
correct, in terms of a product requirement rather than a preference, is exactly the trade-off reasoning
being scored.

### 7.3 Analytics without slowing the redirect

The requirement is a sub-100 ms redirect; a synchronous write to an analytics store would add a database
round trip to every click and couple redirect availability to analytics availability.

The click event — `{short_code, timestamp, ip, user_agent, referrer}` — is published to Kafka and the
response is sent without waiting for the broker to acknowledge, or with a fire-and-forget local buffer
flushed in batches. Losing a small fraction of click events during a broker outage is acceptable; adding
latency to redirects is not. **State this trade-off explicitly** — it is a real decision, and the wrong
default (blocking on the publish) is one many candidates take without noticing.

Downstream: stream aggregation into per-code, per-minute counters in an OLAP store, with rollups to
hourly and daily. Geographic and referrer breakdowns are additional dimensions on the same aggregation.
See Chapter 70 for the pipeline in detail.

### 7.4 Custom aliases

They share a namespace with generated codes, so the same uniqueness mechanism applies — a conditional
insert, returning 409 on conflict.

Three additional concerns:

- **Reserved words.** `api`, `login`, `admin`, `health`, and anything matching an existing route must be
  rejected. Maintain the list explicitly; generated codes must avoid it too.
- **Squatting and abuse.** Custom aliases are a namespace users will compete over. Rate-limit creation and
  consider requiring an account.
- **Offensive strings.** Both custom aliases and *randomly generated* codes can spell something you do not
  want in your domain name. Production shorteners filter the generated keyspace against a blocklist. It is
  a small, real detail that shows familiarity with the actual product.

### 7.5 Expiry and deletion

Lazy deletion on read plus a background sweeper.

On read, if `expires_at` is in the past, return 410 and delete the cache entry. This costs nothing and
handles the common case. A periodic job removes expired rows to reclaim storage — necessarily a job,
because scanning 180 billion rows for expired entries on any kind of schedule is expensive; in a store
with native TTL support (DynamoDB, Cassandra), set it at write time and let the store do it.

Do not scan for expired entries synchronously anywhere on the read path.

### 7.6 Availability of the read path

The redirect path should survive the loss of components it does not strictly need:

- **Cache down:** fall through to the database. Latency rises, the system works. This means the cache must
  be a performance dependency, not a correctness one — cache-aside gives this for free, read-through does
  not.
- **Analytics pipeline down:** clicks are dropped, redirects continue. Guaranteed by the fire-and-forget
  publish.
- **Creation service down:** redirects are unaffected, because they share no components. This is the
  benefit of splitting the services, and it is worth stating as the reason for the split rather than
  presenting the split as a stylistic choice.
- **Region down:** the mapping data is immutable after creation, which makes multi-region replication
  unusually easy — there are no write conflicts to resolve. Replicate asynchronously to a second region
  and serve reads locally.

That last observation is the strongest available answer to "how would you make this globally fast":
immutability removes the hard part of geo-replication.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Redis cluster loss | Every read hits the database; ~350k QPS peak against Cassandra | Size the database for a degraded window; warm the cache after restart; consider a small in-process LRU as a second layer |
| Key service down | Creations continue until blocks exhaust (~90 s), then fail | Larger blocks; pre-fetch the next block at 50% consumption |
| Hot link (a viral link) | One cache key serves a large fraction of all traffic | Already cached; add an in-process LRU for the top entries to avoid a Redis hot shard |
| Scanner enumerating codes | High miss rate, database load | Negative caching, per-IP rate limiting, unguessable codes |
| Malicious destination | Your domain distributes malware | Blocklist check at creation, asynchronous rescan, ability to disable — which requires the 302 decision from §7.2 |

**Monitoring:** redirect p99, cache hit rate (the single most informative metric — a slow decline predicts
an incident), 404 rate (a spike means enumeration), creation error rate, key-block consumption rate.

---

## 9. Common mistakes

1. **Designing the write path for scale it doesn't have.** 1,160 writes/second is not a distributed
   systems problem. Candidates who shard the write path elaborately are demonstrating that they cannot
   size a problem.
2. **Choosing 301 for efficiency** without noticing that it forfeits analytics and moderation.
3. **Using a global counter** and not mentioning that the codes are enumerable.
4. **Recording clicks synchronously**, coupling redirect latency and availability to the analytics store.
5. **Not deriving the code length.** Stating "we'll use 7 characters" without the `62^7` calculation
   misses a free opportunity to show the estimation is real.
6. **Forgetting negative caching**, in a system whose keyspace is 95% empty.
7. **Treating collisions as impossible** in the hash approach. At 180 billion links they are certain, and
   the conditional write is the fix.

---

## 10. Variants

**Pastebin.** Same key generation and same read path; the body is large, so it goes in object storage and
the row holds a pointer. Adds size limits and visibility (public / unlisted / private). The read path may
be served directly from a CDN because the content is immutable — a strictly easier problem.

**Referral and invite codes.** Identical generation problem, much smaller keyspace, and a stronger
unguessability requirement because the code confers value. Use the permutation approach from §7.1(c).

**File sharing links.** Add expiry, single-use semantics (which reintroduces a write on the read path —
the one case where the redirect cannot be read-only), and access control.

**Deep links / attribution links.** Add device fingerprinting, deferred deep linking, and a much richer
analytics dimension set. The core mapping problem is unchanged.

---

## 11. Further reading

- Chapter 04, §1, for the ID-generation alternatives in general form
- Chapter 70, for the analytics pipeline this chapter defers to
- [system-design-primer — Design Pastebin.com / Bit.ly](https://github.com/donnemartin/system-design-primer/blob/master/solutions/system_design/pastebin/README.md)
