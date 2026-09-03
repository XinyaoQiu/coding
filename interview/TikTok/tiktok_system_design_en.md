# TikTok — System Design Crash Course (entry-level)

> Purpose: crash prep for a TikTok entry-level system design round. Written the way you'd actually say it out loud in an interview — first person, conversational, no textbook prose.
> Structure: Part 1 the answering framework (works for any question) / Part 2 building-block cheat sheet / Part 3 the question bank (10 problems, each: requirements → estimation → API → data model → architecture → tradeoffs).

---

## Agent Notes (internal rules — not interview content)

When maintaining this file:

1. **Spoken English, first person.** Write it the way you'd say it in the room ("I'd start by…", "here I'd use…"), not as a written design doc. Contractions are fine and preferred.
2. **Keep the numbers walked-out.** After a formula, say it in words — `QPS = daily requests / 86400` (daily request count divided by seconds in a day). The point is showing the reasoning, not the arithmetic.
3. **Depth = entry-level.** Explain components, data flow, and the core tradeoffs. Don't go into sharding algorithm internals, consensus protocol proofs, or the math behind CAP.
4. **Experience is a credibility signal, not the main thread.** Lead with the conventional design. Real work experience belongs in a one-line aside when the interviewer digs in. Notification / Rate Limiter / video storage are the three where it fits (subscription webhooks, UGC anti-abuse, the video upload pipeline). The Twitter following-feed problem is **not** one of them — that prior product was a content-distribution platform (mostly crawled news, follow was a weak feature), not a social network, so there was no fanout to speak of. Don't force it.
5. **Only add or remove problems when explicitly asked.**
6. **This file mirrors `tiktok_system_design.md` (Chinese).** When one changes materially, the other should follow. Neither is a literal translation of the other — both are written natively in their own language.

---

## Part 1 — The Answering Framework (use this on every question)

The worst thing you can do in a system design round is start drawing boxes and stuffing Redis and Kafka into them. Interviewers aren't counting components. They're checking whether you have a **repeatable process** — whether you can take a vague, open-ended prompt and converge it into something buildable. So my strong advice: no matter what the question is, walk the same seven steps every time. Practice them until they're automatic, and even if you're nervous walking in, you'll always know what to say next and what to draw first.

On pacing: in a 45–60 minute round, clarification + estimation + API + data model together should take about 10–15 minutes, the high-level architecture 5–10 minutes, and everything left goes into deep-diving one or two components. **Don't spread the time evenly.** The deep dive is where the score separation happens; the earlier steps exist to give the deep dive a foundation. Don't grind on them.

### Step 1: Clarify the requirements before answering anything

The first move is not designing, it's asking. The prompt is always underspecified — "design Twitter" doesn't tell you whether images and video are in scope, whether DMs matter, whether the timeline is ranked or reverse-chronological. If you don't pin that down, everything downstream is guesswork. I'd open with something like: *"Before I design anything, can we agree on scope — should this version focus on posting and reading the timeline, and leave DMs and notifications out?"* Let the interviewer draw the boundary with you.

What's being evaluated here: **do you narrow scope on your own initiative?** In real work requirements are always fuzzy, and a good engineer aligns before building. The classic beginner mistakes are skipping this step entirely, or overcorrecting and asking about product trivia ("what's the max username length?"), which signals you can't tell what matters. Ask about things that **change the architecture**, not product details.

### Step 2: Split functional from non-functional requirements

Once scope is settled I write two explicit lists on the board. **Functional requirements** are what the system *does* — users can post, users can read a timeline, users can like. **Non-functional requirements** are how *well* it has to do it — how many users, what latency, how much inconsistency is tolerable, how many nines of availability.

Why call these out separately? Because **the non-functional requirements drive every technology choice that follows**. If the interviewer says "reads massively outnumber writes, and the timeline being a few seconds stale is fine," I immediately know I can lean hard on caching, split reads and writes, and accept eventual consistency. That's an enormous signal. What's being evaluated is whether you can **translate business language into technical constraints**. The common failure is listing features only, never touching scale or latency or consistency — and then every architecture decision later is vibes, with no answer to "why did you pick that?"

### Step 3: Rough out the scale — QPS and storage

This step is about landing on orders of magnitude. Note the word *rough*: **the interviewer doesn't want precision, they want to see you're willing to estimate and that the reasoning holds up.** I usually work backwards from user count. Say 100M DAU, each posting twice a day and refreshing the timeline 50 times.

Write QPS: `write QPS = daily writes / 86400` (total daily writes divided by the number of seconds in a day). 100M users times 2 posts is 200M writes, divided by 86400 is a bit over 2000, so writes land around 2–3K QPS. Reads work the same way and come out far higher, possibly hundreds of thousands. At that point I'd say out loud: *"So the read:write ratio is roughly 100:1 — this is a read-heavy system, which means I'll be putting most of my effort into caching the read path."* That's the whole point. **You estimate in order to justify the design that follows**, not for its own sake.

Storage too: `storage = daily new records × size per record × retention days`. If a post is 300 bytes and there are 200M a day, that's tens of GB a day and tens of TB a year — which immediately tells you a single MySQL box won't hold it and sharding is on the table. Two beginner failure modes here: **skipping estimation entirely** (then what justifies sharding later?), or **getting stuck on arithmetic**, computing 86400 to the digit. Nobody cares — say "I'll round that to 100K seconds to make the math easy," and you actually gain points for knowing what to approximate.

### Step 4: Define the API

With features and scale in hand, I sketch the external API — the contract between client and backend. Posting is `POST /tweet` with `user_id` and `content` in the body. Reading the timeline is `GET /timeline?user_id=...&cursor=...` (fetch a user's timeline with a pagination cursor).

Why do the API first? Because **once the API is fixed, inputs and outputs are fixed, and the data model and architecture have something to anchor to.** I'd deliberately call out pagination here: for an ever-changing list like a timeline I use a **cursor**, not an `offset`, because with offset pagination new inserts shift everything and you end up re-showing or skipping items. Interviewers like that detail — it signals you've shipped something. The beginner mistake is a sloppy API, or skipping it entirely and jumping to boxes, which leaves you unable to say what actually flows between components.

### Step 5: Design the data model

Now decide what's stored and where. I list the core tables or collections — users, posts, follow relationships — with key fields and primary keys for each. Then **pick storage using the numbers from Step 3**: structured, transactional data goes in MySQL; if the volume demands sharding, I say explicitly **what the shard key is** — e.g. shard posts by `user_id`, so one author's posts live together and are cheap to query. Anything read-hot, like a celebrity's timeline, gets a Redis layer in front.

What's evaluated: **whether your storage choice has a reason**, and whether you know the basic SQL/NoSQL tradeoff — relational when you need strong consistency and transactions, NoSQL when you need write throughput and flexible schema and can live with eventual consistency. At entry level you don't need consistent hashing internals; "data got big, so shard it, pick a key, and a bad key creates hotspots" is the right depth. The classic mistake is opening with "NoSQL because it's fast" and then having nothing when asked why. **Every choice needs a tradeoff behind it, not a slogan.**

### Step 6: Draw the high-level architecture

*Now* you draw boxes. Start at the client and go all the way down: client → load balancer → a pool of stateless app servers → database, cache, and a message queue for async work. Narrate the data flow as you draw: *"User posts, request hits the load balancer, lands on some app server, the server writes the post to MySQL and drops an event on Kafka so a downstream consumer can update followers' timelines asynchronously."*

One thing I always emphasize: **app servers must be stateless** — no session data held locally — so any server can handle any request and you can scale horizontally by just adding machines. What's evaluated here is whether you **hold a complete data flow in your head** and can stitch the earlier steps into something that actually runs. The common failure is an overcrowded diagram — rate limiting, monitoring, CDN, service discovery all on the board at once, so tangled you can't narrate it yourself. Keep the high-level view **clean**. Get the trunk right; details belong in the deep dive.

### Step 7: Deep-dive one or two components, then find bottlenecks

This is the main event and where most of the points are. Once the architecture is on the board I'd volunteer: *"The most interesting part of this design to me is timeline generation — I'd like to go deeper there."* Then pick one or two real difficulties and go deep. For timelines: is it computed **at read time** (fanout-on-read / pull) or **pushed to all followers at write time** (fanout-on-write / push)? I'd lay out the tradeoff — push makes reads fast but a celebrity posting once means millions of writes; pull makes writes cheap but reads have to merge on the fly. Then offer the hybrid: push for normal users, pull for celebrities. **Naming the tradeoff and offering a middle path is what a strong answer looks like.**

After the deep dive, go hunting for bottlenecks: what breaks first? Usually database reads or a hot key. Then match each to a remedy — reads failing means cache and read replicas; writes failing means sharding; single points of failure mean replication plus failover. What's evaluated: **can you find your own design's weak spots and answer for them?** That's the senior/junior line. The common failure is not being able to go deep — one sentence per component and then sliding off, or freezing when asked "why is that slow?" The fix is to prepare a few classic deep dives in advance (timeline generation, hot keys, consistency, rate limiting) and steer toward the ones you know, so you keep control of the conversation.

---

One closing principle: the seven steps are really about **going from vague to concrete, with each decision justified by the previous step**. Estimation drives selection, selection drives architecture, architecture exposes bottlenecks, bottlenecks drive scaling. It's a causal chain, not seven isolated moves. If you keep saying that chain out loud — *"because the read:write ratio is 100:1, I'm caching the read path"*, *"because it's tens of TB a year, posts have to be sharded"* — the interviewer hears someone designing a system, not reciting a template. That's worth more than any number of component names.

---

## Part 2 — Building Blocks (interviews are just assembling these)

After enough of these you realize the same dozen-ish components keep showing up in different combinations. Get clear on what each one *is*, when you reach for it, and where the traps are, and you'll stop freezing at the whiteboard. Here they are, roughly the way I'd say them out loud.

### Load balancer

A load balancer sits between clients and a pool of servers and spreads incoming requests across them so no single box gets buried. When do I reach for it? Essentially any time the service is more than one machine and I want horizontal scaling — without it the client has no idea which box to talk to. It also gives you health checking for free: a dead server gets routed around automatically.

The basic tradeoff worth mentioning is round-robin versus sticky routing. Plain round-robin (first request to A, second to B, third to C) is trivial to implement, but if servers hold user session state, the same user's next request lands somewhere else and their login is gone. So the trap is: either make servers **stateless** (push sessions out to Redis so any box can serve anyone) or make the load balancer do **session affinity** (pin a user to one box). In an interview I lean toward the first, because stateless scales better.

### Cache (usually Redis)

A cache puts hot, frequently-read data somewhere very fast — typically Redis, in memory — so requests check the cache first and never touch the database on a hit. When? Any time reads dominate writes and the same data gets read repeatedly: popular articles, user profiles. It takes an enormous load off the database and cuts latency.

The core tradeoff is staleness: cached data can drift from the database, so I need an expiry story, usually a TTL. There are three classic failure modes interviewers love, one line each. **Cache penetration** is querying keys that don't exist in the database at all — every request misses and hits the database (fix: cache the negative result too, or put a bloom filter in front). **Cache avalanche** is a large batch of keys expiring at the same instant and every request stampeding the database at once (fix: add random jitter to TTLs so they don't all expire together). **Cache stampede** (or "breakdown") is one extremely hot key expiring and a flood of concurrent requests all going to the origin simultaneously (fix: a mutex so exactly one request repopulates while the others wait). The names blur together; the way I keep them straight is: penetration = the key never existed, avalanche = a whole batch dies at once, stampede = one hot key dies.

### SQL (MySQL) vs NoSQL (MongoDB)

This comes up almost every time. I don't recite definitions, I talk about when I'd pick each. Relational databases are strong when the schema is fixed, there are real relationships between tables, I need **transactions** (a set of operations that all succeed or all fail — a transfer debiting one account and crediting another), and I need strong consistency. Orders, balances, payments — anything where being off by a cent is unacceptable — go in MySQL.

NoSQL is strong when the schema is flexible and changes often, when a single document holds a big nested blob, and when I need easy horizontal scaling and high write throughput. Behavioral logs, feed content, comments — high volume, loosely structured, no cross-table transactions — is where I'd consider MongoDB.

The one-line summary I'd give: MySQL trades scaling difficulty for consistency and join power; MongoDB trades strong consistency and rich queries for flexibility and scale. So I look at two things — how relational is the data, and do I need transactions. Yes → MySQL. No, and the volume is huge → NoSQL. I'd also point out that mixing them is extremely common in real systems: core financial data in MySQL, high-volume peripheral data in MongoDB.

### Sharding + consistent hashing in one line

When a single database can't hold the data or can't take the write load, I shard — split one big table by some key (say `user_id`) across many machines, each holding a slice. When? When volume or write throughput exceeds what one box can do, and adding read replicas doesn't help because the problem is writes.

The naive approach is modulo: `shard = hash(key) % N`. The fatal flaw is that the moment N changes — you add a machine or lose one — nearly every key maps somewhere new, so effectively all the data has to move. That's the rehash disaster. **Consistent hashing** fixes it: picture the hash space as a ring, map both machines and keys onto it, and a key belongs to the first machine clockwise from it. Adding or losing a machine only disturbs the neighboring arc, so the vast majority of data stays put. Any time sharding and resizing comes up, this is the answer to reach for. In practice you also use **virtual nodes** — each physical machine occupies many points on the ring — so the distribution stays even and one unlucky hash position doesn't create a hotspot.

### Message queue (Kafka): buffering, decoupling, async

A queue sits between producer and consumer: the producer drops a message and moves on, the consumer picks it up at its own pace. It solves three things, and these are the three words I keep in my head. **Buffering** — traffic spikes (a flash sale) queue up instead of crushing downstream services. **Decoupling** — upstream doesn't need to know who consumes or wait for them, and if one side goes down the other keeps running. **Async** — work that doesn't need to block the response (sending a notification, an email) gets handed off so the main path can return to the user immediately.

The tradeoff is that adding a queue turns a synchronous operation into an eventually-consistent one: it's no longer "done now," it's "will be done shortly." So I have to accept the delay and handle duplicate delivery and ordering. "Show the updated balance immediately after a transfer" is a bad fit; "send an SMS after checkout" is a perfect one.

### CDN

A CDN puts caching nodes all over the world and pushes static assets — images, video, CSS, JS — to nodes near the user. When? When you serve a lot of static content to a geographically spread audience. The wins are direct: users fetch from a nearby node so latency drops, and that traffic never reaches your origin, so origin load drops sharply.

The tradeoff mirrors caching generally: CDN content can be stale — you update an image at the origin and edge nodes still serve the old one. The standard fix is to put a version or content hash in the filename (`main.a1b2c3.js`), so changing the content changes the URL and the CDN naturally treats it as a new object. CDNs are for content that doesn't change often; don't put per-user dynamic responses behind one.

### Replication and read/write splitting

This is how you scale reads on a database. One **primary** takes writes, several **replicas** receive a copy of every change. Then you split traffic: all writes go to the primary, reads spread across replicas. When? The classic read-heavy case, where a single database can't keep up with reads — adding replicas is a far smaller change than sharding.

The tradeoff and the trap is **replication lag**: there's a delay between the write landing on the primary and appearing on a replica, anywhere from milliseconds to hundreds of milliseconds. That produces the classic bug — a user posts a comment (write to primary), immediately refreshes (read from a replica that hasn't caught up), and their own comment is missing. The fix is to route read-after-write cases to the primary and let everything else hit replicas. I'd raise this proactively; it shows you've thought it through.

### Bloom filter

A bloom filter is a very memory-efficient structure that answers "definitely not present" or "possibly present." You don't need the internals; the property that matters is: it can falsely say **present** when the item isn't, but it will never falsely say **absent**. That asymmetry is structural and **cannot be flipped** — insertion sets several hash positions to 1 and bits only ever go from 0 to 1, so "any bit is 0" definitively proves absence, while "all bits are 1" might just be other elements happening to cover those positions. If you want false negatives instead, you need a Counting Bloom Filter or Cuckoo Filter — but those introduce them as a side effect of supporting deletion, not as a way to reverse the error direction. The classic use is the cache penetration case above: ask the filter first, and if it says the key definitely doesn't exist, return immediately without touching the database, which shuts down attacks that query nonexistent keys. Crawler URL dedup is the other standard use.

The tradeoff is exactly that false-positive property: the memory savings come from giving up precision, and the cost of a false positive is one wasted database query — never a wrong answer. Also, a standard bloom filter supports insertion but not deletion. So I frame it as **a cheap pre-filter, not an authoritative existence check**. Blocking most of the junk is all it needs to do.

---

## Part 3 — The Question Bank

### Problem 1 — TinyURL

**Clarifying requirements**

I'd pause before drawing anything and pin down scope. On functionality: the core is taking a long URL and producing a short one that redirects back. That's the trunk. Then a few extensions worth asking about — do we support custom aliases (letting a user pick `tinyurl.com/mypromo`)? Do short links expire? Do we need click tracking and analytics? Each of those changes the data model, so I want to know what's MVP and what's a bonus.

On non-functional requirements I'd stress four things. First, this is a textbook **read-heavy** system: creating links is rare, following them is constant, and the ratio might be 100:1 or worse — that judgment alone tells me caching is the centerpiece. Second, **low latency**: a redirect has to feel instant, tens of milliseconds. Third, **high availability**, because if this service dies every short link ever issued breaks — links people have already shared stop working — so availability matters more than strong consistency here. Fourth, **no collisions**: two different long URLs must never map to the same code.

**Scale estimation**

Let me anchor on a number: say 10 million new links a day. Then `write QPS = 10,000,000 / 86400` (daily writes over seconds per day) ≈ 115 QPS — writes are trivial. Reads at 100:1 give `read QPS = 115 × 100` ≈ 11,500 QPS, and I'd budget 2–3× for peaks, so design for around 30K QPS.

Storage: assume a 5-year horizon. `total links = 10,000,000 × 365 × 5` ≈ 18.2 billion records. At roughly 500 bytes each (short code, long URL, timestamps, owner), `total = 18.2B × 500B` ≈ 9 TB. That's too much for one machine but nowhere near needing hundreds — good to know, and it tells me the data layer needs sharding eventually.

One more calculation that matters: **how long does the code need to be?** I'd use base62 (0-9, a-z, A-Z — 62 characters, far denser than decimal). Seven characters gives `62^7` ≈ 3.5 trillion combinations, comfortably above the 18.2 billion I need, so 6–7 characters is plenty.

Worth a sentence on **why base62 and not base64**: base64's alphabet includes `+`, `/`, and `=`, all of which have special meaning in URLs — `/` is a path separator, `+` decodes to a space in query strings. Dropping those into a URL path either breaks the link or forces escaping. base62 is alphanumeric only, so it's URL-safe by construction. Base64 is denser, but only by about 25% for the same length, which isn't worth the encoding headaches.

**API design**

Two endpoints. Creating: `POST /api/v1/urls` with `long_url` in the body, optional `custom_alias` and `expire_at`, returning the short URL. Redirecting: `GET /{short_code}` — not really a REST endpoint we designed, just what the browser hits, and we respond with a redirect.

There's a tradeoff I'd raise unprompted: **301 vs 302**. A 301 is a permanent redirect, so the browser caches it and subsequent clicks never reach us — cheap, but we **lose click tracking** and we're stuck if the mapping ever needs to change, because browsers have cached it. A 302 is temporary, so every click comes back to us: one extra hop, but we can record it. If analytics are in scope, 302. Pure performance with no tracking, 301. I default to 302 because the analytics are usually worth more.

**Data model**

Genuinely simple — one table: `short_code` (primary key or unique index), `long_url`, created_at, expires_at, owner id, click count. The interesting part is picking the store.

The access pattern is exceptionally clean — almost every operation is a primary-key point lookup by short code, no joins, no transactions — and the volume needs sharding. So I lean **NoSQL**: a key-value store, or Mongo/Cassandra, with the code as key and the URL plus metadata as value, which scales horizontally by key naturally. If the interviewer prefers relational, MySQL works fine, you just shard by code hash yourself. I'd be explicit that I'm choosing NoSQL for horizontal scaling and high-throughput point lookups, not because there's anything structurally complex here. Read-heavy plus primary-key lookups is exactly where cache + NoSQL is the least trouble.

**High-level architecture**

Creation path: request hits the load balancer, lands on an app server, the server generates a unique short code, writes the code→URL mapping, and returns the assembled short URL.

Redirect path: request goes through the load balancer to an app server, which **checks the cache first** (Redis). Because reads dominate and popular links get hit repeatedly, the hit rate is very high — on a hit we return the redirect without touching the database at all. On a miss we query the database, backfill the cache with a TTL, and redirect. The vast majority of reads never reach storage.

Scaling further on the read side, I could put a CDN or edge layer in front and cache the hottest mappings closer to users.

**How to generate the short code** is the real difficulty here, and there are two routes. **Hashing**: hash the long URL (MD5), take the first few bits, base62-encode. Stateless and simple, but hashes **collide** — two different URLs can produce the same code, so you check the database before writing and, on collision, salt and retry. **A ticket server**: maintain a globally incrementing ID, assign one per request, base62-encode it. **Collisions are impossible by construction**, but a global counter is awkward in a distributed setting and a single allocator becomes a bottleneck. The fix is segment allocation — hand each machine a block of IDs (say 10,000 at a time) that it increments locally and refills when exhausted, so machines never contend and don't take a lock per request. I prefer the ticket server with segments, because it eliminates the collision problem entirely; the cost is a component to maintain.

**Key tradeoffs and bottlenecks**

**301 vs 302** — covered above; performance versus analytics, and I default to keeping analytics.

**Hash vs ticket server** — hashing is simple and stateless but needs collision handling; the ticket server never collides but needs a globally unique ID source, which I'd de-risk with segment allocation.

**How much the cache matters.** Performance here lives or dies on the cache. Reads dominate, hot links exist, so Redis hit rates get very high and the database only sees creates and misses. Consistency between cache and database is usually a headache, but short links have a lovely property: **the mapping essentially never changes** once created. So read-through backfill plus a TTL is enough; no dual-write consistency machinery needed.

**Database scaling** — 9 TB doesn't fit on one box, so sharding. With NoSQL, sharding by code is natural; with MySQL you partition by code hash yourself. I'd frame this as something you grow into.

**Availability** — issued short links are permanent assets, so the service can't go down. Redundancy at every layer: stateless app servers behind a load balancer, database replicas, Redis in a replicated or clustered setup. I'd prioritize availability and latency over strong consistency throughout — this is not a use case that needs it.

---

### Problem 2 — Rate Limiter

**Clarifying requirements**

I wouldn't jump into algorithms. First, what are we limiting and how? On functionality: what's the limit keyed on — user ID, IP, API key? What does a rule look like ("100 requests per user per minute")? What happens on exceed — reject with HTTP 429, or queue? Do rules need to be configurable and hot-reloadable so thresholds change without a deploy?

Non-functionally: **low latency**, because this sits in front of every request and must never become the bottleneck — sub-millisecond ideally. **High availability**, which leads to a fail-open versus fail-closed discussion I'll come back to. And **accuracy**: counting correctly across many machines is the real difficulty here. I'd say upfront that we usually accept "approximately right" rather than never letting one extra request through. That was the thinking on a UGC anti-abuse system I built — for anti-spam, letting a few extra through isn't fatal, but latency and availability are non-negotiable, so it was fail-open. (If pressed, there's a good lesson there: it initially misfired on internal scripts, and we changed it to observe-only. The principle I took away — **a decision with a false-positive rate shouldn't have blocking authority; anything with blocking authority has to be deterministic.**)

**Scale estimation**

Say a mid-sized service at 1 billion requests a day. `average QPS = 1,000,000,000 / 86400` ≈ 11,500. But a rate limiter's enemy is the peak, not the average, so I'd budget 5× — `peak ≈ 11,500 × 5` ≈ 57,500 QPS, call it 60K.

Storage: one counter per active user, say 10M active users. Each record is a key plus an integer plus an expiry — call it 100 bytes. `10,000,000 × 100B` = 1 GB. Worth noting **why 100 bytes and not 4**: the integer itself is tiny, but the key string, the Redis object header, the hash table entry and pointers, the TTL timestamp, and allocator alignment all add up — real per-entry cost is usually an order of magnitude above the raw value. Anyway, 1 GB fits in a single Redis instance easily, and these counters are short-lived with TTLs so no persistence is needed. The conclusion: **the bottleneck isn't capacity, it's the ~60K QPS of read-modify-write traffic.**

**API design**

There's really one operation, usually a middleware call: `allowRequest(userId, apiName)`. I'd return more than a boolean — allowed/denied plus **remaining quota and time until reset**. At the HTTP layer a rejection is a 429 with `X-RateLimit-Remaining` and `Retry-After` headers, so the client knows how long to back off instead of hammering. Rule configuration goes through a separate interface or a config service, kept off the runtime decision path.

**Data model**

What I'm storing is "how many times has this key been used in this window." The key is something like `userId:apiName:windowId`, the value is a count. I'd pick **Redis** for three reasons: it's in-memory so reads and writes are sub-millisecond, which is what a rate limiter needs; it has native **TTL**, so counters expire on their own and I don't write cleanup code; and it has atomic operations (`INCR`) plus Lua scripting so I can make read-check-write a single atomic step. I would not use a disk-based database here — it can't take the QPS.

**High-level architecture**

Request hits the load balancer, then our limiter, which I'd implement as middleware on an API gateway or as a layer in front of the service. The middleware builds the key from user and endpoint, does one atomic increment in Redis, and compares against the threshold: under, pass through to the real service; over, return 429 immediately so the backend never sees it. Rules live in a config service, loaded at startup and refreshed periodically, so thresholds change without a restart.

Worth a sentence on **where it lives**. Client-side is unreliable — you don't control the client and it can be modified, so it's a UX nicety at best. Inside each service works but duplicates the logic everywhere. **Middleware on the API gateway** is the usual answer: one implementation applied uniformly, and rejected traffic never touches your services.

The key point is **why a shared Redis instead of per-machine counting**. Backends are multiple machines behind a load balancer, so one user's requests land on different boxes. If each counts independently — 10 machines each allowing 100 — the user effectively gets 1000 and the limit is meaningless. Centralizing the counter is the whole basis of distributed rate limiting.

One caveat, because it cuts the other way for aggregate limits: **the key's cardinality decides where the state lives.** A per-user limit is high-cardinality, shards naturally, and centralizing is right. A *global* limit — "100 QPS across the whole fleet" — is a single key by definition, so every request contends on it: a guaranteed hot spot and a single point of failure sitting in front of everything. That one belongs in local memory, each instance holding 1/N of the budget, zero round trips. Approximate, but the goal there is protecting the downstream, not counting exactly — nginx's `limit_req` is per-worker-process for the same reason. So the rule isn't "always centralize," it's **centralize what has an identity, keep aggregate limits local**.

**Key tradeoffs and bottlenecks**

**Algorithm choice** is the classic part. **Fixed window** is simplest — one counter per clock minute — but has a boundary flaw: at 100/minute, a user can send 100 at second 59 and 100 more at second 61, so 200 in a two-second span. **Sliding window** looks at a rolling "last 60 seconds" instead of clock-aligned buckets, which smooths the boundary at the cost of more state. In practice the common implementation isn't storing every timestamp — that's the precise version and it's expensive — but keeping two fixed-window counters and interpolating between them, which gets you most of the smoothing for two integers. **Token bucket** refills tokens at a fixed rate and each request consumes one, and because tokens accumulate up to a cap it **tolerates bursts** — quiet periods build credit that a sudden spike can spend. **Leaky bucket** admits requests into a queue that drains at a fixed rate, deliberately flattening traffic and allowing no bursts, which suits protecting a downstream with fixed throughput.

My default: **token bucket** for user-facing API limits, because normal users do burst (double-tapping, a page issuing several calls) and shouldn't be punished for it. Leaky bucket when the goal is protecting a downstream at a strict rate. I'd mention fixed window mainly to dismiss it — the boundary is exploitable.

Two things worth knowing about how token bucket actually behaves, because they're counterintuitive. First, **the bucket is normally full**, not empty — it's initialized full and any user whose rate is below the refill rate never notices the limiter exists. Second, when the bucket does empty, requests are **rejected outright, not delayed** — there's no queue. Users experience scattered failures that recover quickly, not a steady drip. If you want the "steady drip" behavior, that's leaky bucket.

Also worth separating from all of this: **a long-window quota** — "no more than 40 video uploads per user per 24 hours" — is a different problem, not a rate limit. It's a counting problem, the volume is tiny (40 records), and precision matters more than throughput. For that I'd keep timestamps in a sorted set — specifically a sorted set because `ZREMRANGEBYSCORE` is the only Redis command that deletes a whole time range in one call, where a list would need a loop and therefore Lua — and count the window exactly, or just query the business table with an index on `(user_id, created_at)`. Token bucket's burst allowance is meaningless there.

**Atomicity** — two requests from one user arrive simultaneously; if I read, check, then write, there's a race where both read 99 and both pass. The fix is Redis atomic commands or a Lua script that makes the whole sequence indivisible. I always call this out explicitly.

**Fail-open vs fail-closed** — what happens when Redis is down or slow. Fail-open allows requests through, so normal users are unaffected but limiting is disabled and an actual attacker gets through. Fail-closed rejects, which is safe but harms a lot of legitimate users and can cascade. No universal answer: if the limiter exists to protect the backend from being crushed, I lean fail-open — the limiter breaking shouldn't take the whole service with it. If it's a security control (payment fraud), fail-closed. On anti-abuse I chose fail-open, availability first, and accepted catching up after the fact.

**Latency vs accuracy** — every request making a network round trip to shared Redis adds up. You can count locally per machine and sync periodically, which is much faster but briefly inaccurate. That's trading precision for latency and throughput, and whether it's acceptable depends on the business.

**Scaling** — when one Redis can't take 60K QPS, shard by user ID hash with consistent hashing so adding nodes moves few keys. Rate limiting shards beautifully because counters are independent per key and one user always lands on one node, so nothing is computed across nodes. Hot users can make a node hot, which you'd handle by special-casing very large keys, but "shard horizontally, partition by key" is the right depth here.

One more thing worth being honest about: **in real backend code, `INCR` + `EXPIRE` shows up far more often than token bucket.** Most business cases don't need burst tolerance, and two Redis commands is a lot less to build and maintain than a Lua script with clock handling. Token bucket lives mostly in gateways and CDN layers, or gets pulled in as a library (Go's `golang.org/x/time/rate`), rather than being hand-written per service.

**Monitoring** — I'd close on this because it's what separates a limiter that works from one that's merely deployed. Two things to watch: how much traffic is being rejected, and whether it's the traffic you meant to reject. A limiter that never fires is misconfigured; one that fires on legitimate users is worse than having none. That's exactly what drove the observe-only change on the anti-abuse system.

---

### Problem 3 — Design Twitter (following feed)

**Clarifying requirements**

I'd start by pinning down which feed we mean, because that word covers two completely different architectures. A **following feed** (Twitter's Following tab, Facebook's News Feed) has a content pool defined strictly by who you follow, and the hard part is aggregating a few hundred sources efficiently — that's the fanout push/pull tradeoff. A **recommendation feed** (Twitter's For You, TikTok's For You) draws from the entire corpus, and the hard part is a retrieval-and-ranking funnel where fanout doesn't apply at all. So I'd ask: *"Are we focusing on the following feed or the recommendation feed?"* If it's my choice, I'd take the following feed — that's the classic version of this problem.

Going with the **following feed**. Functional requirements: users post; users read a home timeline aggregating everyone they follow; follow/unfollow; the feed paginates infinitely, a batch at a time.

On ordering I'd **assume pure reverse-chronological first**, because that's what makes the fanout tradeoff legible. Real products rank by score; I'll come back to that in the deep dive — it changes the read path but not the fanout layer.

I'd proactively scope out two things. **Likes, bookmarks, retweets, comments** — storage is a join table with an index, counters are Redis increments flushed asynchronously, and there's no interesting architectural tradeoff. Also, a Twitter reply *is* just a tweet with an `in_reply_to` pointer, so it reuses the same post and fanout path; no separate comment system needed. And **a user's own profile timeline** — that's one indexed query by `user_id`, not a difficulty. The hard problem is the home timeline, which spans hundreds of authors, and that's what fanout exists to solve.

Non-functional requirements matter a lot for feeds. **Low latency** — under 200ms on refresh. **High availability** — if the feed is down the app is a blank screen, and availability matters more than "posting succeeded"; failing to show old content is far worse than failing to accept a new post. **Eventual consistency is fine** — someone I follow posting and it appearing a few seconds later is completely acceptable.

**Scale estimation**

One thing to state before the numbers: **this problem needs two different user counts.** Databases size against **total registered users and cumulative data**, because inactive users' history still has to be stored. The Redis inbox sizes against **active users**, because there's no point maintaining an inbox for someone who never reads. Those differ by several times and mixing them up produces wrong answers.

Assumptions:

```
Total registered users = 50M
DAU = 10M
New tweets per day = 5M (0.5 per user per day)
Reads: each user views their own timeline once + 3 other profiles, 10 items per page
Average following count = 200
```

**QPS:**

```
Writes = 5M / 86400 ≈ 58 QPS
Reads  = 10M × (1 + 3) × 10 = 400M views/day
       = 400M / 86400 ≈ 4,630 QPS
Peak at 3× ≈ 14K QPS
Read:write = 400M : 5M = 80:1
```

The thing to stress: **reads aren't just timeline loads.** Users also open profiles and individual tweets, and those are reads too. Counting only the timeline badly understates the ratio. 80:1 is in line with the ~100:1 Twitter has publicly described.

That ratio is the whole justification for doing extra work at write time to make reads fast — the intuition behind the push model.

**Post storage** (cumulative):

```
A tweet with metadata ≈ 10KB (the text is only 280 chars, but there's the id,
author, timestamps, counters, index overhead; media goes to object storage as a URL)
Daily = 5M × 10KB = 50 GB/day
Yearly ≈ 18 TB, five years ≈ 90 TB
```

**Follow graph** (by total users):

```
50M users × 200 follows = 10B rows × ~16 bytes ≈ 160 GB
```

**Inbox (Redis)** — sized separately and on a different population:

Each inbox entry stores only locating information, not content: `tweet_id` 8 bytes + `author_id` 8 bytes + a byte of flags ≈ **17 bytes per entry**. The `author_id` is carried so the read path can fetch tweet content and author info **concurrently** instead of serially — otherwise you'd have to load the tweet first just to learn who wrote it.

```
Inbox sizes against users active within the TTL window. TTL of 30 days means MAU, not DAU.
MAU is typically 2-3× DAU, so 10M DAU → 25M MAU.

25M × up to 800 entries × 17 bytes ≈ 340 GB raw
Redis real footprint (object headers, skiplist pointers, allocator alignment)
at 2-3× ≈ 700GB - 1TB
```

**That number only means something once you convert it to machines**, so I'd do that out loud:

```
At 64GB per box, budgeting 70% usable (leaving headroom for fork and fragmentation):
~45GB usable per box
1TB / 45GB ≈ 23 shards
Plus one replica each for HA → ~46 machines
```

So the inbox is a **twenty-something-shard, forty-something-machine** Redis cluster sharded by user ID. Converting terabytes into machine count matters — it turns "this part is expensive" into a concrete number you can negotiate about.

And those two parameters are **adjustable knobs**: if the interviewer says the budget is 10 machines, the first things I'd cut are the TTL (30 days → 7) and the per-user cap (800 → 200), which drops memory to a couple hundred GB immediately. Shorter TTL and fewer entries save memory but mean more rebuilds; the reverse saves rebuilds and costs memory. That's a real tradeoff, not a fixed number.

For contrast: post storage is 18 TB a year on disk, the inbox is ~1 TB in memory. **The inbox is an order of magnitude smaller but it's the expensive part** — memory costs an order or two more per GB than disk, so the actual spend is comparable. Every inbox optimization that follows (TTL, truncation, skipping inactive followers, not fanning out celebrities) exists to shrink that number.

**API design**

Thin — three endpoints. Posting is `POST /v1/posts` with content in the body; identity comes from the token, never the body, for obvious security reasons. Reading is `GET /v1/feed?limit=20&cursor=xxx` — deliberately a cursor, not a page number. Why: the feed is constantly changing, so offset pagination duplicates or skips items (new content is inserted at the head, everything shifts, and the next offset re-serves what you just saw). Offsets also degrade linearly — the database scans and discards every skipped row, so deep pagination gets progressively slower. A cursor (the last item's id or timestamp) is an absolute position, unaffected by insertions and deletions, and stays a single index seek no matter how deep you go. The cost is you can only page sequentially, not jump — and a feed doesn't need jumping. Following is `POST /v1/follow`.

The response returns **a batch of complete, renderable objects** plus a `next_cursor` — not a list of ids. One clarification on `limit`: it's **a batch, not a screenful**. The client pulls 20–30 at a time and silently prefetches the next batch when the user scrolls to within a few items of the end, so scrolling is seamless instead of spinning every screen. Batch size is a tradeoff: too small and the prefetch window is too tight and requests are too frequent; too large and the first screen is slow and you waste bandwidth on users who bounce. Bigger batches also mean longer gaps between requests and more feed churn in between, which is another argument for cursors over offsets.

**Data model**

**For post storage I'd pick MongoDB**, and the reasoning ties directly to the numbers above: 18 TB a year shards fine, and native sharding means no middleware to build; writes are 58 QPS, which is nothing; and the query pattern is point lookup by tweet id and range scan by user id, with no joins or cross-document transactions.

If asked "why not Cassandra" — Cassandra's strength is the masterless architecture giving enormous write throughput and local writes across data centers. At 58 QPS that capability is unused, and you'd pay for it with severely limited querying: Cassandra essentially only queries by partition key, so each access pattern needs its own table and the data gets denormalized several times over. If DAU were one or two orders of magnitude higher and writes were in the tens of thousands per second, Cassandra becomes the better answer. **Selection follows write volume and access pattern, not total data size.**

The user table can be anything — stable schema, only 50M rows, MySQL is fine. The follow graph is its own table of `(follower_id, followee_id)` with indexes in both directions: forward for "who do I follow," reverse for "who follows me" (needed at fanout time). 10B rows is about 160 GB, shards fine. At larger scale it's fundamentally a graph, but an indexed table is sufficient here — no need to reach for a graph database.

Then the critical piece — the **inbox** (also called feed cache or home timeline), in a Redis cluster sharded by user ID.

I'd use a **Sorted Set**, not a List:

```
ZADD "inbox:{userid}" {publish_timestamp} {tweet_id + author_id + flags}
ZREVRANGE "inbox:{userid}" 0 29             # newest 30
ZREMRANGEBYRANK "inbox:{userid}" 0 -801     # keep only newest 800
EXPIRE "inbox:{userid}" 2592000             # 30-day TTL, refreshed on read
```

Two reasons for Sorted Set over List. **Pagination**: the cursor is a timestamp, and a sorted set can range-query by score directly, while a list can only index positionally — which is offset pagination again. **Idempotency**: members are unique, so a fanout retry writing the same entry twice doesn't duplicate, and deleting a tweet is a direct removal by id. As for insertion being O(log n) rather than O(1) — at 800 entries that's about ten pointer hops, completely swamped by the network round trip, so it isn't a real factor in the decision.

**What goes in**: only tweet id, author id, and flags — **never the text, author name, or other display content**. Three reasons: storing content once versus hundreds of thousands of times is an enormous memory difference; when a tweet is edited, deleted, or its counters change, copies scattered across hundreds of thousands of inboxes are impossible to keep in sync; and content already has its own cache layer keyed by tweet id, shared by everyone. The `author_id` is the exception — it never changes, and it's denormalized purely to save a serial round trip, not as a content cache.

**The TTL is mandatory.** Without it, keys accumulate forever with registered users — 50M registered users means 50M keys pinned in memory permanently. With a 30-day TTL refreshed whenever the user reads their feed, memory converges to "users active in the last 30 days." That TTL also solves a second problem: at fanout time, check `EXISTS` first, and if the key is absent this follower hasn't been around in a month, so skip the write entirely — **which cuts write amplification substantially.**

Note that TTL and length truncation are **two independent dimensions**: TTL controls how many inboxes exist, `ZREMRANGEBYRANK` controls how big each one is. You need both.

**High-level architecture**

Write path: user posts, request goes through the load balancer to the Post Service, which persists to post storage first (source of truth, non-negotiable). On success it emits a "new post" event to Kafka and returns to the user — **no fanout in the synchronous request**. A Fanout Service consumes from Kafka, looks up the author's followers, and writes the entry into each follower's inbox. That's fanout-on-write.

Read path is light: request to the Feed Service, three steps — `ZREVRANGE` 30 entries of (tweet_id, author_id) from Redis, then **concurrently** batch-fetch tweet content and author info (each through its own cache, hitting storage only on a miss), then assemble complete objects and return. Because the inbox was precomputed at write time, a read is one Redis call plus two parallel batch lookups.

One thing to be explicit about: **the id list only circulates server-side; the client receives fully renderable objects.** I wouldn't hand the client a list of ids to resolve itself — that's an extra public-internet round trip (tens to hundreds of ms) where the server-to-Redis hop is intra-datacenter (single-digit ms), an order of magnitude difference, and it leaks the storage layout to the client.

Whether to return **full content or a summary** depends on payload size. A tweet is 280 characters, so returning the full text in the timeline costs nothing (Twitter's public API does exactly this by default). For heavy content — long-form articles, video — return a summary and fetch detail on tap; the deciding factor is payload size multiplied by the non-click rate, since users only open 5-10% of what they scroll past. Note that even when full text is returned, tapping into a detail view usually still fires a request, because the detail view has replies and conversation context that don't belong in a timeline payload.

**Key tradeoffs and bottlenecks**

**First, push or pull** — the heart of the problem. What I described is push (fanout-on-write): reads are extremely fast, but write amplification is severe since one post writes into every follower's inbox. Pull (fanout-on-read) does nothing at write time and merges the latest posts from everyone you follow at read time — cheap writes, no wasted storage, but reads compute on the fly and get slow when you follow hundreds of people. The tradeoff is symmetric: push spends storage and write cost to buy read latency; pull does the reverse. Because feeds are read-heavy (the 80:1 above), the default leans push.

**Second, the celebrity problem** (the hot-key case), which is push's fatal flaw. An account with tens of millions of followers posts once and fanout does tens of millions of writes, most of them for people who won't open the app today. The industry standard answer is a **hybrid**: normal users get push, celebrities get pull — their posts aren't fanned out, and at read time the feed service separately fetches recent posts from the handful of celebrities you follow and merges them with your precomputed inbox. Twitter's published threshold is around **10,000 followers** (the exact number varies by product; what matters is explaining why there's a threshold at all).

Worth mentioning **outbox** (a user's own posts) here. By default I **wouldn't maintain one** — viewing someone's profile is `WHERE user_id = ? ORDER BY created_at DESC`, one indexed range scan, and keeping a separate copy just creates a consistency problem. The one case that justifies Redis is the celebrity pull path: every follower loading their feed has to fetch that celebrity's recent posts, so the same small result gets read enormously — a textbook hot key. So cache it for celebrities only, 5-10 minute TTL, fall back to the table on a miss.

The distinction between inbox and outbox is worth one sentence: **the inbox is a required materialized view** (without precomputation the feed can't be read fast), **the outbox is just a hot cache** (it can always be recomputed from the table). The test is the cost of losing it — losing an inbox means walking hundreds of follow relationships to rebuild, losing an outbox means one indexed query. They're not symmetric, don't treat them as such.

**Third, controlling inbox memory.** That's ~1 TB across twenty-something shards, the most expensive component, so every memory lever gets pulled at once: the 30-day TTL refreshed on read keeps memory on active users; `EXISTS` at fanout skips inactive followers; truncation at 800 entries with deeper pagination falling back to pull (almost nobody scrolls that far); and celebrities aren't fanned out. All four exist to push down that 1 TB.

**Fourth, how to shard post storage** — three options, each with a problem. Sharding by **user ID** keeps an author's tweets together and makes profile queries fast, but celebrities create skew and hotspots. Sharding by **tweet ID** (hashed) distributes most evenly, but "recent tweets by this user" has to **query every shard and merge**, which is slow. Sharding by **creation time** makes recent content fast to fetch but concentrates all traffic on the newest shard while older ones sit idle. I'd take **user ID sharding with celebrity special-casing** — the dominant access pattern is by user, and the skew problem can reuse the celebrity identification the hybrid fanout model already needs.

**Fifth, graceful degradation**, which I'd raise proactively because the following feed has a peculiar constraint. If the ranking service dies, that's the easy case — the inbox is already stored by timestamp, so returning it in reverse-chronological order is a fully functional feed, just lower quality. That's a structural advantage the following feed has over a recommendation feed: it has a natural fallback ordering. If an inbox read fails, fall back to computing a pull once — slower but correct, and that path already has to exist for rebuilding inactive users' inboxes, so it's reused. But if Redis fails at scale, the pull fallback can't absorb it either — everyone pulling at once would flatten the database — so at that point it's rate limiting plus serving stale data.

One thing I'd emphasize: **a following feed must never degrade into recommended content.** A recommendation feed that fails can backfill with trending content and users won't notice. But the following feed's product contract is "show me what the people I follow posted," and mixing in content from accounts they don't follow breaks that contract. Its degradation axis is quality and freshness, never the content set.

**Sixth, ranking.** I described chronological ordering for clarity, but real products rank by score — relevance, recency, predicted engagement. I'd split ranking into its own stage: retrieval (pull candidates from the inbox), then ranking, then truncate to a batch. That decouples retrieval from ranking so models can iterate without touching storage.

There's a point that's easy to confuse: **scoring happens at read time, not write time.** The sorted set's score is the publish timestamp, whose only job is making "fetch the most recent N candidates" efficient — it is not a ranking score. The actual ranking happens on the read path: pull far more candidates than you'll display (500 candidates to show 30), score them live, take the top 30. Why can't you score at write time? Because **scores are per-viewer** — the same tweet has completely different relevance to different followers, and at publish time you don't know who's going to see it — and scores decay with time and shift with engagement counts, so anything computed at write time is stale within minutes.

Incidentally, the inbox still needs to be ordered even though ranking overwrites the order, because "fetch the most recent 500" depends on ordering. **Retrieval sets the ceiling on ranking** — content that isn't retrieved can't be ranked into the result no matter how good the model is.

Further extensions worth naming: two-stage ranking (coarse then fine), deduplication, filtering already-seen content.

**If the interviewer wants the recommendation feed (For You)**

Then fanout and the inbox are entirely inapplicable — recommendation results can't be precomputed, because their whole value is incorporating the latest behavioral signals. The architecture becomes a multi-stage pipeline: multi-source retrieval (people you follow, collaborative filtering, content similarity, trending, geographic) → coarse ranking to cut volume → fine ranking to score → blending for diversity → return. Followed content is just one retrieval source here, not the whole pool.

The evaluation criteria shift accordingly, and I'd go in four directions: **latency budget** across stages; whether retrieval sources can run **concurrently** and what happens when one times out (drop that source and continue, don't fail the whole request); **degradation** when the recommendation service is entirely down (trending content as a floor — a blank feed is never acceptable); and **what can and cannot be cached** — results can't, but user profiles, content features, and content detail all can, and drawing that distinction is itself a strong signal. Ranking model internals aren't a system design question; I'd say plainly that I treat that as a black-box service call.

**Closing**

On scalability, the bottlenecks are in three places: the inbox's ~1 TB of memory across forty-something machines is the hardest one, addressed by sharding the Redis cluster on user ID; the Fanout Service is stateless and scales by adding consumers behind Kafka's buffering; post storage under read pressure gets read replicas and multi-level caching. The through-line is the same sentence throughout — read-heavy, so push complexity toward the write path and the async path, and keep the read path touching memory only.

If there's time, I'd close with the evolutionary direction in one sentence: **once the following feed also adopts ranking, its architecture converges with the recommendation feed's** — both become retrieval plus ranking, and the inbox demotes from "the answer" to "one retrieval source among several." Which is why real products tend to keep a purely chronological, unranked Following tab: the product contract of following someone is "show me their posts," and any algorithm in that path breaks the promise. That's exactly why Twitter now ships For You and Following as two separate tabs.

---

### Problem 4 — Notification / Push System

**Clarifying requirements**

I wouldn't start drawing. "Notification system" is a broad term and a few things need settling first.

Functionally: which channels? Usually at least three — mobile push (APNs and FCM), SMS (Twilio and similar), and email. I'd assume mobile push is the focus since it's the most representative and the hardest. Then: who triggers a notification? Another backend service ("someone you follow posted"), or an operator blasting a segment (marketing)? Those differ by orders of magnitude and I'd estimate them separately. I'd also confirm **templates** — most notifications aren't free text but a title/body/deep-link structure that varies by locale and A/B bucket, so callers should pass a template id plus variables, not a rendered string. And **user preferences**: can users mute categories, set quiet hours? I'd treat that as required.

Non-functionally, four things. **Delivery with latency tolerance** — a few seconds late is fine, but **duplicates are not** (two identical pushes is a terrible experience), so dedup and idempotency are hard requirements. **High availability** — the upstream service triggering a notification must never be blocked by us, so the two sides need decoupling. **Burst absorption** — a marketing blast is millions of messages at once while APNs and FCM have rate limits, so we have to flatten that spike. **Observability** — for any notification I need to answer whether it was sent, whether the provider rejected it, and whether the user tapped it.

**Scale estimation**

Say 100M DAU, 5 notifications per user per day: `100M × 5 = 500M` notifications a day.

`Average QPS = 500M / 86400` ≈ 5,800. But averages are meaningless here because marketing blasts are spiky, so I'd budget 10-20×: `peak ≈ 5,800 × 20` ≈ 116,000 QPS. That spike is precisely why there has to be a queue in the middle.

Storage, two pieces. Device tokens: users have multiple devices, say 1.5 average, so `100M × 1.5 = 150M` token records at a few hundred bytes each — tens to low hundreds of GB, not a concern. Delivery logs, if we retain every send for audit: `500M × 200B = 100GB/day`, which is ~3 TB a month, so I'd keep only recent days hot and roll the rest to cold storage or expire by TTL.

**API design**

One simple interface upstream, so callers never think about APNs versus FCM. Core is `POST /v1/notifications` with `user_id`, `template_id`, `variables`, optional `channel` (omit it and the system decides from user preferences), and critically an `idempotency_key`. The caller generates that key; retries of the same logical event carry the same key, so we can recognize "already handled" and return the previous result instead of sending twice.

Bulk sends get their own endpoint, `POST /v1/campaigns`, taking a segment definition or user list plus a template, returning a `campaign_id` asynchronously, with `GET /v1/campaigns/{id}` for progress. Bulk is separate from single-send because it needs batch processing and much stricter rate limiting — a different path entirely.

Plus two supporting endpoints: `GET/PUT /v1/users/{id}/preferences` for notification settings and quiet hours, and `POST /v1/devices` for clients to register their APNs/FCM token at startup.

**Data model**

Several stores, chosen by access pattern.

**Device tokens** go in a key-value or document store (MongoDB, or Redis as a cache layer). The access pattern is almost entirely "give me a user id, return all their tokens" — a point lookup with no complex relationships, so a relational database isn't needed. Fields are user_id, token, platform, last_active, and an invalidation flag. The trap here is that **tokens expire** — after an uninstall, APNs tells you the token is invalid, and you need a reclamation flow to delete it or you'll push to dead tokens forever.

**User preferences** go in MySQL — fixed volume, structured fields, and queries like "has this user muted marketing." One row per user, or a few rows split by category. Read-heavy, so a Redis cache in front absorbs almost all reads.

**Templates** also in MySQL: template_id, locale, channel, title template, body template. They change rarely, so I'd cache the whole set in Redis or even in-process and render variables into them.

**Delivery logs** are the highest-volume, most write-heavy piece, so not relational — either a wide-column store like Cassandra, or straight to Kafka and then a warehouse. Queries are by notification_id or user_id, with a TTL.

**Idempotency records** go in Redis keyed by `idempotency_key` with something like a 24-hour expiry, the value being the processing result. Dedup checks hit Redis first and return immediately on a hit.

**High-level architecture**

Walking the whole path.

At the front is a **Notification API / ingest service** that upstream services and the operator console call. It does very little: validate, check the idempotency key (if it's already in Redis, return immediately without going further), drop the request onto **Kafka**, and return 202. That's where decoupling and burst absorption happen — upstream never waits for actual delivery, the spike is buffered in Kafka, and downstream consumes at its own pace.

Behind Kafka are **notification workers**. A worker pulls a message, checks user preferences (has this category been muted, is it currently quiet hours — if so drop or defer), looks up device tokens, renders the template with the variables, and sends to the appropriate channel.

One design point I'd emphasize: **workers are split into physically isolated pools per provider** — one for APNs, one for FCM, one for the SMS gateway — not one big pool that branches by platform. That buys three kinds of independence: **independent scaling** (FCM being busy doesn't touch SMS), **independent failure** (APNs going down doesn't drag other channels with it), and **independent tuning** (push wants second-level delivery while email tolerates minutes, so their timeout and retry policies should differ anyway). Workers scale horizontally, with Kafka partition count as the parallelism ceiling.

**How delivery to providers actually works** is worth detail, because it's counterintuitive. APNs and FCM are both **HTTP/2 long-lived connections**, and production systems maintain a **persistent connection pool** (dozens of connections held open) and multiplex over it, rather than establishing a connection per notification — connection setup and TLS handshake cost far more than the push itself. So **rate limiting here is fundamentally about controlling connection pool concurrency**, not adding a per-second counter in application code. FCM's published ceiling is 100K/sec per project; APNs doesn't publish a hard number and you tune concurrency empirically.

**Failures must be classified first — this is the most important step:**

- **Permanent failures** (invalid token, app uninstalled, payload over APNs' 4KB limit) — **never retry**, they'll fail identically every time. Invalid tokens get flagged and enter the reclamation flow, or you'll push to dead tokens indefinitely.
- **Transient failures** (rate limited, 5xx, timeout) — these retry, with **exponential backoff and mandatory jitter**. Worth saying out loud: a few thousand workers all retrying on the same fixed interval is indistinguishable from a DoS attack on your own provider, and jitter is what spreads them out.

After N attempts a message goes to a **dead letter queue** for human inspection — parked to the side, never lost, never blocking the healthy traffic behind it.

On whether to do **retries beyond the minute scale**, I'd lean no — push notifications are time-sensitive, and a notification delivered half an hour late has little value anyway. For notifications whose value doesn't decay (payment confirmations, order status), long retries do matter, and there I'd use a Redis sorted set as a delay queue (score = execution timestamp) or switch to RabbitMQ's TTL plus dead-letter exchange — rather than improvising tiered topics on Kafka. Kafka has no delayed consumption primitive, so improvising is awkward, and sleeping on a message inside a worker prevents offset commits and eventually trips `max.poll.interval.ms`, which marks the consumer dead and triggers a rebalance.

Delivery and click receipts coming back from providers are ingested the same way — a webhook landing on a queue — and update the delivery log. End to end, every component is decoupled by Kafka, so a slow or dead segment never drags the upstream down.

**Key tradeoffs and bottlenecks**

**Deduplication and idempotency** is the first hard problem. Retries are normal in distributed systems: upstream resends on timeout, and Kafka consumption is at-least-once so the same message can be processed twice. I'd guard in two places: the ingest layer checks the idempotency key against Redis to absorb upstream resends, and the worker checks a dedup key again before actually calling the provider. That costs an extra Redis lookup, but not double-sending is a hard requirement for notifications, so it's worth it.

Worth being honest about the limits, though: **worker-side dedup can't be made strict.** Checking Redis and calling APNs are two separate systems with no atomic operation spanning them — marking before sending means a crash in between silently drops the notification, marking after means a crash causes a duplicate. For push I'd take the second (better duplicated than lost) and lean on the layers that can be strict: the ingest-layer idempotency key, and on the delivery side `apns-collapse-id` / FCM `collapse_key`, which lets the platform itself collapse equivalent notifications on the device. It's the same key threaded through all of them — the ingest layer enforces it strictly, the worker best-effort, and the platform and client as a final net.

**Burst absorption and backpressure.** Peaks reach six figures of QPS, and Kafka in the middle exists for exactly that — upstream writes freely, downstream consumes at whatever rate providers accept. The tradeoff is real-time delivery given up for stability, and a marketing push arriving tens of seconds late is entirely acceptable, whereas firing directly at the provider guarantees rate limiting or worse.

**Fan-out**, mostly in the bulk-send case. An operator pushing to 10 million users can't be expanded synchronously in one request. So the ingest layer only queues the campaign definition, and a dedicated pool of fan-out workers expands it into 10 million individual tasks and pushes them back onto the queue, expanding and consuming concurrently so memory and latency stay bounded.

**Provider unreliability** — APNs and FCM rate limit, time out, and invalidate tokens. The failure classification, jittered exponential backoff, dead letter queue, and token reclamation above are the complete answer to this. One thing to flag: **retries only work if you have idempotency**, or the retry mechanism itself becomes the duplicate-send mechanism. Those two are inseparable — which is also why the ingest-layer idempotency key isn't only for the caller's benefit; the worker's retry path depends on it too.

**Priority isolation**, if there's time to extend. Transactional notifications (login codes, "you received a payment") matter far more than marketing pushes, and a login code must never queue behind a marketing blast. So different priorities get different Kafka topics — a dedicated express lane for high priority so it's insulated from bulk spikes. In practice this is often the thing that finally breaks the user experience, so I'd bring it up unprompted.

On whether the priority split should also mean **separate worker pools**: with a shared pool, "consume high priority first" only holds at the moment of polling — once low-priority messages occupy processing slots (especially stuck in retry backoff), high-priority work still waits behind them. Full isolation means separate pools. The reason I'd still keep priority as a topic split within one per-provider pool is that **rate limits are enforced per provider** — splitting APNs into two pools means either two connection pools (doubling the concurrency the provider sees, blowing past your tuned level) or a shared one (which isn't isolation at all). So provider isolation is mandatory and structural, priority isolation is a business decision applied within it.

---

### Problem 5 — Chat / IM

**Clarifying requirements**

I'd start on functional boundaries. Are we doing one-on-one only, or group chat too? I'd assume both with 1:1 first and groups as an extension. Presence indicators ("online," "last seen")? Read receipts? History retrieval? Rich media? I'd scope the core to: send, receive in real time, fetch history, presence, read receipts, group fanout. For rich media I'd say the message body carries only a URL, with the file itself in object storage behind a CDN, traveling the same channel as text — and leave it there.

Non-functionally, three words for IM. **Low latency** — end to end within a few hundred milliseconds; users are extremely sensitive to chat lag. **High availability** — reconnect automatically on drop, never lose a message. **Ordering and no duplicates.** On consistency I'd say IM generally accepts eventual consistency, but **ordering within a conversation is non-negotiable** — messages must not arrive out of the order they were sent.

One question I'd ask early because it changes the architecture fundamentally: **does the server retain message history?** If multi-device sync and history search are required, the server must store messages long-term and storage becomes the dominant estimate. If it's an end-to-end encrypted product, the server is only a delivery relay and deletes after delivery — much less storage, but switching devices loses history. WhatsApp and Signal are the latter; Telegram, Slack, and Discord are the former. I'd design for **server-side storage**, since multi-device sync is essentially table stakes now.

**Scale estimation**

Say 50M DAU, 40 messages per person per day: `50M × 40 = 2B` messages a day.

`Write QPS = 2B / 86400` ≈ 23,000. Reads run higher because a group message fans out to many recipients and history gets fetched, so I'd estimate 5-10× writes — call it 100K-200K read QPS.

Connection count matters here. WebSocket connections are held open, not request-and-done. If concurrent online is 20% of DAU: `50M × 20% = 10M` concurrent connections. A tuned box handles maybe 500K, so `10M / 500K = 20` machines just for the connection layer. That number is what motivates the "connection layer is its own tier" design later.

Storage: at ~300 bytes per message including metadata, `2B × 300B ≈ 600 GB/day`, so over 200 TB a year. That tells me immediately that messages need a horizontally scalable store partitioned by conversation and time — a single MySQL box is out of the question.

**API design**

Real-time send and receive over WebSocket, not HTTP polling. Once connected, sending is a frame pushed into the socket and receiving is a frame pushed down it. The core operations: `connect(userId, token)` establishing an authenticated long-lived connection; `sendMessage(conversationId, clientMsgId, content)` — note the `clientMsgId`, generated client-side for idempotency so a retry after a network blip is deduplicated server-side; `markRead(conversationId, lastReadMsgId)`; and history over plain HTTP with `GET /conversations/{id}/messages?before=cursor&limit=50`, deliberately off the socket since it isn't real-time.

There's also the conversation list, which is what loads when the app opens: `GET /v1/conversations?limit=20&cursor=xxx`, returning each conversation with the peer, the last message preview, and the unread count.

**Data model**

Messages: message_id, conversation_id, sender_id, content, created_at, and a sequence number `seq`. Where they go matters: write volume is enormous, writes are append-only per conversation, and updates are rare — so a wide-column / LSM store like Cassandra or HBase, with `conversation_id` as **partition key** and `seq` (or timestamp) as **clustering key**. That means one conversation's messages sit physically together, ordered, so fetching the latest 50 is a single efficient range scan. Why not MySQL: messages are write-heavy, append-oriented, and ordered within a conversation, which is precisely this family's strength, and partition-key sharding comes free.

**Conversation membership** is a separate store — MySQL or MongoDB, with a Redis cache — holding per-user, per-conversation state: `last_read_seq`, `unread_count`, `updated_at` for sorting, mute and pin flags. This is what the conversation list reads. It's separate from messages deliberately: messages are append-only and enormous, while this state is small and **frequently updated**, and frequent updates are exactly what an LSM store handles worst.

Note this state is materialized rather than computed. Deriving the last message and unread count by scanning the messages table on every app open would be far too slow, so sending a message updates each member's row (last message, `unread_count`, `updated_at`) and the read path is one indexed query.

Presence and the "which gateway is this user connected to" mapping go in Redis: key is userId, value is the gateway id, with a TTL refreshed by heartbeat. Fast-changing, latency-sensitive, cheap to rebuild — Redis is the right fit.

Read receipts are expressed as a per-user, per-conversation `last_read_seq` watermark rather than a flag per message, which saves enormous storage — whether a given message is read is a sequence comparison.

**High-level architecture — the data flow**

Following one message. User A sends; it hits the load balancer, then the **connection gateway** tier (the WebSocket holders, those ~20 machines). The gateway doesn't write storage — it hands off to the chat service, which does three things: assign a `seq`, write to the message store, and update conversation state (last message, unread counts).

Then delivery to B. The key point is that A and B are almost certainly on different gateways. So Redis (or a dedicated session registry) records which gateway B is on. The chat service looks it up, finds B on gateway-7, pushes the message to gateway-7 over an internal channel, and gateway-7 writes it down B's WebSocket. If B is offline (no live connection in Redis), nothing is pushed — the message is simply stored, and B picks it up on next login via the conversation list's unread state.

A message pushed down the socket carries `conversation_id` alongside the content, and the client routes on it: if the user is currently viewing that conversation, append and scroll; otherwise update that row in the conversation list — preview text, unread badge, move to top. Those are local client-side updates, no extra request needed. But local state drifts (multi-device, disconnection, app restart), so **the server is authoritative** and the client re-syncs the full conversation list on reconnect. Read state also has to sync across devices: reading on desktop means the server pushes a `read_receipt` event to that user's other connections so the phone clears its badge. Which is why the socket carries more than new messages — read receipts, recalls, typing indicators, membership changes — and why frames need a `type` field.

I'd put Kafka between the chat service and delivery. After the write, "who needs this delivered" goes onto Kafka and a downstream delivery service consumes it, handling live pushes or falling back to APNs/FCM. That decouples the connection tier from delivery logic, absorbs bursts, and makes multi-device delivery natural (one person on phone and desktop gets it pushed to both connections).

**Key tradeoffs and bottlenecks**

**Message ordering** is the first hard problem. I wouldn't rely on wall-clock time across machines, since clocks aren't perfectly synchronized and timestamp ordering would scramble. Instead the chat service assigns a monotonically increasing `seq` **per conversation**, and both clients and storage sort by it, guaranteeing that within a conversation what you sent first appears first. Cross-conversation global ordering isn't needed — nobody compares message order between two unrelated chats — which is a meaningful architectural win: no global sequence allocator, no single point, just an independent counter per conversation that shards naturally.

**Group fanout amplification.** One-on-one is trivial — one message, one delivery. For groups, the thing being amplified is worth being precise about: **the message itself is always stored exactly once**, under its conversation partition; there is no per-recipient copy. What amplifies is **conversation state** — a 10,000-member group means updating 10,000 membership rows for unread counts. So the optimizations target that: for large groups, stop maintaining a counter and derive unread as `conversation_max_seq - my_last_read_seq` at read time, which eliminates the write amplification entirely; skip inactive members; or just display "99+" and stop tracking precisely above a threshold.

**Horizontal scaling of a stateful connection tier.** Gateways hold state (which users are attached), so they can't be restarted casually and scaling needs care. My approach is externalizing the user→gateway mapping to Redis so a gateway is as close to "just a socket holder" as possible; when one dies, clients reconnect elsewhere and re-register. The load balancer needs graceful draining for long-lived connections. This is also where **NLB versus ALB** matters: long-lived connections want an L4 load balancer — lower latency since it doesn't parse HTTP, higher connection capacity, and no idle-timeout surprises.

**Reliable delivery and dedup.** Networks are flaky, so: clients attach a `clientMsgId` for server-side idempotency; the server requires an ack for delivery and re-pushes without one; and because re-pushes can duplicate, receivers dedupe on message_id too. Read and delivery receipts flow back over the same channel.

**Presence cost**, if there's time. Pushing accurate real-time presence to every contact is genuinely expensive — the state changes constantly and the fanout is large. The standard move is degrading it: coarse "last active" instead of live status, or fetching presence on demand when a conversation is opened, so presence fanout never becomes the system bottleneck.

**Cold storage for history.** With 200 TB a year, hot data doesn't stay hot. Messages older than a few months get archived — packed by conversation and time window into compressed files in object storage, not one object per message (the request cost of billions of tiny objects exceeds the storage cost). Retrieval is 1-3 seconds including fetch and decompress, acceptable because scrolling that far back is rare, and worth noting you wouldn't sink this to something like Glacier Deep Archive where retrieval takes hours. If global history search is required, that needs a separate Elasticsearch index spanning both hot and archived messages, since archived files can't be searched in place.

---

### Problem 6 — Distributed KV Store / Cache

**Clarifying requirements**

I'd separate two related but distinct things first: a **persistent KV store** where data must survive (DynamoDB, or Redis used as a primary store) versus a **cache** in front of a database where losing data just means re-fetching. I'd ask which one we're building, since it determines how much I emphasize durability and replica consistency. I'll default to "a KV store that can also serve as a cache," which covers the most ground.

Functionally it's three operations: `put(key, value)`, `get(key)`, `delete(key)`, plus optionally a TTL, which matters a lot in the cache case. I'd assume values are opaque bytes — no structured queries, no search by value, no range scans. A KV store is for exact lookups by key; it isn't a database.

The non-functional requirements are what this problem is really testing. **Low latency** — single-digit milliseconds. **High availability** — losing a machine can't take the system down. **Horizontal scalability** — growth is handled by adding machines, not rearchitecting. And the classic tradeoff: which side of consistency versus availability do I favor? I'd say KV stores typically choose **AP** — prioritize availability and partition tolerance, relax to eventual consistency — because the usual workloads (sessions, caches, counters) tolerate slightly stale reads but not unavailability.

**Scale estimation**

Say 100M DAU with 100 KV operations each per day: 10 billion requests daily. `QPS = 10B / 86400` ≈ 115K average. Real traffic isn't flat, so peak at 3-5×: `peak ≈ 115K × 5` ≈ 500-600K QPS. That's well past a single machine, so sharding is inherent.

Read:write I'd assume around 10:1, typical for cache-like systems, which tells me to optimize the read path and add replicas to spread reads.

Storage: 1 billion keys at ~1KB each is `1B × 1KB = 1TB` raw. But that's one copy — with 3× replication for availability, `1TB × 3 = 3TB` actual. Too much for one box and not safe there anyway, which reinforces sharding. Both estimates converge on the same conclusion: single-machine isn't viable on either QPS or capacity.

**API design**

Minimal, around those three operations: `PUT /keys/{key}` with the value in the body and an optional TTL parameter; `GET /keys/{key}` returning the value or 404; `DELETE /keys/{key}`. I'd note that production KV stores don't use HTTP but a leaner binary protocol (Redis's RESP), because HTTP header overhead is heavy relative to a few dozen bytes of value — but REST expresses the semantics clearly enough for an interview.

One detail I'd raise: I'd make `put` idempotent, so writing the same value repeatedly produces the same result and client timeout retries are safe. For concurrency control you'd add a version number (compare-and-set: the write only lands if you present the version you read), but that's beyond entry level.

**Data model**

The model itself is trivial — key to value plus a bit of metadata like TTL and version. The interesting question is what backs it.

Within one machine I need fast lookup by key. If everything's in memory (the cache case), a hash table gives near-O(1) get and put. If it needs to persist and exceed memory, an **LSM-tree** engine like RocksDB, which is write-friendly — writes go to memory and an append-only log first, then batch-flush to disk — giving very high write throughput. That's why so many KV stores (Cassandra, DynamoDB internals) are built on it. I'd make the selection explicit: read-heavy but writes must be fast, values are unstructured, no joins or range queries — using MySQL here wastes everything relational databases are good at while failing to handle the QPS.

**High-level architecture**

Requests hit a load balancer or routing layer whose job is sending each request to the right machine. The core question is: with many machines, which one owns a given key? That's what sharding answers.

The naive approach is `shard = hash(key) % N`, which has a fatal flaw: change N (add a machine, lose one) and nearly every key's mapping changes, meaning a full data migration — a disaster in production. So **consistent hashing**: picture the hash space as a ring, hash both machines and keys onto it, and a key belongs to the first machine clockwise. Adding or removing a machine only disturbs the adjacent arc. I'd add that in practice you use **virtual nodes** — each physical machine occupies many ring positions — so distribution stays even and one unlucky position doesn't create skew.

Each shard isn't a single machine but a replica set, typically one primary with followers or three peers. Writes go to the primary and replicate asynchronously or semi-synchronously; reads can come from any replica, spreading read load, which fits the read-heavy profile. This is where the consistency tradeoff lands: allowing replica reads means possibly reading a stale value — eventual consistency — while strong consistency requires reading the primary or a quorum, costing latency and availability. I'd say explicitly that I choose eventual consistency here, because availability and latency matter more for a KV store or cache.

If this is being used as a cache in front of a database, the read path is: check cache, return on hit; on miss, fetch from the database, write it back, and return — **cache-aside**. The write path is typically write to the database, then delete the cache key so the next read repopulates, avoiding long-lived inconsistency.

**Key tradeoffs and bottlenecks**

**The cost of resharding.** Consistent hashing minimizes how much data moves, but migration still has to happen carefully and online — you can't stop serving to rebalance. That's the core distributed difficulty in this problem.

**Hot keys.** Consistent hashing gives roughly even key distribution but can't help when one key is enormously popular (a viral piece of content). That single key overwhelms one machine. Remedies: extra replicas for hot keys, a small client-side local cache absorbing some of it, or splitting the key into suffixed sub-keys spread across nodes.

**The cache trio**, which is almost always asked. **Penetration** — many requests for keys that don't exist anywhere, so nothing ever caches and everything hits the database; fix by caching the negative result with a short TTL, or a bloom filter in front. **Stampede** — one hot key expires and concurrent requests all go to the origin at once; fix with a mutex so exactly one repopulates while the rest wait, or never expire hot keys and refresh them in the background. **Avalanche** — a large batch of keys expiring simultaneously, or the cache cluster failing entirely, dumping everything on the database; fix by jittering TTLs so expirations spread out, plus making the cache tier itself highly available.

**Eviction under memory pressure** — I'd use **LRU**, evicting whatever hasn't been touched longest. The implementation is a hash table plus a doubly linked list: the hash table gives O(1) lookup, the list maintains recency, each access moves a node to the head, and eviction removes from the tail — constant time for both. If the workload includes periodic full scans that would flush the cache, LFU-style variants help, but LRU explained clearly is the right depth here.

**Replication and failure handling**, if pushed further: primary election after a failure, whether asynchronous replication loses writes, cross-datacenter latency. I'd acknowledge these exist and note that going deeper means replication protocols, but the core idea stays the same — trade consistency against availability according to what the business needs.

---

### Problem 7 — Web Crawler

**Clarifying requirements**

Scope first. Is this crawling the whole web like a search engine, or a fixed set of sites? Do we extract content and build an index, or just fetch and store pages? HTML only, or images, PDFs, video? Do we support recrawling to detect updates? I'd typically scope it to: given seed URLs, follow links outward, store the HTML, feed newly discovered links back into the queue, and support later recrawls.

Non-functionally: **scale** (how many pages in what time frame, which sets throughput); **politeness** — we must not knock over the sites we crawl, must respect robots.txt, and must not hammer one domain; **scalability** to a distributed fleet; **fault tolerance** so a dead machine doesn't lose work; and **deduplication**, both of URLs and of identical content. Politeness versus throughput is the constant tension here.

**Scale estimation**

Say a billion pages in one month. `QPS = 1B / (30 × 86400)` ≈ 386, call it 400 pages per second, and 2-3× for peaks, so design for around 1000.

Storage: at ~500 KB of HTML per page, `1B × 500KB` = 500 TB. That's raw HTML alone — add parsed structures and historical versions for recrawls and I'd budget in the petabytes, which means object storage, not a database.

Bandwidth: `400 QPS × 500KB` ≈ 200 MB/s, close to 1.6 Gbps of ingress, which by itself proves this has to be distributed across many machines.

**API design**

This isn't really a user-facing API, but there are internal contracts worth naming. Externally you might expose `submitSeeds(urls[])` returning a jobId, and `getCrawlStatus(jobId)` for progress.

Internally the two operations that matter are dequeuing a URL from the **URL frontier** and enqueuing newly discovered links back into it. Everything revolves around the frontier — it's the heart of the system.

**Data model**

Four pieces. The **URL frontier** itself, the pending queue, backed by Kafka or Redis — something with high-throughput enqueue and dequeue — holding URLs plus metadata like priority, discovery time, and domain. Not a relational database, because this is queue semantics at high frequency.

The **dedup structure**. Seen URLs go through a bloom filter, which is tiny in memory and extremely fast, at the cost of a small false-positive rate. A billion URLs in a normal hash set would be tens to hundreds of GB; a bloom filter might be a few.

The **raw fetched pages** go to object storage like S3 — petabyte-scale blobs keyed by URL hash, cheap and effectively unbounded.

The **metadata store** holds per-URL crawl status, last fetch time, HTTP status, and a content hash. The access pattern is structured and the volume is large, so a shardable store — Cassandra, or partitioned MySQL. The content hash lives here to enable content-level dedup: two different URLs returning byte-identical content (mirror pages with tracking parameters) are detectable by hash comparison, so we don't store duplicates.

**High-level architecture**

Seeds go into the frontier. **Fetchers** are a pool of workers continuously pulling URLs. The first thing a fetcher does isn't fetching — it's checking that domain's robots.txt, via a dedicated module that downloads and caches robots rules and determines whether this URL is allowed and what the crawl delay is. That's the core of politeness.

Past the robots check, the fetcher resolves DNS and issues the HTTP request. DNS deserves a mention because resolution is frequently a bottleneck, so I'd add a **DNS cache** rather than resolving on every fetch.

Fetched HTML goes to object storage as a raw copy and simultaneously to the **parser**, which does two things: extract all outlinks, and hash the content for dedup. Each extracted link goes through **URL normalization** first (unify http/https, trailing slashes, casing, strip irrelevant parameters — otherwise one page produces a dozen URL variants), then gets checked against the bloom filter: seen or not? Unseen goes back into the frontier; seen is discarded. That closes the loop and the system feeds itself.

The whole thing is distributed: fetchers and parsers are stateless workers that scale horizontally; the frontier is a queue that natively supports many consumers; dedup and metadata are shardable stores. A dead machine means its work is still queued for another worker — fault tolerance falls out naturally.

**Key tradeoffs and bottlenecks**

**Politeness versus throughput** is the real difficulty. I need 1000 QPS aggregate while never hammering any single domain. The answer is that the frontier can't be one flat queue — it's partitioned into per-domain logical queues, with a scheduler enforcing a minimum interval between fetches to the same domain (per robots' crawl-delay) while allowing high parallelism *across* domains. High aggregate throughput, gentle per site. I think this is what the problem is actually testing.

**The dedup tradeoff.** First I'd establish that the bloom filter's **error direction is structural and can't be reversed**: insertion sets several hash bits to 1, bits only go up, so any zero bit proves the URL is unseen, while all-ones might just be other URLs happening to cover those positions. So **false positives are unavoidable and false negatives can't happen** — that asymmetry is fixed, unless you switch to a Counting Bloom Filter or Cuckoo Filter, and those introduce false negatives as a side effect of supporting deletion, not to flip the error direction.

In crawler terms, a false positive means a **missed page**: wrongly judged as seen, skipped, never fetched. So there are three levels of solution:

1. **Bloom filter alone** — cheapest and fastest, at the cost of a tiny number of missed pages. Fine for most cases; crawling the whole web was never about completeness.
2. **Exact database dedup alone** — nothing missed, but slow and expensive, one network query per URL at tens of thousands of URLs per second.
3. **Both layered (my preference)** — if the filter says "unseen," crawl immediately, because that negative is definitive; only when it says "seen" do you pay for one exact database check. Over 99% of queries resolve in memory and only a small fraction costs a query, so **nothing is missed and performance holds**.

So it isn't "filter or database" — a bloom filter was never meant to be the final authority, it's a cheap pre-filter. You'd only degrade to option 1 or 2 if memory genuinely can't hold the filter, or if missed pages truly don't matter.

**Crawler traps and infinite spaces.** Some sites generate unbounded URLs — a calendar you can page forward forever, or deliberate link loops. I'd add limits: max pages per domain, max URL depth, max links extracted per page, so one site can't consume the crawler.

**Scaling and hotspots.** At tens of billions of pages, the frontier and dedup structures become bottlenecks, addressed by sharding on domain hash. The hotspot is that huge sites (news portals) contribute enormous link volume and can overload one shard, handled either by giving large sites dedicated quota and queues or by throttling in the scheduler.

**Incremental recrawling.** Pages change, so one fetch isn't the end. Each URL gets a recrawl interval — frequently-updated pages often, static pages rarely — scheduled off the last-fetch time in the metadata store plus a priority policy. Further out, you'd predict recrawl intervals from observed change frequency.

---

### Problem 8 — Typeahead / Search Autocomplete

**Clarifying requirements**

Typeahead means every keystroke in the search box returns live suggestions — typing "new" surfaces "newsbreak," "news today," "new york weather." Functionally: given a prefix, return the top-k suggestions, k typically 5 or 10; ordered by popularity; refreshing on every keystroke.

Boundary questions: do we support mid-word matching ("york" matching "new york"), or prefix only? At entry level I'd assume prefix-only for simplicity and revisit if there's time. Spelling correction, personalization? I'd mark those nice-to-have and build the trunk first.

The non-functional requirements are the soul of this problem. **Latency must be very low** — a response per keystroke, and past ~100ms it feels laggy, so my target is 50-100ms end to end. Then high availability and high concurrency, since search is a major entry point. And finally, **data is allowed to be stale** — a term that becomes popular today appearing in autocomplete hours or even a day later is completely fine. That "staleness is acceptable" point is crucial, because it's what permits an offline update path.

**Scale estimation**

QPS first. Say 10M DAU, 10 searches each per day, 5 keystrokes per search, one request per keystroke: `10M × 10 × 5 = 500M` requests a day.

`QPS = 500M / 86400` ≈ 5,800 average, and peak at 2-3× gives `≈ 17,000`. That tells me reads are enormous and reads and writes are wildly asymmetric — reads are constant, writes (popularity updates) are rare.

Storage: say 100M unique queries, ~20 bytes of text plus popularity and pointers, call it 50 bytes: `100M × 50B = 5GB` raw. But a trie carries pointer overhead well above raw text, so I'd multiply 3-5× and estimate 15-25GB of memory. That fits on one large machine, though I'd still shard and replicate for availability and concurrency.

**API design**

One endpoint: `GET /suggestions?prefix=new&limit=10`, returning a popularity-ordered list. I'd stress that it's stateless, horizontally scalable, and cacheable.

There's also a popularity update path, but it isn't a synchronous API — it's log collection. Each actual search submission emits an event to Kafka, and an offline job consumes those to compute popularity. **Read path and write path are entirely separate**, which I'd call out explicitly.

Worth noting the response isn't a bare string array in production. Each suggestion is an object carrying the text, a **type** (some suggestions are entities that navigate directly rather than search terms, which the client renders differently), **highlight ranges** for bolding the matched portion, and a **tracking id** so clicks can be attributed. That last one matters — without click feedback the ranking model has nothing to learn from.

One design question worth mentioning: the highlight ranges come from the search service, not the caller. Real matching involves phonetic matching, abbreviations, and typo correction — "bj" matching "Beijing," "beijng" matching "beijing" — and only the side that performed the match knows which characters matched. The caller holding two strings can't reconstruct it. General principle: **whoever produces the information emits it**, because recomputing downstream is often impossible.

**Data model**

The core structure is a **trie**. Why a trie rather than `LIKE 'new%'`? Prefix matching is exactly what a trie is for — walk down character by character to locate the prefix node, and the time depends only on prefix length, not corpus size. The key optimization: if every query had to traverse the whole subtree below the matched node and sort, it'd be far too slow. So **each trie node precomputes and stores the top-k suggestions for its prefix**. A query becomes: walk to the node, return its stored list. Essentially constant time.

The trie lives **in the memory of each suggestion service process** — not Redis, not disk. Trie traversal is pointer-chasing layer by layer, so putting it in a remote store would mean either dozens of network round trips per query or fetching the entire structure each time; neither is viable. Every service instance holds its own complete copy, which is what makes millisecond responses possible. For durability I'd periodically serialize the trie to disk or object storage and load from that snapshot on restart, rather than rebuilding from scratch. Raw search logs and popularity aggregates live somewhere cheap for the offline job to consume.

**High-level architecture**

Two separate paths.

**Read path**: keystroke → load balancer → a stateless suggestion service instance → check cache (Redis or process-local) and return on hit; on miss, walk the in-memory trie, get top-k, backfill the cache, return. Prefix caching is unusually effective because short hot prefixes ("n", "ne", "new") get queried constantly, so hit rates are very high and most traffic never touches the trie.

**Write path (offline popularity)**: search submissions → Kafka → a batch job (hourly or daily Spark/MapReduce) aggregates counts per query → recompute popularity and each node's top-k → produce a new trie snapshot → roll it out to service instances for hot loading. This path is allowed to be slow, because we established staleness is acceptable.

On the reload: instances poll for a new snapshot version, download it, build the new trie in the background, and swap a pointer atomically — **double buffering**, so the old trie keeps serving during construction and the swap doesn't interrupt anything. One trap: every instance pulling simultaneously will hammer object storage, so stagger the checks with random jitter — the same reasoning as retry jitter.

**Key tradeoffs and bottlenecks**

**Read/write separation plus accepted staleness.** I removed popularity updates from the online request path entirely, replacing them with batch computation and hot reload. The tradeoff is that suggestions aren't real-time and new hot terms lag; what I buy is a read path that's extremely simple and extremely fast, which is absolutely worth it at 17K QPS with a 100ms budget.

If the product genuinely needs breaking terms to surface within minutes, I wouldn't change the trie's update mechanism — I'd add a side path: stream processing computes hot terms over a few-second window into a small Redis table, and queries merge trie results with that. The table is small (hundreds of entries) so a Redis round trip is affordable, unlike trie traversal. **The trie keeps its simple full-replacement model and real-time comes from a bypass.**

**Distributed tries (sharding).** When the corpus exceeds one machine's memory or QPS exceeds one machine, split the trie. The obvious partition is by leading characters — "a" on one machine, "b" on another. The tradeoff is hotspots, since some letters carry far more and far hotter terms, requiring finer splits or consistent hashing for evenness, plus a routing layer that dispatches by prefix. At entry level, knowing "this problem exists, roughly split by prefix, expect hotspots, need routing" is enough.

**Layered prefix caching.** Cache at several levels: the client caches prefixes already typed, the CDN or edge caches hot prefixes, and the server has Redis. More aggressive caching is faster but makes invalidation after a popularity update harder — I'd use short TTLs (minutes to an hour) as the backstop, which is fine precisely because staleness is acceptable.

**Latency engineering details.** A request per keystroke will bury the backend, so the client **debounces** (waits ~100ms of typing pause before firing), cutting wasted requests. Response size also matters — return top-k and nothing extra.

Further extensions: personalization (reranking top-k by user history or location), spelling correction and fuzzy matching, and mid-word matching (which a pure prefix trie can't do — you'd need n-grams or an inverted index). But I'd hold the trunk: in-memory trie, precomputed per-node top-k, prefix caching, offline popularity updates. That combination carries a low-latency high-concurrency typeahead.

Worth adding one boundary statement: in a real product, ranking isn't just search counts — it's a model over click-through rate, personalization, and recency, and candidate generation may use an inverted index or FST rather than a pure trie to support phonetics and typo tolerance. But that's search relevance, not system design. I'd say plainly that I treat the ranking logic as a replaceable module and focus on how the service achieves millisecond latency, scales horizontally, and hot-swaps the index safely.

---

### Problem 9 — Top-K / Trending

**Clarifying requirements**

I'd box the problem in first, because "trending" can mean many things. Functionally: what ranking are we producing — a global top-10, or one per category or region? What's the window — trailing hour, trailing 24 hours, or "right now"? That's decisive, because it determines whether I need time windowing at all. How big is K — top-10, or top-1000? And the input is presumably an event stream (a user clicked an item, searched a term) that I count and rank by frequency.

Non-functionally: **volume** — writes (events) vastly exceed reads (viewing the ranking), which is characteristic; **latency** — the ranking is user-visible so reads should be milliseconds, straight from cache; **freshness** — how stale can the ranking be, seconds or tens of seconds; and the crux, which I'd raise myself: **exact versus approximate**. If we can tolerate slight error (positions 8 and 9 occasionally swapping while the top 3 are always right), approximation cuts cost dramatically. Almost no trending use case needs exact counts, so I'd default to approximate.

**Scale estimation**

Say 100M events a day. `Write QPS = 100M / 86400` ≈ 1,157. But trending traffic is spiky — a breaking event multiplies it — so at 5× the peak is around 5,000 writes/sec. On reads, 10M DAU viewing 10 times each: `10M × 10 / 86400` ≈ 1,157, comparable to writes, but reads are entirely cache-served and never touch the computation layer.

Storage, if **exact**: a counter per distinct item. At 10M distinct items a day and ~100 bytes each, `10M × 100B = 1GB` a day — actually quite manageable. But if the item space is huge (URLs, arbitrary queries, potentially unbounded), exact counting explodes. That's where **approximate** pays off: a count-min sketch estimates frequency for arbitrarily many items in a fixed-size 2D array — say 5 rows × 270K columns of ints, on the order of ten-odd MB, and **constant regardless of how many distinct items appear**. That's the concrete payoff of the approximation tradeoff.

**API design**

Two interfaces. On write I wouldn't expose an "increment" HTTP endpoint; events go through Kafka, roughly `recordEvent(itemId, timestamp, count=1)`. Kafka rather than synchronous calls because write volume is high and bursty, so I need a buffer to flatten peaks and decouple producers from consumers.

On read, `getTopK(category, window, k)` returning an ordered array of items with estimated scores, served directly from cache in milliseconds.

**Data model**

Three pieces. The **approximate counting structure** — the count-min sketch — is an in-memory 2D array living in the stream processor, optionally mirrored to Redis. It answers "roughly how many times has this item appeared," saving memory and updating fast, at the cost of possible **overestimation** (never underestimation, since hash collisions only add).

The **top-K heap**. The sketch tells you a specific item's frequency but won't enumerate the leaders. So alongside it I keep a min-heap of size K (the root is the current Kth place, i.e. the weakest qualifier): for each incoming item, query the sketch for its current estimate and compare against the root — bigger, and it replaces the root. A min-heap is right because the operation I do constantly is "evict the weakest," which is exactly what sits at the root, at O(log K).

The **result cache** — the computed ranking written to Redis under `topk:{category}:{window}` as an ordered list. All read traffic terminates here, fully isolated from computation. Raw events, if you want an offline exact reconciliation, go to a warehouse separately, off the real-time path.

**High-level architecture**

Events flow into **Kafka**, partitioned by item (itemId as the partition key), so the same item always lands on the same partition and consumer. Each consumer owns a slice of the item space, maintaining its own local sketch and local top-k — independent and horizontally scalable by construction.

Then the **stream processing layer** (Flink, Spark Streaming, or hand-written consumers) does two things: accumulate events into the sketch, and maintain the K-sized heap. **Time windowing** is how "trailing hour" is implemented — rather than one counter accumulating since the beginning of time, keep a series of small buckets (one sketch per minute) and sum the last 60 for a one-hour window. The window slides, expired buckets are dropped, and cooling topics fall off the list naturally instead of dominating forever. That's the reason for windowing rather than simple accumulation.

Because events are partitioned by item, each consumer only has a **local top-K**, so there's a **merge step**: a coordinator periodically (every few seconds) collects local top-K candidates, merges, re-sorts, and produces the **global top-K**, writing it to Redis.

There's a subtlety worth stating: **merging local top-Ks does not in general produce the correct global top-K.** An item can rank below the cutoff in every partition and still be globally first if its count is spread evenly. The standard mitigation is having each partition report **top-N with N well above K**, giving such items a chance to survive. The more rigorous fix is that a count-min sketch merges by **cell-wise addition**, preserving estimates for all items rather than just the top-K, so merged sketches can be queried for any candidate. In practice both are used: report a generous candidate set, then query the merged sketch for exact-ish global counts before the final sort.

Also worth distinguishing: the failure mode above applies when partitioning **by data volume**. If you partition **by key** — which is what Kafka partitioning on itemId actually does — then all of an item's events land in one consumer, that consumer's count is already global, and its local top-K is genuinely reliable. The error case only arises when the same item's counts are split across partitions. Being able to say which regime you're in is the real signal here.

The read path is trivial: `getTopK` hits a read service that returns the precomputed ranking from Redis in milliseconds. Write path (heavy, bursty, latency-tolerant) and read path (light, fast, stable) are cleanly separated.

**Key tradeoffs and bottlenecks**

**Exact versus approximate**, the central one. Exact means a real counter per item, which explodes with cardinality and is hard to scale; approximate uses a count-min sketch with constant memory and very fast updates, at the cost of overestimation from hash collisions and occasional jitter in the lower ranks. My judgment: for trending, the top-3 and top-10 relative order is what matters and nobody perceives whether 47th place is truly 47th, so approximation almost always wins. If exactness were genuinely required (billing, prizes), I'd revert to exact counting, or use the sketch as a coarse filter and exactly recount the candidate set.

**Hot keys overwhelming a partition.** Partitioning by item hash is fine until one term goes viral and its partition gets buried. Remedies: split the extremely hot key across partitions with a random suffix and recombine at merge time, or add local pre-aggregation in front of consumers so producers batch a single item before reporting.

**Window granularity.** Finer buckets (per second) mean a more real-time, smoother-sliding ranking but more sketches to maintain and merge; coarser buckets (hourly) are cheap but update in visible jumps. I'd pick a middle ground like one-minute buckets based on the freshness requirement.

**Freshness versus cost** — the merge and cache refresh interval. More frequent refreshes mean a fresher ranking but more coordinator collection, merging, and Redis writes. Since most trending features don't need second-level precision, I'd set the refresh to seconds or tens of seconds and trade a little latency for a much easier system.

**Scalability and fault tolerance.** Scaling writes means more Kafka partitions and consumers; scaling reads means more Redis replicas and read services — independently, which is why I insisted on read/write separation from the start. On fault tolerance, Kafka is durable and replayable, so a stream processor that dies resumes from its offset; in-memory sketches get periodic **checkpoints** to disk so a crash doesn't mean recomputing everything. Worst case you lose a small number of counts, which is acceptable for an approximate ranking.

---

### Problem 10 — Video Storage and Playback (YouTube-like)

**Clarifying requirements**

I wouldn't start drawing, because "video platform" covers wildly different products — YouTube, TikTok, and Netflix are very different systems. Functionally the core is three things: users upload, uploads get processed into a playable format, and other users watch smoothly. Comments, likes, subscriptions, recommendations I'd set aside as optional and revisit if there's time — get the upload-to-playback trunk standing first. I'd also confirm adaptive bitrate playback (dropping to 480p on a weak connection, 1080p on a good one), which is essentially mandatory and directly shapes the transcoding design.

Non-functionally, three points. First, this is textbook **read-heavy** — far fewer people upload than watch, and a popular video is uploaded once and played millions of times, a property that runs through every subsequent tradeoff. Second, playback needs low latency and global distribution, so a CDN is unavoidable. Third, video files are large, so storage and bandwidth are the dominant costs; availability must be high (never lose a video) but consistency can be relaxed — a video being searchable a few seconds after upload, or a view count updating late, is entirely fine.

**Scale estimation**

Say 1B registered users, 200M DAU. Reads first. At 5 videos per DAU per day: `200M × 5 = 1B` playback requests a day. `Playback QPS = 1B / 86400` ≈ 11,600, and with evening peaks at 2-3×, peak is in the low tens of thousands.

Writes are far smaller. Say 1M users upload one video a day: `upload QPS = 1M / 86400` ≈ 12. So the read:write ratio is roughly 11,600 to 12, about a thousand to one — confirming the read-heavy claim, which means the architecture optimizes the read path above all.

Storage: at ~300MB per source file and 1M a day, `1M × 300MB = 300TB/day` of raw ingest. But transcoding produces multiple renditions (1080p/720p/480p), so multiply by two or three — call it 700TB to 1PB of daily growth, hundreds of PB a year. Only object storage can absorb that; a database or normal filesystem is out of the question.

**API design**

Thin, three groups. For upload I wouldn't have clients POST the whole video to my application servers — slow and bandwidth-hostile. It's two steps: `POST /videos` (client sends title, description, and other metadata; server returns a videoId and a presigned upload URL), then the client PUTs the file directly to object storage using that URL, bypassing my servers entirely — the server only issues credentials.

**How the presigned URL is produced** is worth a sentence because it's counterintuitive: the server generates it by computing an HMAC signature with its own cloud credentials, **with no network call to S3 at all** — because the signing party and the verifying party share the same key. The signature covers the HTTP method, bucket, object key, and expiry, so changing any of them invalidates it. That makes the credential extremely narrow: this method, this key, within this window, and nothing else.

Large files go through **multipart upload** (5-10MB parts, uploaded in parallel, resumable). The flow differs slightly: `CreateMultipartUpload` is called by the **server** (it needs cloud credentials, and business validation happens here), and after getting an uploadId the server signs a batch of per-part URLs for the client; `UploadPart` and `CompleteMultipartUpload` go client-to-S3 directly. Mobile networks are unreliable and an upload without resumability is a bad experience, so video defaults to multipart; small files like avatars are fine with a single PUT and don't need the complexity.

One design point: this is **one endpoint, not two**. The client declares the file size and the server decides which form to return, with an `upload_type` field in the response telling the client which path to take. The threshold belongs on the server because it's a tunable operational parameter — changing it server-side takes effect immediately, while a client-side threshold requires an app release and leaves old and new versions behaving differently. There are also several clients (iOS, Android, web), and the same logic shouldn't be implemented several times. The server needs the file size for quota validation anyway, so the decision is free.

For playback, `GET /videos/{videoId}` returns metadata plus a manifest URL (the file describing available renditions and where each segment lives). The video data itself doesn't go through this endpoint — the client takes the manifest and pulls segments straight from the CDN. There's also `GET /videos/{videoId}/status` for polling transcoding progress, since a video isn't watchable the moment it's uploaded.

**Data model**

The central question is what goes where. Video files themselves — the source file and every transcoded rendition and segment — all go to object storage, because it's cheap, effectively unbounded, purpose-built for large files, and integrates directly with a CDN. Never in a database.

The source bucket holds **exactly what the user uploaded**, in whatever format they uploaded — a `.mov` stays a `.mov`. Since the presigned URL means the client writes directly to storage, no service is in a position to alter the bytes. And keeping the original matters independently: adding 4K later, switching to a better codec, or fixing a transcoding bug all require re-encoding from the source.

Metadata — videoId, uploader, title, description, duration, upload time, transcoding status, and the storage paths for each rendition — goes in a database. MySQL sharded by videoId is sufficient for structured fields at a billion rows; if the schema needs to stay flexible for evolving tags and attributes, MongoDB. View counts and likes are high-frequency updates that don't need strong consistency, so Redis absorbs them and flushes to the database asynchronously — writing the primary on every playback would flatten it.

Popular videos' metadata gets read repeatedly, so a Redis cache in front of the database keyed by videoId. That echoes the read-heavy theme — cache anything on the read path that can be cached.

**High-level architecture**

Upload path: the client calls `POST /videos` for a videoId and presigned URL — **all business validation (auth, quota, parameters) happens here**, because once a presigned URL is issued it can't be recalled and only expires on its own. The server also creates the metadata record now (title, description, status = uploading), so the later event can be correlated by object key.

The file then goes directly to the raw bucket. For multipart, the client also reports each part's ETag back to the application server — not for object storage's benefit (S3 has its own copy) but for **resumability**: if the client crashes and restarts, it asks the server which parts already landed and skips them.

Once all parts are uploaded the client calls `CompleteMultipartUpload` (an object storage API, called client-to-S3 directly, not through our servers). S3 validates the ETags, assembles the parts into a single object, and **only then emits exactly one `ObjectCreated` event**, which triggers the transcoding pipeline. A detail worth mentioning: the trigger isn't "bytes finished transferring," it's "the client explicitly called Complete" — **parts before Complete are staged, not an object** — so a user who abandons midway, or uploads and changes their mind, produces nothing downstream, and the staged parts are cleaned up by a lifecycle policy. Transcoding is heavy, slow, and unpredictable, so it must be asynchronous and must never block the upload request.

Transcoding workers pull jobs, convert the source into multiple renditions, segment them, generate the manifest, write results to the output bucket, and update the video's status to ready. The worker pool scales horizontally — add machines when the queue backs up.

Worth being precise about what a worker actually does: it isn't implementing codecs, it's **orchestrating** them — pulling the source, invoking ffmpeg (or submitting to a managed service like MediaConvert), handling retries, reporting status. The first step isn't transcoding at all, it's **probe and normalize**: inspect the container, codec, frame rate, and rotation metadata, then convert to a standard intermediate format. This must come before segmentation, because user uploads are wildly heterogeneous — iPhone `.mov`, older Android `.3gp`, variable frame rate, rotation metadata from portrait recording, missing audio tracks, and outright corrupt files. Without normalization every downstream step has to handle infinite cases; with it, segmentation and parallel encoding get uniform input. The probe stage is also where validation happens — corrupt file, unsupported codec, zero duration all fail fast, before wasting compute. Note that **file extensions can't be trusted** — a file named `video.mp4` may contain something else — so format detection reads the file header, and I'd key objects by videoId without an extension, since filenames are user-controlled input that shouldn't drive logic.

The real system design content here is the **orchestration**: segmenting a long video by time and encoding segments **in parallel** turns hours of serial encoding into minutes; the dependency graph (segment → encode each rendition in parallel → concatenate → generate manifest); and making a failed segment retry independently rather than restarting the whole video.

Playback path: the client gets metadata and a manifest URL from the application server, then pulls segments straight from the CDN. The CDN is the key piece — it caches video at edge nodes near users, so the first viewer causes an origin fetch and everyone after hits the edge, cutting both latency and origin egress. The player switches between renditions in the manifest based on measured bandwidth, which is adaptive bitrate, over HLS or DASH.

On the CDN itself: you buy it, you don't build it — it's a capital-intensive business of leasing facilities in hundreds of cities and negotiating peering, and only companies at Netflix's scale build their own (and even they started on commercial CDNs). With the origin in S3 the default is CloudFront, largely because S3-to-CloudFront egress is free and at video volume that's a significant difference. The configuration essentials are three: use origin access control so the bucket is private and only the CDN can reach it, preventing users from bypassing the CDN and hitting S3 directly; set cache policy by file type — segments get a one-year TTL since they're immutable, while manifests get seconds so content can be pulled quickly; and for paid content, layer signed URLs on top (the read-side analogue of the upload presigned URL).

**Key tradeoffs and bottlenecks**

**Transcoding** is the core difficulty — slow, CPU-hungry, and failure-prone — so a queue plus a worker pool makes it fully asynchronous, with the upload returning immediately and the client polling status. The tradeoff is that uploaded video isn't instantly watchable, bought in exchange for stability and scalability. A dead worker leaves the message in the queue for a retry.

**CDN and bandwidth cost.** The largest expense in a video system is egress bandwidth. Without a CDN, every playback hits object storage — the bill explodes and latency is bad. With one, popular videos are served from the edge and origin fetches are rare. CDNs cost money and cold videos have poor hit rates, but for a read-heavy workload where a few videos carry most of the traffic, it's overwhelmingly worth it.

**Storage amplification from adaptive bitrate.** Supporting several renditions means storing the same video multiple times, doubling storage or worse. That's spending storage to buy playback quality, and it's usually the right call, but it can be optimized: transcode only low renditions for unpopular videos, or transcode on demand (first request triggers it), reserving the full ladder for popular content.

**Scaling the read path.** Application servers are stateless and scale horizontally behind a load balancer. The database tier is shielded by Redis for most reads plus read replicas. High-frequency counters aggregate in Redis and batch-flush. But the real traffic never touches my servers at all — the CDN absorbs it — so this system's scaling bottleneck is usually transcoding compute and CDN/storage cost rather than application logic.

If the interviewer wants to go further, good directions are resumable upload details, prewarming popular videos into the CDN, and soft deletion with storage lifecycle management (aging cold data down to cheaper tiers).

---
