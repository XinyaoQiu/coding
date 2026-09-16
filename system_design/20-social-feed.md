# Chapter 20 — Social Feed (Twitter / X Timeline)

> **Prerequisites:** Chapters 01 (partitioning, hot partitions), 02 (caching), 03 (queues, at-least-once)
> **Patterns:** fanout on write vs on read, write amplification, power-law skew, materialized views

---

## 1. The problem

A user follows other users. When they open the app, they see a timeline of recent posts from the accounts
they follow, newest first. Posting is instantaneous; the timeline appears instantly.

This is the most-asked system design question in existence, and it will arrive wearing some costume:
Instagram, Facebook, LinkedIn, a news app, an internal activity feed. The costume changes nothing.

**The property that makes it hard:** the follower distribution is a power law spanning eight orders of
magnitude. The median account has a few hundred followers; the largest have hundreds of millions. Any
design that treats these the same fails at one end or the other. A design optimized for the median
collapses when a celebrity posts; a design optimized for celebrities makes every ordinary read expensive.

Everything else in this chapter is machinery. **The skew is the problem.**

---

## 2. Requirements

### Functional

1. Post a tweet (text, optionally with media).
2. Follow and unfollow a user.
3. Read a home timeline: recent posts from followed accounts, reverse chronological.

Defer, but name: search, direct messages, notifications, trending topics, replies and threads, retweets,
likes. Ask whether the timeline is **chronological or ranked** — it is the one clarifying question that
changes the architecture (§7.6), and a good interviewer is waiting to see whether you ask it.

### Non-functional

- **Timeline read latency** — p99 under 200 ms. This is the number that forces precomputation.
- **Read:write ratio** — heavily read-dominated, on the order of 100:1 or more at the timeline level.
- **Consistency** — eventual is fine for the timeline. A tweet appearing after a few seconds is
  acceptable and universally expected. **Say this explicitly**: it is what licenses the entire
  asynchronous fanout design, and a candidate who does not establish it has not earned the design they
  are about to draw.
- **Durability** — a posted tweet must never be lost. The write path and the fanout path have different
  durability requirements, and separating them is the point.
- **Availability** — the read path must stay up; degraded (stale, or partial) beats unavailable.
- **Skew** — the system must tolerate accounts with 10^8 followers. State this as a requirement, because
  it is the one the design is really for.

---

## 3. Estimation

Assume 500 million daily active users.

**Timeline reads**

```
500M users × 2 sessions/day  = 1B timeline loads/day
1e9 / 86,400                 ≈ 11,600 reads/sec average
peak (3×)                    ≈ 35,000 reads/sec
```

**Tweet writes**

```
100M tweets/day / 86,400     ≈ 1,160 writes/sec
peak                         ≈ 3,500 writes/sec
```

Modest. Storing tweets is not the problem.

**Fanout writes — the number that matters**

```
average followers ≈ 200
1,160 tweets/sec × 200 = 232,000 timeline-entry writes/sec (average)
peak                    ≈ 700,000/sec
```

Two hundred thousand writes per second, sustained, purely to maintain materialized timelines. Large, but
tractable: they are small appends to a sharded store, and they are asynchronous.

Now the tail:

```
One tweet from an account with 100,000,000 followers
  = 100,000,000 timeline writes for a single user action
```

**This is the design.** A hundred million writes triggered by one HTTP request cannot be done
synchronously, cannot be done quickly, and — done at all — will saturate the fanout system for minutes
while every other user's tweets queue behind it. And there are thousands of such accounts, several of
which post during any given minute.

**Storage**

```
100M tweets/day × 365 × 5 = 180B tweets
~300 B each (text + metadata; media by reference)
                          ≈ 55 TB
```

Sharded, but unremarkable. Timeline entries, if we store only IDs at ~16 bytes and cap each timeline at
800 entries: `500M × 800 × 16 B ≈ 6.4 TB` — which fits in memory across a modest Redis fleet. That the
capped, ID-only timeline fits in RAM is the second most important number in this chapter.

---

## 4. API

```
POST /tweets
  body: {text, mediaIds?, replyToId?}
  ->    201 {tweetId, createdAt}

GET  /feed?cursor=<opaque>&limit=50
  ->    200 {tweets: [...], nextCursor}

POST /users/{userId}/follows      -> 204
DELETE /users/{userId}/follows    -> 204

GET  /users/{userId}/tweets?cursor=&limit=   # a user's own posts — the pull path's primitive
  ->    200 {tweets: [...], nextCursor}
```

**Cursor, not offset.** The timeline is prepended to constantly; an offset-based page 2 would show items
already seen on page 1 and skip others. The cursor encodes a position in the ordering — with Snowflake
IDs (Chapter 04), the tweet ID *is* the cursor, since it sorts by time.

**The author comes from the auth token.**

Note that `GET /users/{id}/tweets` is not merely a profile endpoint: it is the primitive the pull path
depends on in §6, so it must be fast and heavily cached for exactly the accounts that are most expensive
to fan out.

---

## 5. Data model

```
tweets
  tweet_id      BIGINT      partition key   (Snowflake — time-sortable)
  user_id       BIGINT
  text          VARCHAR(280)
  media_ref     TEXT NULL
  created_at    TIMESTAMP

follows
  follower_id   BIGINT      partition key
  followee_id   BIGINT      clustering key
  created_at    TIMESTAMP

followers                                    # the reverse index — required for fanout
  followee_id   BIGINT      partition key
  follower_id   BIGINT      clustering key

timeline                                     # the materialized view
  user_id       BIGINT      partition key
  tweet_id      BIGINT      clustering key, descending
```

Three deliberate decisions:

**The follow graph is stored twice.** `follows` answers "who does X follow" (needed on the read path);
`followers` answers "who follows X" (needed on the fanout path). Both are single-partition reads. The
alternative — one table with a global secondary index — makes one of the two queries a scatter-gather,
and it is always the one on the hot path. Storing the edge twice is the correct call and should be
presented as a decision, not an accident.

**`timeline` stores tweet IDs only, never tweet bodies.** Justified in §7.2; it is the most important
schema decision in the chapter.

**Timelines live in Redis, not in a durable store.** They are a derived cache: everything in them can be
recomputed from `tweets` and `follows`. Losing a timeline costs a rebuild, not data. This licenses using
memory, which is what makes 35,000 reads/second at sub-200 ms possible.

---

## 6. Architecture, derived

### Attempt 1: pure read-time computation (fanout on read, "pull")

Store tweets. On timeline read: look up followees, query each one's recent tweets, merge, sort, return.

```
GET /feed
  followees = follows[user]                    # ~200 rows
  for each followee: recent tweets             # ~200 queries
  merge-sort, take 50
```

**Writes are trivial** — one row insert, no fanout at all. Unfollows are instantly reflected. No storage
beyond the tweets themselves.

**Reads are catastrophic.** Two hundred queries per timeline load, at 35,000 loads/second, is seven
million queries per second — before the merge. The p99 of the whole operation is the p99 of the slowest
of two hundred parallel calls, which at any realistic tail latency is far above 200 ms.

Pull is correct for the write side and unusable for the read side.

### Attempt 2: pure write-time computation (fanout on write, "push")

Maintain a materialized timeline per user. On tweet: look up the author's followers and append the tweet
ID to each of their timelines.

```
POST /tweets
  insert tweet
  for each follower: timeline[follower].prepend(tweet_id)
```

**Reads become trivial** — one Redis range read, sub-millisecond, exactly the property we needed.

**Writes become the problem.** 232,000 timeline appends per second on average is fine. One hundred million
appends for a single celebrity tweet is not: it takes minutes even at high throughput, it starves every
other tweet queued behind it, and the tweet is visible to the first followers long before the last —
which is a fairness problem people actually notice and complain about publicly.

Push is correct for the read side and unusable for the tail of the write side.

### Attempt 3: hybrid — the answer

**Push for ordinary accounts, pull for high-fanout accounts, merge at read time.**

Define a threshold on follower count. Below it, fan out on write as in attempt 2. Above it, do not fan
out at all. On read:

```
GET /feed(user):
    A = timeline[user]                                   # Redis, materialized (push path)
    celebs = followees(user) where follower_count > T     # small: single digits, typically
    B = for each celeb in parallel: recent_tweets(celeb)  # cached, tiny fan-out
    return merge_sort_dedup(A, B)[:50]
```

The merge is what hides the seam: the user sees one coherent timeline and has no way to tell which
entries came from which path.

Why this works arithmetically: the expensive-to-push accounts are **few**, so the pull side's fan-out per
read is small — a user follows perhaps two or three accounts above the threshold, not two hundred. And
those accounts' recent tweets are the single most cacheable objects in the entire system, because
millions of timeline reads request the same handful of tweets. The pull path is two or three reads
against near-certain cache hits.

**The threshold is a tuning knob, not a constant.** Say so. Somewhere around 10^5 followers is a
reasonable starting point; the right value depends on the measured cost of a fanout write versus a
read-time merge, and it should be adjustable without a deploy. Candidates who present a hard-coded number
as if it were derived are over-claiming.

### Attempt 4: make fanout asynchronous and bounded

The `POST /tweets` request must not wait for fanout. It writes the tweet durably, publishes a
`tweet_created` event, and returns.

```
POST /tweets ──► tweets store (durable) ──► Kafka ──► fanout workers ──► Redis timelines
      │
      └──► 201, in ~20 ms
```

This gives the right failure semantics: if the fanout system is degraded, timelines are **stale**, not
wrong, and no tweet is lost. Stale is a recoverable state; the backlog drains and the timelines catch up.

Two bounds are applied here:

- **Cap each materialized timeline** at ~800 entries. Beyond that, users are paginating into history,
  which is rare enough to serve from the pull path. The cap is what keeps total timeline storage at a
  few terabytes instead of unbounded.
- **Skip inactive users.** An account that has not opened the app in thirty days does not need a
  materialized timeline; compute it on demand when they return. In a real social network the inactive
  majority is large, and this is one of the biggest single savings available (§7.4).

### Final architecture

```
                                   ┌──────────────► Redis: timeline[user]  (IDs, capped 800)
                                   │                        ▲
POST /tweets ─► Tweet service ─► Kafka ─► Fanout workers ────┘
                    │                         │ (skip if author above threshold
                    ▼                         │  or follower inactive)
              Cassandra: tweets               │
                                              ▼
                                     Cassandra: followers

GET /feed ─► Timeline service ─┬─► Redis: timeline[user]            (push path)
                               ├─► Redis: recent_tweets[celeb] ×3    (pull path)
                               ├─► merge, dedup, sort
                               └─► Tweet cache: hydrate IDs → bodies
```

---

## 7. Deep dives

### 7.1 The celebrity problem, stated properly

The naive framing is "celebrities have too many followers." The precise framing is: **fanout-on-write
converts a single logical write into a number of physical writes proportional to the author's follower
count, and that count is power-law distributed, so the cost of a write has no useful upper bound.**

Three responses, only one of which is right:

1. **Scale the fanout fleet.** Does not fix write amplification, only distributes it. A hundred million
   writes is a hundred million writes on any number of machines, and the fleet must be sized for the
   worst-case author rather than the average one.
2. **Rate-limit or delay celebrity fanout.** Makes the fairness problem explicit and worse.
3. **Don't fan out above a threshold; merge at read time.** Removes the writes entirely. The read cost
   added is small and — crucially — is incurred against the most cacheable data in the system.

The reason (3) works is worth stating as a general principle, because it transfers to other chapters:
**when a write is expensive because it is amplified, and the source data is highly cacheable, move the
work to read time.** Chapter 50's location updates and Chapter 31's live comments are the same move.

### 7.2 Store IDs, not bodies

The materialized timeline holds `tweet_id` values, and bodies are fetched from a tweet cache at read time.

Three reasons, in increasing order of importance:

**Storage.** IDs are 8–16 bytes; a tweet with metadata is ~300. Storing bodies multiplies timeline storage
by roughly twenty, taking it from "fits in a Redis fleet" to "does not."

**Mutation.** A tweet can be deleted, edited, or hidden by moderation. With bodies denormalized into
millions of timelines, each such change is a distributed rewrite that cannot be done atomically and will
never fully converge. With IDs, the change happens in one place and every timeline sees it on next
hydration.

**Correctness of filtering.** Blocks, mutes, and suspensions must be applied at read time against current
state. If the body is already in the timeline, filtering requires knowing what is in every timeline; if
only the ID is there, filtering is a lookup during hydration.

Hydration is a batch `MGET` against a tweet cache with a very high hit rate — the fifty tweets on a
timeline are, by construction, recent, and recent tweets are hot.

### 7.3 Unfollow, block, and deletion

You cannot retroactively clean the materialized timelines. A user who unfollows someone with a thousand
of their tweets already in their timeline would require scanning and rewriting that timeline, and the
same operation for blocks, mutes, and suspensions, at unbounded cost.

**Filter at read time instead.** During hydration, drop entries whose author is no longer followed, is
blocked, is muted, or is suspended, and drop tweets that have been deleted. The filter set for a single
user is small and cacheable.

The consequence is that a timeline read may return fewer than the requested fifty items after filtering,
so the read must over-fetch — request seventy, filter, return fifty — and be prepared to fetch again.
This is a real implementation detail that follows directly from the design, and mentioning it signals
you have thought past the diagram.

New follows are the mirror image: the new followee's existing tweets are not in the materialized
timeline. Either backfill the recent ones on follow (bounded, cheap, and what users expect) or accept
that the new followee appears only for future tweets.

### 7.4 Inactive users

Fanning out to accounts that will not read the timeline is pure waste, and in a mature social network the
majority of accounts are dormant.

Maintain a `last_active_at` per user. Fanout workers skip followers inactive beyond a threshold. When such
a user returns, their timeline is empty or stale, so it is **rebuilt on demand** from the pull path — the
same merge machinery, applied to all their followees rather than just the celebrities — and they are
marked active again so subsequent fanouts include them.

The first load after a long absence is therefore slower. That is the correct place to put the cost.

### 7.5 Timeline as a cache, and rebuilding it

Because the timeline is derived, it can be lost. This is a feature: it means Redis can be run without
durability guarantees, and a node failure costs a rebuild rather than data.

The rebuild path must exist and must be tested, because it is also the path used for returning inactive
users, for new users, and after a cache incident. Rebuilding a timeline is: fetch followees, fetch recent
tweets per followee, merge, take the cap, write. It is the pull design from attempt 1 — which is why
attempt 1 was not wasted work. The design retains it as a fallback for a bounded number of users at a
time.

**Guard the rebuild.** After a large cache loss, every active user simultaneously needs a rebuild, which
is the thundering herd of Chapter 02 at maximum scale. Rate-limit rebuilds, serve partial timelines while
rebuilding, and warm the highest-traffic users first.

### 7.6 Ranking

If the timeline is ranked rather than chronological, the retrieval design above **does not change**. Keep
the stages separate:

```
retrieval (this chapter) → candidate set of a few hundred
         ↓
feature fetch → scoring model → re-ranking for diversity and business rules
         ↓
final 50
```

Retrieval produces candidates; ranking orders them. Conflating the two — trying to maintain a
score-ordered materialized timeline — fails because the score depends on the viewer and on features that
change continuously, so every score change would require rewriting timelines.

The one thing ranking does change: the cap must be larger, because ranking selects fifty from several
hundred rather than taking the newest fifty. See Chapter 22.

### 7.7 What happens when a celebrity tweets

Worth walking through explicitly, because interviewers ask it and it exercises the whole design:

1. The tweet is written to `tweets` and acknowledged. ~20 ms.
2. The fanout worker sees the author is above the threshold and **does no fanout work at all.**
3. The tweet enters the author's `recent_tweets` cache entry — one key, one write.
4. Every follower's next timeline read pulls that key. It is one Redis key being read by millions of
   clients, which is a hot key.
5. The hot key is handled by replication of that cache entry across many nodes and by an in-process
   local cache on the timeline servers with a short TTL. Because the data is immutable once written, a
   local cache is safe and there is no invalidation problem.

The system absorbs the event with one write and a very hot read. That is the desired outcome, and the
contrast with a hundred million writes is the whole argument for the hybrid design.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Fanout workers lag | Timelines stale by the queue depth; no data loss | Alert on consumer lag; the design's failure mode is staleness by construction |
| Redis timeline cluster loss | Timelines empty; reads fall back to rebuild | Rate-limited rebuild, partial results, warm the top users first |
| Tweet cache miss storm | Hydration hits Cassandra at read volume | Request coalescing; hydration is a batch read so amplification is bounded |
| Celebrity key hot spot | One Redis node saturated | Replicate the key; local in-process cache (safe — immutable data) |
| Follower table hot partition | A celebrity's follower list is a huge partition | Only read on fanout, which the threshold already suppresses for exactly these accounts |
| Duplicate fanout (at-least-once) | A tweet ID appears twice in a timeline | Timeline is a sorted set keyed by tweet ID — insertion is naturally idempotent |

That last row is worth noticing: choosing a **sorted set** rather than a list for the timeline makes the
at-least-once delivery of the fanout queue harmless, for free. Selecting a data structure whose semantics
absorb a delivery guarantee you already have is a small elegant thing to point out.

**Monitoring:** fanout queue lag (the leading indicator for everything), timeline read p99, hydration
cache hit rate, fanout writes per second broken down by author follower bucket, rebuild rate.

---

## 9. Common mistakes

1. **Choosing push or pull exclusively.** Either one fails at one end of the distribution. The interview
   is testing whether you notice that the distribution has two ends.
2. **Presenting the threshold as a constant.** It is a tuned parameter, and saying so is the difference
   between reciting and reasoning.
3. **Storing tweet bodies in timelines.** Multiplies storage twentyfold and makes deletion and moderation
   unimplementable.
4. **Synchronous fanout** in the POST handler, coupling post latency to follower count.
5. **Trying to clean timelines on unfollow or block.** Unbounded work for a filter that costs nothing at
   read time.
6. **Not capping the timeline**, leaving storage unbounded and deep pagination undefined.
7. **Ignoring inactive users** — the largest cheap saving in the design.
8. **Merging retrieval and ranking**, which makes the materialized timeline impossible to maintain.
9. **Never stating that eventual consistency is acceptable.** The entire asynchronous design is unjustified
   without it, and it takes one sentence in step 1.

---

## 10. Variants

**Instagram (Chapter 21).** The same feed with a media pipeline attached and a much heavier hydration
payload. Stories add a 24-hour TTL, which is a different storage decision — ephemeral content is a
natural fit for a store with native expiry and needs no deletion machinery.

**Facebook News Feed (Chapter 22).** Ranking dominates. Retrieval is this chapter; the interview goes into
candidate generation, feature stores, and model serving.

**Reddit (Chapter 23).** Content is organized by community rather than by author, so a post is
materialized once per community rather than once per follower. **The fanout problem largely disappears**
— and pointing this out is a good way to demonstrate you understand what causes it, rather than having
memorized the solution.

**LinkedIn / activity feeds.** Same structure, lower volume, usually ranked, with an additional constraint
that the same actor's repeated activity must be collapsed into a single entry.

**Notification feeds.** Structurally identical (a per-user materialized inbox) but with read/unread state,
which makes each entry mutable and therefore rules out treating the store as a disposable cache.

---

## 11. Further reading

- Chapter 22, for ranking; Chapter 21, for media
- [HelloInterview — Design Twitter](https://www.hellointerview.com/learn/system-design/problem-breakdowns/tweet-search)
- [system-design-primer — Design the Twitter timeline and search](https://github.com/donnemartin/system-design-primer/blob/master/solutions/system_design/twitter/README.md)
- Raffi Krikorian, "Timelines at Scale" (QCon 2012) — the original public account of Twitter's hybrid
  fanout, still the clearest description of the trade-off
