# Chapter 52 — Matching (Tinder)

> **Prerequisites:** Chapters 01 (partitioning, write skew), 04 (§4 geospatial, §5 Bloom filters), 50 and 51 (the two geo shapes this sits between)
> **Patterns:** candidate pre-generation, write amplification, mutual-state detection, approximate exclusion sets, exposure fairness

---

## 1. The problem

A user is shown one profile at a time and swipes right or left. When two users have both swiped right on
each other, they match and a conversation opens.

The product is three sentences long and the system underneath is not, for a reason that is easy to state
and easy to miss: **the interaction that produces almost all of the load produces almost none of the
value.** Swipes outnumber matches by two orders of magnitude, and matches outnumber conversations again.
You are building a system whose dominant write is a signal that will, with ~99% probability, never be read
by the person it is about.

That inverts the usual instinct. Most systems treat their dominant write as precious and engineer for its
durability; here the correct posture is to treat the dominant write as cheap and disposable in isolation,
while making the rare event it can trigger — the mutual match — absolutely correct.

**The property that makes it hard:** the system must detect a *mutual* condition across two independently
arriving, high-volume, geographically partitioned writes, exactly once, without ever reading the whole
swipe history — and must simultaneously decide *what to show next* under a constraint that has no
equivalent in a search product, namely that showing the same profile to everyone is a product failure even
when it maximizes engagement.

---

## 2. Requirements

### Functional

1. Retrieve a queue of candidate profiles for a user, filtered by distance, age, and gender preference.
2. Record a swipe (right or left) on a profile.
3. Detect a mutual right-swipe and create a match, notifying both users.

Defer, but name: messaging after a match (Chapter 30's problem), profile media (Chapter 21's), boosts and
super-likes, and paid features that reveal who liked you.

### Non-functional

- **Candidate fetch latency** — p99 under 200 ms for a batch, and the swipe UI must never wait. A stall
  between cards is the single most damaging latency in the product.
- **Swipe write latency** — the client must not block on it. Fire and forget from the user's perspective;
  the response is not needed to show the next card.
- **Match detection** — must be exactly-once and must never be missed. A missed match is invisible to
  users and unrecoverable; a duplicate match notification is visible and embarrassing.
- **Never re-show a profile the user has already swiped.** This is the constraint that shapes the storage
  design, and it must hold across devices and sessions indefinitely.
- **Write volume** — assume swipes dominate everything else by 100:1 or more; §3 derives it.
- **Consistency** — swipe recording can be eventually consistent; **match creation cannot**. Different
  guarantees for two writes in the same code path, and saying so is worth the sentence.

### Explicitly out of scope

The recommendation model that orders candidates (Chapter A4), abuse and fake-account detection, and
identity verification. Name them; the last two are the ones interviewers most often want acknowledged.

---

## 3. Estimation

Assume 50 million monthly actives, 10 million daily actives.

**Swipe volume — the constraining number**

```
10M DAU × ~100 swipes/day  = 1 × 10^9 swipes/day
1e9 / 86,400               ≈ 11,600 swipes/sec average
peak (evening, 3×)         ≈ 35,000 swipes/sec
```

Roughly a billion writes a day, and — the detail that matters — **they are not uniformly distributed in
time**. Swiping is an evening activity with a sharp local peak, and it is local *per timezone*, so the
global curve has several humps rather than one.

**Match volume**

```
right-swipe rate     ≈ 30% of swipes            → 3 × 10^8 right swipes/day
mutual probability   ≈ 3% of right swipes       → ~9 × 10^6 matches/day  ≈ 104/sec
```

**The ratio is ~110:1.** One in a hundred and ten swipes produces anything a user ever sees. This is the
number to state out loud, because every subsequent decision follows from it: the swipe path must be
optimized to the floor, and the match path — at a hundred per second — can afford a transaction, a lock,
and a synchronous notification without anyone noticing.

**Storage**

```
1e9 swipes/day × 365 × ~40 bytes (swiper, target, direction, ts)
                              ≈ 14.6 TB/year
```

Fourteen terabytes a year of data that is, in the overwhelming majority, never read except as a membership
test — "have I seen this person?" — which is the observation §7.2 exploits.

**Candidate retrieval**

```
10M DAU × ~4 queue refills/day = 40M retrievals/day ≈ 460/sec average
```

Two orders of magnitude below the swipe rate, because one retrieval serves a batch of many cards. This is
what makes candidate generation affordable enough to do well.

---

## 4. API

```
GET  /recs?limit=30
     -> 200 {profiles: [{userId, photos[], bio, distance_km, age, ...}], batchId}

POST /swipes
     body: {targetUserId, direction: "like"|"pass", batchId, clientSwipeId}
     -> 202 {matched: bool, matchId?}      // 202: recorded, not necessarily durable yet

GET  /matches?cursor=&limit=20
     -> 200 {matches: [{matchId, userId, matchedAt, lastMessageAt}], nextCursor}
```

Four decisions:

**Candidates are fetched in batches of ~30, not one at a time.** A per-card request puts a network round
trip between every swipe, which the latency requirement forbids. Batching also lets the client prefetch
the next batch while the user works through the current one, so the queue never visibly empties. The batch
size is a trade-off: larger batches mean staler candidates (someone may have swiped on you, or moved, or
been banned since the batch was built) and more wasted computation when the user abandons the session
after four cards.

**`clientSwipeId` makes the write idempotent.** Mobile clients retry; without a client-supplied key, a
retried swipe is a second row, and in the mutual-check path a duplicate can produce a duplicate match.
This is Chapter 03, §4 applied at 35,000 writes per second, where the dedup store's cost is a real design
input rather than an afterthought.

**`202 Accepted`, and `matched` is returned synchronously anyway.** The status code is honest — the swipe
is queued for durable storage — but the mutual check must be synchronous, because the product shows a
"It's a Match!" animation immediately. This is the seam in the design: two different durability postures
in one request, and §7.1 is about why that is safe.

**`batchId` is echoed back** so the server can attribute swipes to the batch that produced them, which is
what makes the exposure and ranking feedback loops measurable. Without it, you cannot tell whether a
profile was passed over or never rendered.

---

## 5. Data model

```
users
  user_id        BIGINT PRIMARY KEY
  lat, lng       DOUBLE
  h3_r6          BIGINT            -- ~36 km² cell, precomputed
  age, gender    ...
  pref_min_age, pref_max_age, pref_gender, pref_radius_km
  last_active_at TIMESTAMP
  status         SMALLINT          -- active / paused / banned

swipes                            -- the 14 TB/year table
  swiper_id      BIGINT            -- partition key
  target_id      BIGINT            -- clustering key
  direction      SMALLINT
  created_at     TIMESTAMP
  PRIMARY KEY ((swiper_id), target_id)

likes_received                    -- the reverse index; only right-swipes
  target_id      BIGINT            -- partition key
  swiper_id      BIGINT            -- clustering key
  created_at     TIMESTAMP
  PRIMARY KEY ((target_id), swiper_id)

matches
  match_id       BIGINT PRIMARY KEY
  user_a, user_b BIGINT            -- stored with user_a < user_b, always
  created_at     TIMESTAMP
  UNIQUE (user_a, user_b)          -- the correctness guarantee, see §7.1
```

Four deliberate decisions:

**`swipes` is partitioned by `swiper_id`,** so a user's entire swipe history is one partition. It is
written once and read only as a membership test. Nothing scans it.

**`likes_received` stores only right-swipes, partitioned by `target_id`.** This is the reverse index that
makes the mutual check a single-partition point read rather than a search. It is ~30% the size of
`swipes` because passes are not worth indexing — nobody ever asks "who passed on me". Storing the edge
twice, partitioned two ways, is the same move as Chapter 20's follow graph, and for the same reason: two
queries with different keys.

**`matches` stores the pair in canonical order with `user_a < user_b`, under a unique constraint.**
This is the single most important line in the schema. Without canonical ordering, the pair `(7, 12)` and
`(12, 7)` are different rows and the unique constraint does not fire, so two simultaneous mutual swipes
create two matches. Sorting the pair before writing makes the database itself the arbiter of
exactly-once, with no lock and no coordination. §7.1 develops this.

**Preferences live on the user row, not in a separate table,** because every candidate query reads all of
them together and none of them individually.

---

## 6. Architecture, derived

### Attempt 1: query candidates on demand, check the swipe table

For each `GET /recs`: find nearby users matching preferences, exclude anyone in `swipes` for this user,
return 30.

The exclusion is the problem. A heavy user has swiped on tens of thousands of profiles. In a dense metro
the geo query returns tens of thousands of candidates. The anti-join between them:

```
candidates in radius (dense metro)   ≈ 50,000
already swiped by this user          ≈ 30,000
```

Both sets are large, they live in different systems (a geo index and a wide-column store), and there is no
index that answers "nearby AND not in this user's 30,000-row partition". The engine must materialize one
side and filter. At 460 retrievals/second this is tens of milliseconds of anti-join per request in the
best case and seconds in the worst, and it degrades as users age — the exclusion set only grows.

The mutual check has the same shape in miniature and is fine: on a right swipe, read one row from
`likes_received`. That part works; keep it.

### Attempt 2: precompute a candidate queue per user

Build a queue of ~200 candidates per active user in a background job, store it in Redis, and serve
`GET /recs` by popping from it. The expensive geo query and exclusion run once per refill rather than once
per request.

```
10M DAU × 1 refill per ~200 swipes ≈ 10M × 0.5/day = 5M refills/day ≈ 58/sec
```

Fifty-eight expensive queries per second instead of 460, and each one amortizes across 200 cards. This is
the right structure and it exposes the real problem: the exclusion check inside the refill is still the
anti-join from attempt 1. Moving it off the request path made it affordable, not correct.

Two further problems appear:

- **Staleness.** A queue built at 6 p.m. and consumed at 11 p.m. contains people who have moved, gone
  inactive, been banned, or already swiped right on this user (in which case showing them is fine, but
  showing them *later* than necessary costs a match).
- **Queue abandonment.** Most users swipe a handful of cards and leave. Precomputing 200 candidates for
  every daily active is mostly wasted work. Refill in smaller increments, triggered by consumption, rather
  than filling to a fixed depth on a schedule.

### Attempt 3: make the exclusion set a Bloom filter

The exclusion is a **membership test over a set that only grows** — precisely what a Bloom filter is for
(Chapter 04, §5). Keep one per user, in Redis, alongside the queue.

```
30,000 swipes × 10 bits/element ≈ 37 KB per heavy user
10M DAU × ~10 KB average        ≈ 100 GB across the fleet
```

A hundred gigabytes of Redis to eliminate the anti-join entirely. Candidate generation becomes: run the
geo-and-preference query, then test each candidate against the filter and drop the hits. That test is a
handful of memory reads per candidate rather than a distributed join.

**The false-positive direction is the whole argument, and it is favorable.** A Bloom filter never returns
"absent" for a present element, so **you can never re-show a profile the user has already swiped** — the
hard requirement holds absolutely. What can happen is the reverse: at a 1% false-positive rate, roughly one
in a hundred *never-seen* profiles is wrongly excluded and silently skipped.

That error is acceptable, and articulating *why* is the point of the deep dive: the candidate pool is far
larger than any user will ever exhaust, the user cannot perceive the absence of a profile they never knew
existed, and the alternative — an exact set — costs a distributed join on every refill. Losing 1% of a
pool you will never finish, in exchange for removing the dominant cost, is a trade a designer should take
without hesitation. It would be entirely wrong in a system where the excluded item mattered individually,
which is why the error direction must be argued rather than asserted.

The filter cannot delete, so an "undo last swipe" feature cannot be implemented by removing from it.
§7.2 handles that.

### Attempt 4: make the match write atomic

At 104 matches/second the mutual path can afford real machinery, and it needs it — this is the one place
where being wrong is visible forever.

On a right swipe: write to `swipes` and `likes_received`, then read `likes_received[target][swiper]`. If
present, the swipe is mutual, and both users may be executing this same sequence concurrently. Attempt to
insert into `matches` with the pair in canonical order; the unique constraint makes exactly one of the two
concurrent inserts succeed. The winner emits the notification; the loser reads the existing row and
returns the same `matchId`.

No distributed lock, no coordination — the database's uniqueness constraint is the entire concurrency
control, which is the two-layer principle from Chapter 80 in its simplest possible form.

### Final architecture

```
  SWIPE PATH (35k/sec peak)

  Client ──► LB ──► Swipe service ──┬──► Redis: Bloom filter add (swiper)
                        │           │
                        │           ├──► Kafka ──► writer ──► Cassandra: swipes
                        │           │                          likes_received
                        │           │
                        │  if "like": read likes_received[target][swiper]
                        │           │
                        │           └── if present ──► Postgres: INSERT matches
                        │                              (user_a<user_b, UNIQUE)
                        │                                     │
                        │                                     ▼
                        └──── 202 {matched} ◄───────── notify both (Chapter 32)


  CANDIDATE PATH (460/sec, refills 58/sec)

  Client ──► Recs service ──► Redis: queue[user]  ──► 30 profiles
                                   │ (low water mark)
                                   ▼
                            Refill worker
                                   │
                    ┌──────────────┼───────────────┐
                    ▼              ▼               ▼
            geo+prefs query   Bloom filter    ranking (Ch. A4)
            (H3 k-ring on     exclusion       + exposure control
             users index)                       (§7.5)
```

The two paths share almost nothing, which is correct: one is 35,000 writes per second that must be cheap,
the other is 58 expensive computations per second that must be good.

---

## 7. Deep dives

### 7.1 Mutual match detection, exactly once

Two users swipe right on each other within the same few milliseconds. Both requests read the reverse
index, both find the other's like, both conclude "this is a match". Without care, two matches are created,
two notifications fire, and the conversation thread is ambiguous.

This is a **write-skew** anomaly (Chapter 01, §8): two transactions each read a set, each check a
condition that holds, and each write a different row. Snapshot isolation does not prevent it, which is
worth knowing because "we'll use a transaction" is not by itself an answer.

Three mechanisms, in increasing order of quality:

**A distributed lock on the pair.** Take a Redis lock on `sorted(a,b)` before checking. Works, and it puts
a network round trip and a failure mode into a path that runs 35,000 times a second — of which 99% do not
need it at all, since a pass or a non-mutual like never contends with anything.

**A single-partition transaction.** Co-locate both users' data so the check-and-insert is one atomic
operation. Impossible in general: users are partitioned by ID, and any given pair spans two partitions.
This is the option to raise and reject, because rejecting it correctly demonstrates you understand what
the partitioning bought and cost.

**A unique constraint on the canonically-ordered pair (the choice).** Compute `(min(a,b), max(a,b))`,
attempt the insert, and let exactly one of the two concurrent inserts win. The loser catches the
constraint violation and reads the row the winner created, returning the same `matchId`. Both users see a
match; only one row exists; no lock was taken; the 99% of swipes that are not mutual pay nothing.

The canonical ordering is doing all the work, and it is one line of code. Omitting it is a subtle bug that
survives testing — you have to swipe simultaneously from both sides to see it — and it is the specific
thing to say in an interview, because "use a unique constraint" without "on the sorted pair" does not
actually prevent anything.

**Notification exactly once** is a separate question. The insert's success is the trigger, and it happens
in a transaction; publishing the notification from inside that transaction is not possible across systems.
Use the outbox pattern (Chapter 01, §9): write the match row and an outbox row in one transaction, and let
a publisher deliver at-least-once to the notification service, which dedups on the `matchId` (Chapter 32).

### 7.2 The seen-set, false positives, and the undo problem

Covered in attempt 3; three refinements matter.

**Sizing and growth.** A Bloom filter's false-positive rate depends on how full it is, and it degrades as
elements are added beyond the design capacity. A user who swipes 300 times a day for two years reaches
~220,000 elements, well past a filter sized for 30,000, and the rate climbs toward useless. Handle it by
**scaling**: maintain a chain of filters, each sized for a capacity tier, and add to the newest while
testing against all of them. Membership is the union; the cost is one extra memory read per tier.

**The undo problem.** Bloom filters do not support deletion, so "undo my last swipe" cannot be implemented
by removing the element. Two workable answers: keep a small exact set of the last N swipes and check it
*before* the filter, treating a hit there as authoritative (this works because undo only ever applies to
very recent swipes); or use a counting Bloom filter, which supports deletion at ~4× the space. The first
is better — it exploits the fact that the deletable window is tiny, and it keeps the main structure
simple. A counting filter pays 4× on 100 GB to support an operation that applies to the last one element.

**Durability.** The filter is derived state — it can be rebuilt from the `swipes` partition — but a
rebuild for a heavy user reads 200,000 rows. Persist the filters (Redis with AOF, or periodic snapshots to
object storage) rather than treating them as a pure cache, and rebuild lazily only for users whose filter
is missing. Losing every filter simultaneously would mean re-showing profiles to everyone, which is the
one user-visible violation of the hard requirement, so this is worth the durability.

### 7.3 Swipe write amplification

At 35,000 writes/second peak, the swipe path must be cheap, and the natural implementation is not.

The naive version writes two rows synchronously (`swipes` and `likes_received`), reads one row for the
mutual check, and returns. That is three round trips to a distributed store on every swipe, of which the
`likes_received` write and the read are needed only for right-swipes (30%) and the mutual check produces
something only 3% of the time.

Three optimizations, each justified by the 110:1 ratio:

**Only index right-swipes.** `likes_received` skips passes entirely, cutting that write by 70%. Nothing
ever queries "who passed on me", and if a future feature needs it, the raw `swipes` table has it.

**Make the durable write asynchronous.** Publish the swipe to Kafka and let a consumer write to Cassandra.
The Bloom filter update (which is what prevents a re-show) happens synchronously in Redis, and the mutual
check reads the reverse index directly. The user-visible correctness — never re-show, and match detection
— does not depend on the durable write having landed. A swipe lost in a broker outage costs a row in an
analytics table, not a user-visible defect.

**Batch on the client.** A client can send swipes in groups of five with a short flush interval, cutting
request count by 5× at the cost of up to a few seconds of delay for non-mutual swipes. Right-swipes must
bypass the batch and go immediately, because the match animation cannot wait — which means the batching
applies to 70% of traffic and costs nothing on the path that matters. This asymmetry is only visible once
you have internalized the ratio.

### 7.4 Candidate generation and queue freshness

The refill job runs when a user's queue drops below a low-water mark, not on a schedule. It:

1. Runs a geo query — H3 k-ring at a resolution matched to the user's radius preference (Chapter 51, §7.1)
   — filtered by age, gender preference, and `last_active_at` within some window.
2. Excludes via the Bloom filter.
3. **Boosts users who have already right-swiped this user.** Reading `likes_received[me]` gives a small
   set of people who are guaranteed to produce a match if this user swipes right. Placing them early in
   the queue is the highest-value ranking signal available and costs one partition read.
4. Ranks the remainder (Chapter A4) with exposure control (§7.5).
5. Writes ~50 candidates to the queue.

**Queue depth is a freshness trade-off.** Deep queues amortize the expensive work and go stale; shallow
queues stay fresh and refill often. Fifty is a reasonable starting point at two refills per active session.

**Staleness must be handled at serve time, not prevented.** Before returning a batch, filter out users who
are now banned, paused, or out of range. This is ~30 cheap lookups against a hot cache, and it is far
cheaper than trying to invalidate every queue containing a user when that user's status changes — which
would be a fanout problem of exactly the Chapter 20 shape, with none of the payoff.

**Inactive users should not be in queues at all.** Someone who has not opened the app in thirty days will
not swipe back, so showing them wastes a card and produces a dead-end match if the user swipes right.
Filter on `last_active_at` in the geo query. This is the same insight as Chapter 20's inactive-user
optimization, applied to the read side instead of the write side.

### 7.5 Exposure fairness, which is a system property

Left alone, a ranking system that orders candidates by predicted engagement concentrates almost all
exposure on a small fraction of profiles. The distribution of right-swipes received is far more skewed
than the distribution of attractiveness by any measure, because ranking amplifies small initial
differences: a profile that ranks slightly higher is shown more, receives more likes, and ranks higher
still.

This is a **feedback loop** (Chapter A4, §7.4), and it is a product failure rather than a modeling
inaccuracy: the users receiving no exposure churn, and the users receiving overwhelming exposure are
buried in low-quality likes and also churn. A marketplace with a heavily skewed matching distribution
degrades from both ends.

Mitigations, all of which cost engagement in the short term:

- **Per-profile exposure caps within a time window.** Cap how many queues a given profile can appear in
  per hour. Simple, effective, and it requires a shared counter, which at this volume means a sharded
  counter in Redis.
- **Exploration slots.** Reserve a fraction of each batch for candidates the ranker is uncertain about
  rather than confident in. This is the standard exploration answer and it is also how you gather the
  data needed to rank new profiles at all.
- **Reciprocity in the objective.** Rank on predicted *mutual* interest rather than predicted right-swipe
  from the viewer. A profile that everyone likes and who likes nobody is a bad recommendation for
  everyone, and optimizing the one-sided probability cannot see that.

The last is the most important and the least obvious: **this is a two-sided matching problem wearing a
one-sided recommendation interface.** A candidate who frames it that way, rather than as "recommend
profiles", is demonstrating that they understood the product rather than the API.

### 7.6 Geographic partitioning and the mobile user

Shard the user index geographically (Chapter 51, §7.6) so a candidate query touches one shard. Two
complications specific to this product:

**Users move, and the interesting ones move a lot.** A user who travels invalidates their queue — every
candidate in it is now in the wrong city. Detect a location change beyond a threshold and flush the queue
rather than trying to repair it; a flush is cheap because the queue is derived state.

**Matches are permanent and location is not.** Two users who match in one city remain matched after either
moves. The `matches` table is therefore *not* geographically partitioned — it is partitioned by user, and
a match spans two user partitions. Because it is written 104 times a second and read on demand, keeping it
in a relational store partitioned by `match_id` with indexes on both users is entirely adequate, and it is
the store where the unique constraint from §7.1 lives.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Redis Bloom filter cluster loss | Profiles get re-shown — the one hard requirement violated | Persist filters (AOF or snapshots); rebuild lazily from `swipes`; accept degraded behavior for the rebuild window rather than blocking swipes |
| Redis queue loss | Queues empty; refill storm as every active user needs one at once | Rate-limit refills; serve a smaller batch immediately and backfill; warm the most active users first |
| Kafka backlog on the swipe path | `swipes` table lags; Bloom filter and match detection unaffected | This is the designed failure mode — alarm on lag, do not page |
| Duplicate match rows | Two conversations for one pair | Prevented by the canonical-order unique constraint; alarm on any constraint violation rate change, which would indicate the ordering was skipped somewhere |
| Match notification lost | Users never learn they matched — invisible and unrecoverable | Outbox pattern; the match row is the source of truth, and the match list shows it even if the push failed |
| Stale queue after a ban | A banned user is shown | Serve-time status filter over the ~30 returned candidates |
| Exposure concentration | Marketplace degradation, churn at both ends | Exposure caps and reciprocal ranking objective; monitor the Gini coefficient of likes received |

**Monitoring:** swipes per second by direction; match rate as a fraction of right-swipes (a drop is the
earliest signal that candidate quality or the reverse index has broken); queue-empty events per user
session (the direct measure of the latency requirement); Bloom filter fill ratio and estimated false-
positive rate per tier; **the distribution of likes received across profiles**, which is the fairness
metric and the one nobody instruments until the marketplace is already skewed.

---

## 9. Common mistakes

1. **Not deriving the swipe-to-match ratio.** Every design decision in the chapter follows from 110:1, and
   without it there is no argument for treating the two paths differently.
2. **Checking the exclusion set with a database anti-join.** It is the dominant cost, it grows with user
   tenure, and a Bloom filter removes it.
3. **Using a Bloom filter without naming the error direction.** "It never returns absent for a present
   element, so we can never re-show, and the 1% we wrongly exclude is imperceptible" is the argument. Just
   saying "Bloom filter" is not.
4. **Omitting the canonical ordering on the match pair.** The unique constraint does nothing without it,
   and the resulting bug only appears under true simultaneity.
5. **Reaching for a distributed lock on the mutual check**, putting a round trip on 35,000 requests per
   second to serialize the 3% that contend.
6. **Writing `likes_received` for passes**, tripling the size of an index nothing queries.
7. **Making the durable swipe write synchronous**, coupling the swipe UI to Cassandra availability for
   data that is not user-visible.
8. **Building deep queues on a schedule** rather than shallow ones on consumption, wasting most of the
   work on sessions that end after four cards.
9. **Treating this as a one-sided recommendation problem.** It is two-sided matching, and optimizing
   one-sided engagement produces a skewed marketplace that fails everyone.

---

## 10. Variants

**Job and candidate marketplaces.** Structurally identical two-sided matching, with two differences that
change the design: the "swipe" volume is orders of magnitude lower, so precomputation matters less; and
the exclusion set must be exact rather than probabilistic, because wrongly hiding a job from a candidate
is a real harm rather than an imperceptible one. That single requirement change removes the Bloom filter,
which is a clean demonstration that §7.2's argument was about the *consequence* of the error, not the
structure.

**Ride and delivery matching (Chapter 50).** Also two-sided and also contention-bound, but matching is
system-initiated rather than user-initiated, and the match must be *unique in time* (a driver takes one
ride) rather than *unique in pairs*. The concurrency control is a state machine transition rather than a
unique constraint.

**Social discovery ("people you may know").** Same candidate generation, no mutual gate, and the exclusion
set is smaller and exact. The interesting part moves entirely into ranking.

**Marketplace listings with mutual interest** (buyer saves, seller accepts). The mutual detection of §7.1
transfers directly; the volume ratio does not, so the asynchronous swipe path is unnecessary complexity.

---

## 11. Further reading

- Chapter 50 and Chapter 51, for the two geo shapes this chapter sits between
- Chapter 04, §5, for Bloom filter sizing, and Chapter 01, §8, for write skew
- Chapter A4, for the ranking stage and the feedback-loop problem in general form
- Chapter 32, for match notification delivery semantics
- Burton Bloom's original 1970 paper, for the space/error trade-off in its original statement
- Almeida et al., "Scalable Bloom Filters" (2007), for the scaling-chain construction in §7.2
