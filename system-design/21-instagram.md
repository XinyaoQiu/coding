# Chapter 21 — Instagram

> **Prerequisites:** Chapter 20 (fanout — assumed in full), 01 (partitioning), 02 (CDN, invalidation),
> 03 (queues, at-least-once), 04 (Snowflake IDs, Bloom filters)
> **Patterns:** presigned direct upload, asynchronous derivative generation, immutable blobs behind a CDN,
> native-TTL ephemeral storage, retrieval without a graph edge

---

## 1. The problem

A user posts a photo or short video with a caption. Followers see it in a reverse-chronological feed.
There is also a profile grid, Stories that vanish after a day, and an Explore surface full of content from
accounts the user does not follow.

The feed is Chapter 20 and this chapter will not re-derive it: assume the hybrid fanout, the materialized
per-user timeline of IDs, the read-time merge for high-follower accounts, read-time filtering, and
inactive-user suppression. All of it applies unchanged. Spend the interview elsewhere.

**The property that makes it hard:** the unit of content is a large immutable binary, and a binary is not
finished when the user presses "share." In Chapter 20, `POST /tweets` returning 201 meant the tweet existed
and was complete — the write was an insert. Here the write is a state machine spanning a cellular upload, a
transcode farm, and a global cache fill, completing tens of seconds after the user has put the phone away.
Two consequences drive everything: **media bytes must never pass through the application tier**, and
**fanout must be triggered by the media becoming ready, not by the post being created.**

A quieter third consequence appears in §7.3: a feed entry that was 300 bytes of text is now a few
kilobytes of metadata, and a tenfold hydration payload does real damage to a cache sized for tweets.

---

## 2. Requirements

### Functional

1. Upload a photo or short video and create a post with a caption.
2. Read a home feed of posts from followed accounts, newest first.
3. Read a user's profile grid.

Defer, but name: comments, likes, direct messages, search, hashtags, notifications, live video. Two
extensions are worth *taking*, because they are where this stops being Chapter 20 in a costume: **Stories**
(§7.4) and **Explore** (§7.5). If the interviewer will not choose, take Stories — the argument is cleaner.

### Non-functional

- **Feed metadata latency** — p99 under 200 ms for the JSON response. Chapter 20's budget, unchanged.
- **First image visible** — under 1 second on a 10 Mbit/s mobile connection. A *separate* budget from the
  JSON budget; conflating the two is the most common framing error in this question.
- **Time to visible after posting** — p95 under 30 seconds from last upload byte to appearance in
  followers' feeds. Deliberately loose; it is what buys the asynchronous pipeline.
- **Upload success rate** — above 99.5% including retries, on networks that drop mid-transfer routinely.
- **Durability** — the *original* must never be lost; it exists nowhere else once the phone's copy goes.
  Derived variants are regenerable and need only ordinary durability.
- **Media availability** — above 95% CDN edge hit rate; below that, origin bandwidth and origin QPS both
  become binding constraints (§3).
- **Consistency** — eventual for the feed, plus **read-your-writes for the author**, who must see their
  own post immediately even while it is processing.

### Explicitly out of scope

Moderation classifiers, copyright matching, ad insertion, and the recommendation *model* behind Explore —
the retrieval system is in scope, the model is Chapters 22 and A4.

---

## 3. Estimation

Assume 500 million daily active users, matching Chapter 20 so the comparison is honest.

**Post writes, bytes in, and storage**

```
100M posts/day / 86,400        ≈ 1,160 posts/sec average, ~3,500 peak     (Chapter 20's rate exactly)
average post ≈ 1 image, ~3 MB off a modern phone camera
peak ingress: 3,500/sec × 3 MB ≈ 10.5 GB/sec = 84 Gbit/sec

variants: 150px 15 KB + 320px 50 KB + 640px 150 KB + 1080px 400 KB ≈ 0.6 MB (20% of the original)
100M/day × 3.6 MB × 365 × 5    ≈ 650 PB
```

The record count is never the problem. Nor is 650 PB a *design* number: object storage scales
horizontally and prices linearly, and nothing in the architecture differs between 65 PB and 650 PB. It
forces tiering (§8), nothing more.

**Bytes out — the number that constrains everything**

```
500M users × 2 sessions × 30 posts viewed = 30B media fetches/day
30e9 / 86,400            ≈ 350,000 media req/sec average, ~1,000,000 peak
delivered variant ≈ 200 KB
350,000 × 200 KB         ≈ 70 GB/sec  = 560 Gbit/sec average
peak                     ≈ 200 GB/sec = 1.6 Tbit/sec
```

Put the two paths side by side. The metadata path is 35,000 JSON requests/sec at a few kilobytes each —
about 100 MB/sec. The media path is 1.6 Tbit/sec. **The media path moves roughly two thousand times more
bytes, and it must be served by a fleet you do not own.** That ratio is the architecture: any design in
which media bytes traverse an application server, inbound or outbound, is off by three orders of magnitude
and cannot be fixed by adding machines. Bytes go client → object storage inbound and CDN → client
outbound; the application tier only ever handles pointers.

At a 95% edge hit rate the origin sees 50,000 req/sec and 10 GB/sec; at 90% it sees 100,000 and 20 GB/sec.
Doubling the miss rate doubles origin cost, which is why §2 states 95% as a number rather than a hope.

---

## 4. API

```
POST /media/uploads
  body: {contentType, byteSize, sha256}
  ->    201 {mediaId, uploadUrl, uploadHeaders, expiresAt}

PUT  <uploadUrl>                   # client → object storage, direct. Never touches the API tier.

POST /posts
  body: {mediaIds: [...], caption, location?, clientToken}
  ->    201 {postId, state: "processing", createdAt}

GET  /feed?cursor=<opaque>&limit=10
  ->    200 {posts: [{postId, author, caption, media: [{variants, blurhash, w, h}],
                      counts, viewerState}], nextCursor}

GET  /users/{userId}/posts?cursor=&limit=33     # profile grid: 3 columns, 11 rows
GET  /stories/tray  |  GET /stories/{userId}  |  POST /stories {mediaId}      # §7.4
GET  /explore?cursor=&limit=30                                                # §7.5
```

**Upload is two calls, and that is the point.** The first is a small JSON request to your API that
allocates a `mediaId`, records intent, and returns a short-lived presigned URL. The second is a raw `PUT`
from the phone to object storage. Your servers see the first and never the second. Collapsing them into
one `multipart/form-data` POST is the mistake §6 exists to kill.

**Post creation references media by ID** — a small, fast, retryable JSON write, decoupled in time from
the slow flaky upload. The response carries `state`, because the write is a state machine and the API must
admit it. **`clientToken` and `sha256` make both writes idempotent** (Chapter 03, §4); mobile networks
make every write a retry candidate, and a re-uploaded identical file must resolve to the same object.

**The response carries a set of variants, not a URL.** Only the client knows its viewport width, pixel
density, and network; server-side selection needs device knowledge the server does not reliably have.
Ship the manifest and let the client choose, plus a `blurhash` so there is something to paint in 50 ms.

**`limit=10`, not 50.** Chapter 20 returned 50 tweets in 15 KB; 50 posts here is 150 KB of JSON the client
will not render for two more minutes of scrolling. Small pages, aggressive prefetch, and Chapter 20's
cursor (the Snowflake post ID sorts by time, so it *is* the cursor).

---

## 5. Data model

```
media
  media_id      BIGINT   partition key   (Snowflake)
  owner_id      BIGINT
  state         ENUM(reserved, uploaded, processing, ready, failed)
  content_type  VARCHAR
  content_hash  BYTES(32)
  original_key  TEXT                      # key in the quarantine bucket
  width, height, duration_ms
  variants      JSON                      # {"320": {key, w, h, bytes}, "640": {...}, ...}
  blurhash      VARCHAR(40)
  created_at, ready_at

posts
  post_id       BIGINT   partition key   (Snowflake)
  user_id       BIGINT
  media_ids     LIST<BIGINT>
  caption       TEXT
  state         ENUM(processing, published, removed)
  created_at

user_posts    user_id partition key, post_id clustering key desc   # grid, denormalized per Ch 20

timeline      Redis ZSET  user_id  -> {post_id}     # Chapter 20, unchanged, capped ~800
story_tray    Redis ZSET  author_id -> {story_id}, TTL 24h
story_seen    Redis HASH  viewer_id -> {author_id: last_seen_story_id}, TTL 48h
stories       media_id, author_id, created_at, expires_at    # store with native per-row TTL
```

**`media` is a first-class entity, separate from `posts`.** The state machine belongs to the bytes: media
is uploaded before a post exists, is often abandoned without becoming one, may be reused by a Story, and
fails for reasons (corrupt file, unsupported codec) unrelated to the post. Making media state a column on
`posts` forces a post row for every abandoned upload — a large fraction of all uploads. Its **variants are
a JSON blob, not a table**: always read as a set, never queried individually, so a `media_variants` table
buys nothing and costs a round trip on the hottest read in the system.

**Store object keys, never URLs.** URLs contain a CDN hostname, possibly a signature, possibly a region;
all three change during a migration or key rotation, and a URL denormalized into a hundred million rows
cannot be changed at all. Compose it at serialization time — Chapter 20, §7.2's argument (store the
identifier, not the rendering) one level down.

**Originals land in a quarantine bucket, distinct from the serving bucket.** Presigned upload means
unvalidated bytes from an untrusted client land in your storage before anything inspects them. Nothing in
quarantine is publicly readable or a CDN origin; promotion happens only after a worker decodes the file
and confirms it is what it claimed to be. **`content_hash`** makes upload retries idempotent, deduplicates
repeated images, and is the join key for future abuse matching, for 32 bytes.

---

## 6. Architecture, derived

### Attempt 1: bytes through the application tier

The client POSTs `multipart/form-data` to `POST /posts`; the API server receives the file, writes it to
object storage, resizes it into four variants, writes the post row, returns 201. Three independent
failures, each sufficient on its own:

**Bandwidth.** 3,500 posts/sec × 3 MB = 84 Gbit/sec of ingress at peak. At a realistic 10 Gbit/sec usable
per host that is nine machines acting purely as a wire, on the same fleet and load balancers serving feed
reads.

**Occupancy.** A 3 MB upload over mobile takes 8–20 seconds. At 3,500/sec with a 10-second mean, Little's
law gives 35,000 concurrent in-flight uploads, each holding a connection, a request slot, and a buffer.
The feed fleet is now overwhelmingly occupied by sockets waiting on a phone in a subway tunnel.

**CPU.** Decoding a 3 MB JPEG and producing four re-encoded variants costs 300–600 ms of CPU — at 3,500
posts/sec, 1,000–2,000 cores of image work colocated with the latency-critical read path. The first upload
spike takes feed p99 with it.

Rejected three times over, and all three fixes are one fix: get the bytes, the waiting, and the CPU off
the tier.

### Attempt 2: presigned direct upload, synchronous processing

The client uploads straight to object storage; the API issues a presigned URL and sees no bytes. Bandwidth
and occupancy both vanish — the object store is built for exactly this and its capacity is somebody else's
problem.

But `POST /posts` still waits for processing. For a photo that is 300–600 ms: bad, survivable. For video
it is fatal — a 60-second clip transcoded into four renditions takes 30 to 120 seconds of wall time even
with parallel segment encoding (Chapter 40), and no HTTP request waits for that. The worse problem is not
latency: synchronous processing returns an error to a user who already spent twenty seconds uploading over
cellular, and the retry re-uploads. Coupling a cheap retryable metadata write to an expensive flaky
computation is the wrong failure boundary.

### Attempt 3: asynchronous variant generation, fanout gated on readiness

```
client ─PUT─► quarantine bucket ─event─► Kafka ─► processing workers ─► variants
                                                        │
                                             media.state = ready ─► post.state = published
                                                                          │
                                                                          ▼
                                                                 Chapter 20 fanout
```

The capacity argument changes character. Steady-state CPU is unchanged — 1,160/sec × 0.5 core-seconds ≈
580 cores — but it now sits behind a queue, so peak is absorbed as **queue depth rather than provisioned
capacity**. A 3× spike against a fleet sized for 1.5× does not fail; it produces a backlog that drains,
and the symptom is time-to-visible drifting from 15 seconds to 90 — the requirement §2 wrote loosely so it
could be spent here.

**The critical ordering constraint: fanout is triggered by `media.state = ready`, not by post creation.**
Get this backwards and a post lands in a million timelines pointing at an image that 404s — invisible in
your metrics, highly visible to users, unfixable afterward because fanout cannot be recalled. This is the
one place the media pipeline and the Chapter 20 machinery interlock. The author is the exception: their
client renders optimistically from the local file and their profile query includes `processing` posts,
because read-your-writes for one user is cheap.

### Attempt 4: delivery

Serving 1,000,000 requests/sec and 200 GB/sec from object storage directly does not happen: the request
rate alone exceeds what a bucket does without partitioning tricks, and egress at commercial rates would
exceed the rest of the infrastructure combined. A CDN (Chapter 02, §6) turns this into an unusually
favorable cache problem. **The objects are immutable** —
a variant, once written, never changes — so TTLs are effectively infinite, there is no invalidation
problem, and every layer from shield to browser to in-app disk cache can hold it indefinitely.
Immutability is not luck: it follows from writing variants to keys derived from `(media_id, variant)`.
Access is also recency-skewed and shared — a POP's working set is the recent posts of accounts followed by
that POP's users — so 95–98% is achievable. Add a **shield tier** between edge and origin so a cold object
is fetched once per region rather than once per POP; with 100 POPs and no shield, a newly viral post
causes 100 origin fetches.

### Attempt 5: the hydration payload

Chapter 20's tweet cache assumed 300-byte entries; each post here is ~1.1 KB packed, so the same cache RAM
holds **one tenth as many posts** — three days of coverage becomes under a day. §7.3 handles it.

### Final architecture

```
                        ┌───── presigned PUT (bytes) ─────┐
                        │                                 ▼
Client ─┬─► POST /media/uploads ─► media row (reserved)  Quarantine bucket ─┐
        │                                                                   │ event
        │                Serving bucket ◄─┬─ Image workers (validate,       ▼
        │                                 │   4 variants, blurhash) ◄──── Kafka
        │                                 └─ Video workers (ABR — Ch 40) ◄──┘
        │                                          │
        └─► POST /posts ─► posts row ◄── media.state = ready
                               │
                               ▼
                     Chapter 20 fanout ──► Redis timelines (post IDs)

GET /feed ─► Feed service ─► Redis timeline + celebrity merge    (Chapter 20, §6)
                  ├─► post cache (viewer-independent core, ~400 B)      §7.3
                  ├─► counter store (likes, comments)                   §7.3
                  ├─► viewer-state batch (liked? saved? blocked?)       §7.3
                  └─► compose variant URLs from keys ──► JSON

Media bytes:  Client ◄── Edge POP ◄── Shield ◄── Serving bucket  (never via the API tier)
```

---

## 7. Deep dives

### 7.1 Presigned direct-to-object-storage upload

The client asks your API for permission; your API returns a URL containing a signature encoding the
bucket, key, expiry, and constraints, which the object store verifies itself. Your service is the
authorization decision and nothing else — no bytes, no connection, no timeout. **Constrain the signature,
not just the URL:** sign `Content-Length` and `Content-Type`, and set a short expiry. Without a length
constraint the URL is an unmetered write primitive against your bucket and someone will find it; without
an expiry it is a permanent one.

**The completion signal must come from the storage layer, not the client.** A client callback saying "I
finished" is a latency optimization: clients lie, crash after the last byte, and lose the callback. The
authoritative trigger is the bucket's object-created event. Use both — the callback to start 200 ms
sooner, the event as truth — and make the worker idempotent so both firing is harmless (Chapter 03, §4).
**Multipart and resumable upload for anything large.** A 50 MB video over mobile will be interrupted, and
without resumability the retry re-sends 50 MB and the 99.5% requirement is unreachable; part size is the
knob, and 5–8 MB is the usual compromise. **Abandoned uploads are collected by a bucket lifecycle rule,
not by your code** — expiring incomplete uploads and unreferenced quarantine objects after 24 hours is one
line of configuration, and writing a sweeper for it is self-inflicted.

**The cost:** you have handed an untrusted client a write handle into your storage, and validation can no
longer precede the write. Hence quarantine. A file claiming `image/jpeg` that is actually an HTML document,
served from your domain, is stored cross-site scripting — and presigned upload is exactly what makes it
possible. Naming that is the difference between having read about presigned URLs and having run them.

### 7.2 Asynchronous variant generation

**Which variants?** Derive them from surfaces, not round numbers: the grid is three columns on a ~400 dp
screen at 2–3× density → 320 px; the feed is full width → 1080 px; avatars and story rings → 150 px;
notifications and embeds → 640 px. Four served artifacts plus the retained original.

**Eager generation versus on-the-fly resizing** is a genuine contest. *On-the-fly* — a resizing service
behind the CDN generating any size from the original on a miss — stores only originals, supports any
dimension, needs no backfill, and is what a small product should build.

*Eager* costs the 20% storage computed in §3 and needs a backfill over 650 PB whenever the variant set
changes. It buys predictability, and two numbers make the case. An on-the-fly cache miss costs a full
decode-and-resize: 100–400 ms added to an image load with a 1-second budget, landing on exactly the
requests already slowest — cold, distant, first-view. And decisively: at 1,000,000 requests/sec a 2% miss
rate is 20,000 resizes per second sustained, so a CDN misconfiguration, a key rotation, or a POP coming up
cold becomes a CPU stampede against a fleet sized for 2%. The eager design's miss path is a byte-range
read; the on-the-fly design's is a computation, and computations stampede.

**Choose eager for the four hot variants, keep an on-the-fly service for the long tail** of odd sizes
requested by web embeds. Cost: 20% storage plus a backfill pipeline that must exist, be rate-limited, and
be tested — because you will change the variant set.

**Worker idempotency for free.** Delivery is at-least-once, so media will occasionally be processed twice.
Write each variant to a **deterministic key** derived from `(media_id, variant_name)`; a duplicate run
recomputes identical bytes and overwrites them with themselves. No locks, no dedup table. Same move as
Chapter 20's sorted-set timeline: choose a structure whose semantics absorb a delivery guarantee you have.

**Failure classification.** Bounded retry, backoff, dead-letter queue (Chapter 03, §5): transient failures
(an OOM on a huge image) retry, permanent ones (an unsupported codec) fail into `failed` with a specific
error to the author. Distinguishing them requires the worker to classify its own exceptions — unglamorous,
and where these pipelines rot.

### 7.3 Why hydration is heavier, and what it does to the cache

Chapter 20's hydration was a batch `MGET` of fifty 300-byte tweets: 15 KB, one round trip, near-certain
hits. Run it again here.
```
per post: author 150 B + caption 300 B + 5 variant descriptors 400 B + blurhash 40 B
          + meta 150 B + counts 50 B + viewer state 30 B   ≈ 1.1 KB packed, ≈ 3 KB as JSON
50 posts × 3 KB = 150 KB per feed response

Chapter 20:  100 GB cache / 300 B  = 330M tweets ≈ 3 days of content
Chapter 21:  100 GB cache / 1.1 KB =  90M posts  ≈ 21 hours of content
```

A cache that comfortably held every tweet now holds under a day. The hit rate does not collapse — feed
reads are recency-dominated — but the tail (profile browsing, permalinks, older notifications) starts
missing, and each miss is now a wide-row read. Three responses, taken together:

**Split the record by mutability.** The core — author, caption, media keys, dimensions, timestamp — is
immutable after publication. Counts are not: a popular post's like count changes hundreds of times per
minute, and in one entry every like invalidates the whole 1.1 KB record for every reader — write
amplification disguised as a caching problem (Chapter 02, §5). Immutable core in a long-TTL cache,
counters in a separate store read in the same batch; the core then never needs invalidation at all.

**Keep the cached record viewer-independent.** "Did *I* like this" must not be in the shared entry, or the
key becomes `(viewer, post)` and its cardinality becomes followers × posts — not a cache but a
materialized view of every possible read. Fetch viewer state as one batched set-membership query for all
fifty IDs; for the common negative case a per-viewer Bloom filter (Chapter 04, §5) answers "definitely not
liked" in microseconds.

### 7.4 Stories: ephemeral content as a different storage decision

A Story is visible for exactly 24 hours to followers, presented as a tray of authors rather than a
chronological list. Two things make it structurally different.

**It is never fanned out.** The tray is exactly "which accounts I follow have a live Story,"
answered by reading one small key per followed author. This is Chapter 20's *attempt 1* — the pull design
rejected there as catastrophic — and here it is correct, for precisely the reason it failed there,
inverted. In Chapter 20 a pull meant 200 queries returning per-author data no other reader wanted: nothing
shareable, query volume irreducible. Here perhaps 20 of 200 followees have a live Story, each key is a few
hundred bytes, and decisively **every one of those keys is read by all of that author's followers**, so the
hit rate approaches one. Twenty cache hits against a shared hot set is a different operation from two
hundred cold per-user queries. Pull is not wrong; pull is wrong when the merge is unshareable, and
Chapter 23 makes that observation the center of its argument.

**And the store needs no deletion machinery.**

```
300M stories/day (posted far more freely than feed posts)
metadata: 300M × 200 B = 60 GB live;   media: 300M × 1.5 MB = 450 TB live
```

Note what that arithmetic does *not* contain: a growth term. The permanent post store grows 360 TB every
day forever; the Story store is in steady state — 450 TB today, 450 TB in five years — because deletion
rate equals creation rate by construction. That single property changes store selection:

- **Metadata** goes in something with native per-key TTL: Redis `EXPIRE`, DynamoDB TTL, Cassandra's
  per-row TTL. Write the row with a 24-hour expiry and you are done — no tombstone table, no `deleted_at`
  column, no nightly sweeper, no risk of the sweeper falling behind.
- **Media** is expired by a bucket lifecycle rule on the Stories key prefix. Configuration, not code.
- **CDN TTL** must be capped at the object's remaining lifetime, or the edge serves a Story for a week
  after it expired. The one place ephemerality must be plumbed by hand, and the one people forget.
- **Seen state** is per-viewer and mutable, so unlike the tray it cannot be a disposable derived cache —
  but it can still be TTL'd at 48 hours, since there is nothing to remember about a Story that no longer
  exists. Storing `viewer -> {author: last_seen_story_id}` rather than a per-story set collapses it from
  O(viewers × stories) to O(viewers × authors-with-live-stories).

Compare the alternative: Stories as posts with an `expires_at` column. Now you need a deletion job that
keeps up with 300M rows/day without creating a compaction storm, the read path must filter rows the job
has not reached, and a product guarantee ("this disappears") is coupled to a batch job's health.
**Choosing a store whose native semantics match the product's semantics eliminates an entire subsystem.**
That is the transferable lesson. **The cost:** expiry is irreversible and untargeted, so a recovery
request or a legal hold cannot be served after the fact. An archive must be a *copy written at creation
time* into the permanent store — double storage, opt-in, decided at write time. There is no retrofit.

### 7.5 Explore: retrieval without a graph edge

The feed answers "what did the accounts I chose post," bounded by an explicit edge you already store.
Explore answers "what would this person like," over a corpus with no edge at all — so **there is no
recipient list to fan out to** and every technique from Chapter 20 is inapplicable on its face. The
candidate set is the whole corpus: perhaps 300M recent-enough posts. Scoring all of them per request is
35,000 × 3×10^8 ≈ 10^13 scorings/sec, absurd by seven orders of magnitude, so the two-stage
retrieval-then-ranking structure of Chapter 22 is forced rather than chosen.

The retrieval stage is this chapter's part. **Offline**, a pipeline produces an embedding per post (image
content, caption, early engagement) and per user (engagement history), builds an approximate
nearest-neighbor index over post embeddings, and materializes per-cluster popularity lists. **At serve
time**, retrieve a few hundred candidates by ANN lookup on the viewer embedding, union with cluster
popularity and "posts liked by accounts you follow," apply per-source quotas (Chapter 22, §7.1), then
filter and rank. The filters are where Explore differs from the feed, and they are not optional:

- **Seen-set filtering.** A chronological feed self-deduplicates via its cursor; Explore has no such
  invariant and will return the same striking photo every session. A Bloom filter over the last ~10,000
  impressions costs ~12 KB per user (Chapter 04, §5) against 40 TB for exact storage at 500M users. Cost:
  a ~1% false-positive rate silently hides 1% of eligible posts, undebuggable for any one complaint.
- **Integrity filtering, evaluated live.** In the feed the user chose the authors and a bad post is a
  failure of their choice; in Explore you chose, and it is a failure of yours. Classification changes after
  publication, so a precomputed list must still be re-filtered at serve time.

**Precompute the ranked list, refresh it periodically.** The budget forces this: ANN 20–40 ms, feature
fetch 30 ms, scoring 30–50 ms and re-ranking sum to 150 ms or more that cannot hide behind a spinner on a
grid the user expects instantly. Compute a few hundred ranked candidates per active user asynchronously —
on session start and on a schedule — cache the list, and serve pages from it with only the cheap live
filters applied. Cost: **staleness**; something liked thirty seconds ago does not influence the grid until
the next refresh. Mitigate with a small realtime layer boosting candidates similar to the last few
interactions, and accept that the bulk is minutes old.

### 7.6 Deletion, and the parts of the system that do not forget

Deleting a post must reach the row, the grid, every materialized timeline containing it, the post cache,
the media and its variants, and every CDN edge holding them — and these behave very differently. Row and
grid are a normal write; the post cache is invalidated by key. **Materialized timelines are not cleaned**
(Chapter 20, §7.3): the entry is filtered at hydration, because scanning a million timelines to remove one
ID is unbounded work for a filter that is free at read time. **Media is deleted late and asynchronously** —
hours of delay before removing bytes converts "an accidental deletion is unrecoverable" into "a support
ticket," at negligible cost.

**The CDN is the hard one.** A global purge is slow, provider-rate-limited, and not something you can do
at deletion volume. Two mitigations, and you want both: **surrogate keys** — tag each variant with the post
ID at fill time and purge by tag rather than enumerating URLs — and **short-lived signed URLs** for
anything with privacy semantics, so access expires on its own even if bytes remain cached. Accept that a
deleted public photo may be edge-served for a bounded window, and know the number. The general point:
**deletion is cheap only where the data is addressed by a key you control.**

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Processing workers lag | Time-to-visible grows; nothing lost, nothing wrong | Queue-depth alert; autoscale on lag; the design converts capacity failure into latency by construction |
| Fanout fired before media ready | Timelines full of broken images; unrecoverable afterward | Gate fanout on `media.state = ready`, asserted in the consumer, not just the producer |
| CDN hit rate drops (config change, key rotation) | Origin goes 50k → 200k req/s and 10 → 40 GB/s | Shield tier; origin rate limiting with 503 rather than collapse; alert on hit rate before origin notices |
| Object storage degraded in one region | Uploads fail; existing media still served from edge | Regional upload failover; the read path survives because it is cache-fronted and the data is immutable |
| Corrupt or hostile upload | Worker crash, or stored XSS served from your domain | Quarantine bucket; decode-and-validate before promotion; never serve from the upload bucket |
| Post cache loss | Hydration falls through to the primary store at 35k req/s, 1.1 KB per row | Request coalescing; batch reads; drop optional fields under load |
| Counter store loss | Likes and comment counts show zero on a working feed | Counters are approximate and recomputable; render without counts rather than failing the feed |
| Story TTL not propagated to CDN | Expired Stories still fetchable at the edge | Edge TTL set to remaining lifetime; signed URLs with matching expiry |

One operational note follows from the storage arithmetic rather than from an outage. **Tier the media:**
access drops by orders of magnitude within a week, so objects move to infrequent-access and then archival
classes on an age-based lifecycle — originals and variants on *different* schedules, since the original is
a durability artifact almost never read while the 1080 px variant is hot. Their durability requirements
differ too: a lost variant is regenerated, so that tier can be cheaper — which means the regeneration path
must work and be exercised.

**Monitoring:** processing queue lag and time-to-visible p95 (the write path's leading indicators); CDN
edge hit rate by variant (the read path's, and the bill's); upload success rate by platform and network
type; the media state distribution over time, where a rising `processing` population is a stall and a
rising `failed` one is a codec or validation regression; hydration cache hit rate; and bytes served per
feed session, the number connecting engineering decisions to the cost line.

---

## 9. Common mistakes

1. **Routing media bytes through the application tier.** The disqualifying error: §3 puts the media path
   at two thousand times the byte volume of the metadata path, so anything in it that you scale yourself is
   a design error, not a capacity problem.
2. **Making the client's upload-complete callback authoritative.** Clients crash, lie, and lose networks;
   the object store's own event is the truth and the callback is only a latency optimization.
3. **Fanning out on post creation rather than media readiness**, producing timelines full of broken images
   that cannot be repaired because fanout cannot be recalled.
4. **Generating variants synchronously** — works for photos, impossible for video, and couples a cheap
   retryable write to an expensive flaky computation.
5. **Storing URLs instead of object keys**, making a CDN migration or key rotation a hundred-million-row
   rewrite.
6. **Putting mutable counts and viewer state in the shared post cache**, turning every like into an
   invalidation and every cache key into a `(viewer, post)` pair.
7. **Treating Stories as posts with an `expires_at` column**, inventing a deletion subsystem a native-TTL
   store provides for free and coupling a product guarantee to a batch job.
8. **Assuming Chapter 20's hydration cache sizing carries over**, when entries are ten times larger, so
   the cache covers a day instead of three and the tail reads begin missing.
9. **Treating Explore as "the feed, but for strangers."** There is no recipient list and nothing to fan
   out; it is retrieval-and-ranking with a seen-set and a stricter integrity filter, precomputed because
   its latency cannot be paid per scroll.

---

## 10. Variants

**Pinterest.** Explore *is* the product and the follow graph is nearly vestigial, so §7.5 becomes the main
path and board membership replaces the timeline as the organizing structure. Masonry layout means variants
are selected by width alone, with height in metadata so the client can reserve space before bytes arrive.

**TikTok.** Video-first, so the pipeline is Chapter 40's rather than §7.2's, and retrieval is almost
entirely unconnected content, so Chapter 22 dominates. One item fills the screen and the next must be
prefetched and decoded before the swipe, so the budget moves from "first image in 1 second" to "next
video's first frame already buffered."

**Snapchat.** Ephemerality is the default rather than an extension, so §7.4 applies to the whole corpus
and permanence is the special case. Media is often delivered once to a small known audience, which weakens
the CDN argument badly — a CDN caches well when many readers want the same object, and a snap has one.

**Photo backup (Google Photos, iCloud Photos).** Same upload path, no feed. Durability and deduplication
dominate, content hashing becomes the core of the design, and the read pattern is a personal archive
browse — far colder and more tail-heavy than a feed. **E-commerce catalogs** invert the write profile
instead: uploads are batched by merchants rather than trickled by users, so the pipeline is a throughput
problem rather than a latency one, over a fixed and well-known set of surfaces.

---

## 11. Further reading

- Chapter 20 for the feed this builds on; Chapter 22 for the ranking behind Explore; Chapter 40 for the
  video pipeline §7.2 defers to
- Beaver et al., "Finding a Needle in Haystack: Facebook's Photo Storage" (OSDI 2010) — why general-purpose
  filesystems fail for billions of small immutable blobs
- Muralidhar et al., "f4: Facebook's Warm BLOB Storage System" (OSDI 2014) — §8's tiering argument, worked
  out in production
- Lisa Guo, "Scaling Instagram Infrastructure" (QCon London 2017)
- [HelloInterview — Design Instagram](https://www.hellointerview.com/learn/system-design/problem-breakdowns/instagram)
