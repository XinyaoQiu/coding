# Chapter 23 — Reddit / Message Board

> **Prerequisites:** Chapter 20 (fanout, and why it is expensive), 01 (partitioning, clustering keys),
> 02 (caching), 04 (Snowflake IDs)
> **Patterns:** per-community materialization, materialized-path trees, time-decay ranking, approximate
> counting

---

## 1. The problem

Users subscribe to communities. Anyone may submit a post to a community; anyone may vote on it and reply
to it, and replies nest arbitrarily deep. Each community shows a listing ordered by "hot," "new," or
"top," and a user's home page is the merge of the listings of the communities they subscribe to.

The first thing to notice is what is *absent*. There is no follow graph on the read path, so the write
amplification that dominated Chapter 20 does not appear. A post is materialized **once, into its
community**, and readers assemble their home page from the communities they subscribe to. §6 derives this
properly, because explaining precisely why the fanout problem evaporates here is the most valuable thing
this chapter does — it is the difference between having understood Chapter 20 and having memorized it.

**The property that makes it hard:** the sort key is a function of time. Chapter 20's ordering key was
`created_at` — immutable, so a materialized listing was an append-only structure that never needed
rewriting. Here a post's rank depends on its vote count, which changes thousands of times an hour for a
front-page post, and on its age, which changes for every post continuously. A naive materialized ranking
is stale the instant it is written and would need rewriting on a clock tick, for every post, in every
community. The chapter's central trick (§7.2) is a ranking function constructed so that **a post's score
changes only when its votes change, never with the passage of time** — which converts an O(posts × ticks)
update rate into an O(votes) one.

The second hard part is unrelated to the first: comment threads are trees, reads of them are subtree
reads, and the storage layout has to make that a range scan (§7.1).

---

## 2. Requirements

### Functional

1. Submit a post to a community; read a community listing sorted by hot, new, or top.
2. Read a home listing: the merge of subscribed communities.
3. Read and write threaded comments, and vote on posts and comments.

Defer, but name: search, moderation tooling, private and quarantined communities, awards, chat, user
profiles, multi-community bundles. Cross-community ranking is worth taking as an extension (§7.5).

### Non-functional

- **Community listing p99 under 150 ms**; **comment page (post plus top ~200 comments) p99 under 300 ms**.
  The comment page is the more expensive read and deserves its own number.
- **Read:write ratio** — roughly 150:1 at the post level. Votes are the high-volume write (§3).
- **Vote counts may be approximate and eventually consistent.** Say this explicitly. It is what licenses
  batched counting, in-memory counters, and vote fuzzing (§7.3), and none of those are defensible without
  it. The **user's own vote state**, by contrast, must be exact and immediately visible.
- **Durability** — posts and comments are never lost; an individual vote row is never lost either, since
  it is the authoritative record from which counts are reconstructed.
- **Listing stability** — a user paging through a listing should not see the same post twice or skip one.
  This is achievable only approximately, and §4 says how approximately.
- **Availability** — reads must survive the loss of the ranking pipeline; a stale listing beats no listing.

### Explicitly out of scope

Spam and manipulation detection beyond the fuzzing discussion, federation, and the recommendation of
communities to join.

---

## 3. Estimation

Assume 50 million daily active users — an order of magnitude below Chapter 20, which is honest for a
message board and which matters, because several conclusions below depend on the *ratio* of communities to
users rather than on raw scale.

**Traffic**

```
posts       2M/day  /86,400 ≈     23/sec,  peak    70/sec
comments   20M/day           ≈    230/sec,  peak   700/sec
votes     150M/day           ≈  1,700/sec,  peak 5,000/sec     ← the write path
page views 300M/day          ≈  3,500/sec,  peak 10,000/sec
```

Twenty-three posts per second. Nothing about the write path is hard.

**Fanout, computed twice — the arithmetic this chapter exists for**

```
Chapter 20, per-follower:    1,160 posts/sec × 200 followers  =  232,000 writes/sec

Here, if we copied that design and fanned out to subscribers:
  ~30% of posts go to communities averaging 5M subscribers
  23/sec × 0.30 × 5,000,000                                   ≈ 34,500,000 writes/sec

Here, materializing per community:
  23 posts/sec × 1 community                                  =         23 writes/sec
```

Two things to take from this. Naive fanout is not merely unnecessary here, it is **150 times worse than
Twitter's** despite fifty times fewer posts, because communities are larger than follower lists. And
materializing per community is four orders of magnitude cheaper than Chapter 20 and six orders cheaper
than the naive alternative.

**The materialized ranking state — the number that constrains the design**

```
active communities:   10,000 × top 1,000 post IDs+scores × 16 B  =   160 MB
dormant communities: 3,000,000 × top   100               × 16 B  =  4.8 GB
                                                        total    ≈    5 GB

Chapter 20's per-user timelines: 500M users × 800 × 16 B         =  6.4 TB
```

**The entire ranked state of the site fits in about five gigabytes** — roughly 1,300× less than Chapter
20's materialized timelines. It fits on one machine, let alone a Redis cluster, and it is shared: the same
listing key serves every subscriber of that community. That is what makes read-time assembly viable here
and impossible there.

**Storage**

```
posts:    2M/day × 365 × 15 yr  =  11B  × ~1 KB   = 11 TB
comments: 20M/day × 365 × 15 yr = 110B × ~400 B   = 44 TB
votes:    150M/day × 365 × 15   = 820B × ~32 B    = 26 TB
```

Sharded, unremarkable, and — note — comments outnumber posts ten to one and outweigh them four to one.
The comment system is the larger system.

**Listing read amplification**

```
10,000 listing reads/sec × ~50 subscribed communities = 500,000 ZSET reads/sec
spread over ~10,000 hot keys                          = ~50 reads/sec/key
```

Half a million small reads per second against a few thousand hot keys in memory. This is the load profile
Chapter 20's pull design could not achieve, and §6 explains exactly what changed.

---

## 4. API

```
POST /r/{community}/posts    {title, url?, body?, clientToken}   -> 201 {postId}

GET  /r/{community}/posts?sort=hot|new|top&t=day&cursor=&limit=25
  -> 200 {posts: [...], nextCursor}

GET  /feed?sort=hot&cursor=&limit=25                # merge over subscriptions

GET  /posts/{postId}/comments?sort=best&limit=200&depth=8&cursor=
  -> 200 {comments: [{commentId, parentId, depth, path, body, score, ...}],
          more: [{parentId, hiddenCount, token}]}

GET  /comments/{commentId}/children?limit=200&depth=8&token=

POST /posts/{postId}/comments {parentId?, body, clientToken}     -> 201 {commentId}
POST /vote  {thingId, dir: 1|0|-1}                               -> 204
```

**The listing cursor is `(score, post_id)`, not an offset and not a timestamp.** For `sort=new` the post
ID is time-sortable and behaves exactly as in Chapter 20. For `sort=hot` nothing is stable: a post can
gain votes between page 1 and page 2 and cross the boundary, appearing twice or not at all. Three options.
An *offset* exhibits the anomaly maximally and is the worst choice. A *per-session snapshot* of the ranked
listing eliminates it entirely, at the cost of state per session — reasonable, and it is what Chapter 22,
§4 chooses for a ranked feed. A *`(score, id)` keyset cursor* is stateless and anomalous only for posts
whose score crossed the boundary between requests.

**Choose the keyset cursor here**, and justify it with the distribution rather than with a preference:
listings are shallow — the overwhelming majority of sessions never go past the third page — and hot scores
move slowly for anything below the top of a large community, so the anomaly rate is small and the failure
is a duplicate rather than a missing item. Chapter 22 chooses differently because a *ranked personalized*
feed re-scores everything on every request, which makes the anomaly rate enormous rather than small. The
same problem, two answers, and the deciding factor is how fast the ordering churns.

**The comments endpoint returns a flattened list with `depth`, plus a `more` array — not nested JSON.**
Nesting an arbitrarily deep tree produces a response whose shape and size are unpredictable, blows out
recursive parsers, and prevents incremental rendering. Flattening with an explicit depth lets the client
indent, and the `more` entries are the continuation points where the server truncated (§7.4).

**Voting is a `SET`, not an `INCREMENT`.** The body carries the desired state (up, none, down), not a
delta. Sending it twice is a no-op, which makes the endpoint idempotent for free and makes at-least-once
retries from flaky clients harmless (Chapter 03, §4). An increment-based API would require an idempotency
key to be safe; a state-based one requires nothing.

---

## 5. Data model

```
posts
  post_id       BIGINT   partition key      (Snowflake)
  community_id  BIGINT
  author_id, title, url, body, created_at
  ups, downs, comment_count                 # denormalized, approximate (§7.3)
  hot_score     DOUBLE                      # recomputed only on vote change (§7.2)
  removed_at, locked

comments
  post_id       BIGINT   partition key      <-- the whole thread on one partition
  path          VARCHAR  clustering key     <-- materialized path, pre-order by sort
  comment_id    BIGINT
  parent_id     BIGINT NULL
  depth         SMALLINT
  descendant_count INT                      # needed for "N more replies" (§7.4)
  author_id, body, created_at, ups, downs, removed_at

votes
  user_id       BIGINT   partition key      <-- by voter, not by thing
  thing_id      BIGINT   clustering key
  dir           SMALLINT, created_at

subscriptions   user_id partition key, community_id clustering key

listings (Redis)
  hot:{community_id}          ZSET(post_id -> hot_score),  capped 1,000
  new:{community_id}          ZSET(post_id -> created_at), capped 1,000
  top:{community_id}:{window} ZSET(post_id -> net_votes)
```

**Comments are partitioned by `post_id`, so an entire thread lives on one partition.** Reading a thread is
then a single-partition range scan with no scatter-gather. The cost is honest and worth stating: a thread
with 100,000 comments is a 40 MB partition, and a front-page thread is a hot partition. The alternative —
partitioning by `comment_id` — spreads that heat perfectly and makes *every* thread read a scatter-gather
across the whole cluster, which is strictly worse for the dominant query. Accept the hot partition and
absorb it with a cache (§7.1).

**`path` is a materialized path and is the clustering key.** Ordering rows by path yields the tree in
pre-order, which means any subtree is a *contiguous range*: `WHERE post_id = ? AND path >= 'X' AND
path < 'X<max>'`. §7.1 argues this against the alternatives.

**`votes` is partitioned by `user_id`, not by `thing_id`.** The query on the read path is "which of these
25 posts have I voted on," a batch lookup inside one user's partition. Partitioning by thing would make
that a 25-way scatter *and* would put every vote on a viral post into one partition. Partitioning by voter
distributes the vote write load perfectly uniformly by construction — the single most elegant consequence
in this schema, and it comes free. The query it gives up, "who voted on this," is needed only by offline
abuse analysis, which can afford a scan.

**Counters are denormalized onto the row and treated as approximate.** The authoritative count is derivable
from `votes` and is never computed on the read path.

**Listings are derived, disposable Redis structures**, exactly as in Chapter 20 — except that there are
three million of them rather than five hundred million, and each is read by every subscriber rather than
by one person.

---

## 6. Architecture, derived

### Attempt 1: fanout on write, and why it evaporates

Try Chapter 20's design honestly. On submission, look up the community's subscribers and append the post
to each subscriber's home listing. From §3 that is ~34.5 million writes per second, because subscriber
counts are power-law distributed exactly like follower counts and the largest communities are larger than
the largest accounts. Worse than Twitter, from fifty times less content.

But the interesting result is not that fanout is expensive. It is that **fanout is unnecessary**, and the
reason is precise.

In Chapter 20 a reader's timeline is a merge of ~200 streams, one per followed *account*. No two readers
follow the same 200 accounts, so the merge is unique to that reader: computing it produces a result nobody
else can use, and there are 500 million readers. The only way to amortize an unshareable merge is to
compute it once and store it — which is exactly what fanout-on-write is.

Here a reader's home is a merge of ~50 streams, one per subscribed *community*. There are three million
communities and fifty million readers, and each community's listing is read by every one of its
subscribers. The merge inputs are **shared**, so computing a community listing once serves millions of
readers, and the per-reader merge is 50 cheap reads against near-certain cache hits.

State the principle, because it transfers: **fanout-on-write is required only when the merge is
unshareable. Materialize at the unit of sharing.** Chapter 20's unit of sharing is the individual reader,
so it needs 500 million materializations; here it is the community, so it needs three million, and they
are two orders of magnitude smaller each (§3: 5 GB against 6.4 TB). Chapter 21, §7.4 makes the same move
for Stories and reaches the same conclusion from the other direction.

### Attempt 2: compute "hot" at read time

Store posts; on a listing read, run `SELECT ... WHERE community_id = ? AND created_at > now() - 7d ORDER
BY hot(votes, age) LIMIT 25`.

`hot` depends on `now()`, so it cannot be indexed and the database must scan and sort the window on every
request. A large community produces 5,000 posts/day, so the window is 35,000 rows scanned and sorted, at
500,000 listing reads/sec. Dead by three orders of magnitude.

### Attempt 3: materialize the ranking — and hit the time problem

Keep post IDs in a sorted set scored by the hot value. Reads become `ZREVRANGE`, which is O(log N + 25) and
trivially fast. But if the score depends on *age*, every score is wrong the moment the clock advances, and
keeping them correct means rescoring every post in every listing on a tick:

```
3M communities × 1,000 posts, rescored every minute = 3 × 10^9 score updates/minute
```

Dead again — and dead for a reason that has nothing to do with scale and everything to do with the shape
of the ranking function.

### Attempt 4: make the score independent of the clock

Choose a ranking function whose value changes only when the vote count changes. §7.2 derives it; the
result is

```
score = log10(max(|U − D|, 1)) + sign(U − D) × (t_submitted − t_epoch) / 45000
```

The age term is a function of *submission time*, a constant per post, rather than of elapsed age. Newer
posts simply start with a larger constant. Relative order is identical to a decaying formulation, and the
update rate collapses from `posts × ticks` to `votes`: **5,000 score updates per second at peak, not three
billion per minute.**

### Attempt 5: absorb the vote write rate

Each vote now implies four writes: the durable `votes` row, the counter, the recomputed score, and the ZSET
update. At 5,000 votes/sec that is 20,000 writes/sec — fine, but wasteful, because most of it changes
nothing anyone can see. A post going from 4,000 to 4,010 net votes moves `log10` by 0.0011, which against a
time term advancing 1.0 per 12.5 hours is worth about 50 seconds of equivalent age. Invisible.

But the *first* ten votes move `log10` by a full 1.0 — the same as 12.5 hours of newness — so a new post's
rank is extremely sensitive.

**So make the update policy adaptive:** below ~100 votes, recompute the score on every vote; above it,
batch through a stream and recompute every few seconds or every K votes. This is a derived decision, not a
default, and the derivation is the two numbers above.

### Attempt 6: the home merge

Read 50 community ZSETs, take the top ~50 from each, merge, return 25. This works mechanically and fails
editorially: hot scores are not comparable across communities. A post with 40,000 votes in a huge
community scores ~4.6 + t; a post with 12 votes in a small one scores ~1.08 + t. The largest subscribed
community wins every slot. §7.5 fixes it with normalization and quotas — which is the per-source quota
idea of Chapter 22, §7.1, arriving here from a completely different direction.

### Final architecture

```
POST /posts ─► posts store ─► listing writer ─► Redis ZSET  hot:{community}, new:{community}
                                                            (capped 1,000)

POST /vote ─► votes store (partitioned by voter) ─► Redis INCR counter
                    └─► Kafka ─► score aggregator ─┬─► adaptive ZADD hot:{community}
                                                   └─► periodic authoritative recount

GET /r/{c}/posts ─► Redis ZREVRANGE ─► hydrate posts ─► filter (removed, blocked) ─► JSON

GET /feed ─► subscriptions ─► 50 × ZREVRANGE ─► normalize + quota merge (§7.5)
                                             ─► hydrate ─► JSON

GET /posts/{id}/comments ─► thread cache (rendered top-of-thread, 5 s TTL)
                             └─ miss ─► single-partition range scan by path
                                      ─► budgeted traversal + `more` tokens (§7.4)
```

---

## 7. Deep dives

### 7.1 The comment tree: materialized path, and why

Decide from the access pattern, not from taste. The dominant read is *"give me the best N of this tree,
depth-limited, in traversal order"*; the second is *"expand this subtree."* Both are subtree reads — the
first is the subtree rooted at the post. Writes are **always leaf inserts**: a reply creates a new leaf,
nodes are never moved, and deletion is logical because a deleted comment with children must remain as a
tombstone. That set of properties decides the layout.

**Adjacency list** (`parent_id` only). O(1) insert and the simplest schema. Reading a tree of depth *d*
takes *d* sequential round trips, or a recursive CTE that pushes the recursion into the database and does
not shard. A 5,000-comment thread of depth 30 is 30 round trips at 2 ms — 60 ms of pure network before any
work — against a 300 ms budget that also has to hydrate and sort. Fatal on the dominant read.

**Nested set** (each node stores left/right bounds). A subtree is exactly one range query, which is what
we want. But inserting a leaf requires renumbering every node to its right — up to 100,000 row updates for
one reply in a large thread, at 700 comments/sec. Nested sets are for trees read constantly and written
almost never, like a category taxonomy. Disqualified by the write pattern.

**Closure table** (a row per ancestor-descendant pair). Exact ancestor queries, O(depth) rows per insert,
but storage grows with the sum of depths and it provides no natural ordering, so the traversal order still
has to come from somewhere else. More machinery than the problem needs.

**Materialized path** — the node stores the ordered concatenation of its ancestors' keys. Insert is O(1):
copy the parent's path and append your own segment. A subtree is a contiguous prefix range. Depth is
`len(path)`. Sorting by path *is* pre-order traversal. Every property the access pattern asked for.

Choose it, and name the costs honestly:

- **Path length grows with depth**, so depth must be capped — around ten visible levels, with deeper
  replies reachable through a "continue this thread" link that starts a new root. Use fixed-width base-36
  segments so lexicographic order equals numeric order; six characters per level and ten levels is 60
  bytes per row, which is acceptable against a 400-byte row.
- **The path encodes insertion order, not score order.** Sorting siblings by "best" cannot be done by the
  index; you fetch a bounded slice by path and sort in the application. This is the real cost: the range
  scan's order and the presentation order are different, so you must over-fetch relative to what you
  return. It is tolerable only because the slice is bounded (§7.4).
- **A deleted comment cannot be removed** if it has children, because their paths contain its key.
  Removal is a content tombstone: the row stays, the body becomes `[removed]`, the subtree survives.

**Cache the rendered top-of-thread.** A front-page thread's first page is requested thousands of times per
second, and recomputing the same sort of the same 200 comments each time is waste. Cache the serialized
result per `(post_id, sort)` with a short TTL rather than invalidating on write: at 5 seconds, a thread
receiving 50 comments a minute is at most four comments stale, which no reader can perceive, and a TTL is
immune to the invalidation storms that a write-triggered scheme produces on exactly the hottest threads
(Chapter 02, §5).

### 7.2 Deriving the hot score

Write down what the function must satisfy before writing the function.

1. More net votes ranks higher.
2. Newer ranks higher, all else equal.
3. The marginal value of a vote must **decrease** with vote count — otherwise a 10,000-vote post from
   yesterday outranks everything posted today, permanently, and the front page freezes.
4. *(The systems requirement, and the one usually forgotten.)* A post's score must not change with the
   passage of time, only with new votes. Otherwise the update rate is O(posts × ticks), which §6 attempt 3
   showed is three billion updates a minute.

(1) and (3) together demand a concave function of votes. Logarithm is the canonical choice and base 10 is
the legible one: the first 10 votes are worth as much as the next 90, which are worth as much as the next
900. So the vote term is `log10(max(|U − D|, 1))`.

(2) and (4) are the interesting pair, because they look contradictory. A decaying form `g(votes) − d(age)`
satisfies (2) and violates (4) — every score changes every second. But `age = now − t_submitted`, and
`now` is *shared by every post being compared*. Subtracting the same quantity from every score changes no
comparison. So drop it and keep only the constant part:

```
score = log10(max(|U − D|, 1))  +  sign(U − D) × (t_submitted − t_epoch) / T
```

Ordering is identical to the decaying version, and scores are now write-once-per-vote. That is the whole
trick, and it is worth presenting as a derivation rather than as a formula, because the derivation is what
generalizes: **when a score decays uniformly for all items, fold the decay into a per-item constant.**

**Solving for T.** `T` sets the exchange rate between newness and votes: one unit of the time term equals
one order of magnitude of votes. With `T = 45,000 s = 12.5 hours`, a post needs ten times the votes to
outrank one that is 12.5 hours newer. Check it: a 12-hour-old post with 1,000 votes scores `3.0 + x`; a
brand-new post with 100 votes scores `2.0 + x + 0.96 = 2.96 + x`. Nearly tied — a plausible product
intent, and `T` is the single dial that sets how fast the front page turns over. `T = 6h` produces a
frantic page dominated by the last few hours; `T = 48h` lets a very popular post sit at the top for two
days.

**The `sign` term** makes heavily downvoted posts sink and inverts the newness bonus for them, so they
fall permanently rather than resurfacing. It also produces a genuine wart: a post at exactly zero net votes
gets `sign = 0`, so its score is `log10(1) = 0` regardless of when it was posted, and it sorts below
everything. Being able to name a discontinuity in a famous algorithm is a good sign that you derived it
rather than recalled it.

**Comments need a different function.** A comment's job is not to be new; it is to be good, and comments
have wildly unequal exposure — a top-level comment gets a hundred times the impressions of a fifth-level
reply — so a raw ratio is meaningless at low counts. Use the **Wilson score lower bound**: the lower end of
a confidence interval on the true positive proportion. One upvote and no downvotes yields ≈ 0.21; ten and
zero yields ≈ 0.72; a hundred up and ten down yields ≈ 0.83. It penalizes uncertainty, so a single-vote
comment cannot beat a hundred-vote one, which is exactly the property ratio-sorting lacks. Its cost is that
it is exposure-blind and therefore biased toward early comments, which accumulate votes simply by having
been visible longer — the same position bias analyzed in Chapter 22, §7.5, and mitigated the same way, by
randomizing initial ordering for new threads.

### 7.3 Vote counting at scale, and vote fuzzing

**Counting.** 5,000 votes/sec globally is small; the concentration is what matters. A front-page post can
take 500 votes a minute — about 8 per second on one row, and several times that in a site-wide event.
Three options. A durable `UPDATE posts SET ups = ups + 1` holds a row lock for well under a millisecond, so
8/sec on one row is genuinely fine on a single node — but it is a durable write per vote and it does not
shard. A Redis `INCR` per post costs microseconds and gives read-your-writes immediately. A stream
aggregation (Chapter 03) recomputes counts in windows and writes them back periodically.

**Use all three in their proper roles.** The vote row is written durably and is the authority — and note
that it lands in the *voter's* partition, which is uniformly distributed, so the authoritative write has no
hot partition at all. The Redis counter serves the read path. A periodic job recomputes the true count from
`votes` and corrects drift. The cost, which must be stated: two representations of the same number will
diverge, so the reconciliation job is mandatory, must be idempotent, and must be monitored — a
reconciliation job nobody watches is the same as no reconciliation job.

**Vote fuzzing** — deliberately perturbing the displayed count — looks like a product quirk and is
actually load-bearing.

- **It removes an oracle from manipulators.** If a spammer can see the exact effect of each sockpuppet
  vote, they can determine which accounts are shadowbanned and which votes were discounted, and iterate
  until they find accounts that work. Fuzzing makes the anti-abuse system's decisions unobservable, and
  anti-abuse systems that can be probed are eventually defeated.
- **It converts an exact-counting problem into an approximate-counting problem**, which is the systems
  point. Once the displayed number is admittedly approximate, you no longer need a linearizable counter,
  cross-region consensus on vote order, or synchronous durability on the counter path. Almost every
  simplification in this section is licensed by fuzzing.

Two costs. Users notice and object, and the count stops being auditable by anyone outside. And the fuzz
**must be stable** — derived deterministically from the post ID and a coarse time bucket — or the number
jitters on every refresh and looks broken. That stability requirement is easy to miss and immediately
visible when missed. Note also that a viewer's *own* vote state is never fuzzed: it comes from their own
`votes` partition and is exact, which is why the arrow turns orange instantly even though the number
beside it is a polite fiction.

### 7.4 Paginating a deeply nested tree

A thread has 50,000 comments and the response can carry 200. Which 200, and what is page two?

The naive answer — flatten in pre-order, take the first 200, cursor on the last path — produces a page that
is one exhaustive dive into the first top-level comment and nothing else. Pre-order is the right *storage*
order and the wrong *presentation* order.

What is wanted is a **breadth-preferring, score-ordered truncation**: the best top-level comments, each
with a few levels of their best children, and an explicit marker everywhere the server stopped. So:

**The tree does not have one page two. It has a page two per truncation point.** That is why §4's response
carries a `more` array rather than a single cursor, with one entry per truncated node giving the parent,
the hidden descendant count, and a continuation token. This is the single most important structural insight
in the deep dive and it falls directly out of the shape of the data.

The traversal is a budgeted best-first walk: seed a priority queue with the top-level comments ordered by
the requested sort, pop the best, emit it, push its children with a depth penalty so breadth wins over
depth, and stop at the budget. Every node whose children were not fully emitted contributes a `more` entry,
which requires the `descendant_count` column from §5 — maintained by incrementing the counter of each
ancestor on insert, which is O(depth) writes and bounded because depth is capped (§7.1).

**The size problem, quantified.** Building the tree in memory requires fetching more than you return. A
50,000-comment thread is 20 MB per request, which at any real QPS is not a thing you do. So:

```
threads under ~5,000 comments (≈99.9% of all threads):  fetch the whole partition, ~2 MB, build in memory
threads above that:                                     fetch depth ≤ 2 with a limit, serve the
                                                        cached top-of-thread blob, expand on demand
```

The distribution is extreme — the vast majority of threads have fewer than a hundred comments and cost
nothing — and essentially all of this machinery exists for a fraction of a percent of threads. Say so; it
is the same power-law-with-two-ends lesson as Chapter 20, and the correct response is the same: handle the
common case simply and the tail specially, rather than designing everything for the tail.

**Continuation tokens must be stable under concurrent inserts.** "Children of X with path after P" is
stable, because new siblings receive paths that sort after P and therefore appear on a later page, never
duplicated onto an earlier one. "Offset 200 within X" is not. This is the same argument as the listing
cursor in §4, and the same argument as Chapter 20's rejection of offset pagination — a keyset cursor over
an append-ordered key is the general answer whenever the underlying set grows during pagination.

### 7.5 The home merge across communities

Fifty ZSET reads, merge, return 25. The mechanical part is trivial; the ranking part is not, because hot
scores are **not comparable across communities**. The vote term is `log10` of an absolute count, and
absolute counts differ by three orders of magnitude between a 40-million-subscriber community and a
5,000-subscriber one. Merging raw scores gives the largest community every slot, and a user who subscribes
to forty niche communities and one default sees only the default.

Two fixes, and they compose.

**Normalize within the community.** Replace the raw score with the post's standing *relative to its own
community's recent distribution* — its rank percentile within that community's listing, or its score
divided by a rolling median of the community's top-N. Percentile rank is cheap because the listing is
already sorted: the position in the ZSET is the answer. The cost is that a genuinely enormous story in a
huge community no longer dominates the way it should, so a small absolute-magnitude term usually gets
added back, and now there is a weight to tune.

**Quota per community.** Cap any one community at, say, three of twenty-five slots. Crude, effective,
auditable — and structurally identical to Chapter 22, §7.1's per-source quotas, arrived at from an entirely
different problem. The cost is symmetric: a user subscribed to one active community and forty-nine dormant
ones ends up with a thin page, so the quota must be a cap that relaxes when other sources cannot fill.

### 7.6 Removal, tombstones, and read-time filtering

Moderation is a first-class read-path concern here in a way it is not in a personal feed, because a
moderator's removal must take effect immediately across every reader.

**Posts** are removed by deleting the ID from the community's listing ZSET *and* filtering at hydration —
both, because the ZSET is a derived structure that can be rebuilt from the posts table and a rebuild would
otherwise resurrect the post. This is Chapter 20, §7.3's read-time filter, applied to a much smaller number
of much more shared structures, which makes it cheaper: there are thousands of listings to fix, not
millions of timelines, so eager removal is affordable here even though it is not there.

**Comments cannot be deleted at all if they have children**, because the children's materialized paths
contain the removed comment's key (§7.1). The row stays, the body is replaced, and the subtree survives. A
consequence people find surprising and that follows directly from the storage choice: choosing materialized
paths chose tombstones.

**Community-level visibility** — private, quarantined, banned — is an authorization check applied to the
merge, over 50 communities, on every home read. Cache the subscriber's resolved visibility set per session;
recomputing it per request would put a permission lookup on the hottest path in the system.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Listing ZSETs lost | Community pages empty | Rebuild from the posts table: scan the last 7 days per community and rescore. Bounded (§3: 5 GB total) and should be a routine, tested job |
| Score aggregator lags | Rankings freeze at the last computed scores | Listings stay readable and merely stale — the failure mode the design was chosen for. Alert on consumer lag |
| Redis counter and `votes` diverge | Displayed counts drift from truth | Periodic authoritative recount; fuzzing already licenses small error, so drift is not user-visible until it is large |
| Hot thread partition | One partition serves a large share of comment reads | Rendered top-of-thread cache with a short TTL absorbs it; the partition sees cache fills, not reads |
| Very large thread (100k+ comments) | 40 MB partition, expensive reads | Depth-limited fetch plus cached top-of-thread (§7.4); never build the full tree in memory |
| Vote brigading | Ranking manipulated | Fuzzing removes the attacker's feedback signal; vote discounting applied offline so it cannot be probed |
| Community listing hot key | One Redis key at very high read rate | Replicate the key; short-TTL in-process cache on the listing servers — safe, since the data is already approximate |
| Moderator removal not propagated | Removed content still visible | Remove from the ZSET *and* filter at hydration; never rely on only one |

**Monitoring:** score-aggregator consumer lag (the leading indicator for ranking staleness), listing read
p99 and comment-page p99 tracked separately because they have different budgets and different causes,
thread-cache hit rate, the counter-vs-`votes` reconciliation delta (a rising delta means the aggregator is
losing events), comment insert latency by thread size (this is where large-partition pain shows first), and
the distribution of merge results per home page — if one community occupies more than its quota, the
normalization in §7.5 has drifted.

---

## 9. Common mistakes

1. **Applying Chapter 20's fanout reflexively.** It is not merely unnecessary here, it is 150× worse than
   Twitter's, because communities are larger than follower lists. The interview is checking whether you
   know *why* fanout exists, not whether you can recite it.
2. **Not noticing that the merge inputs are shared.** This is the whole reason read-time assembly works
   here and fails in Chapter 20, and a design that does not say it has not been derived.
3. **A ranking score that decays with elapsed time**, which makes every score stale on every clock tick and
   turns the update rate into posts × ticks. Fold the decay into a submission-time constant.
4. **Storing comments in an adjacency list** and paying *d* round trips per thread read, or in a nested set
   and paying up to 100,000 row updates per reply.
5. **Partitioning comments by `comment_id`**, turning every thread read into a cluster-wide scatter-gather
   in order to avoid a hot partition that a cache handles.
6. **Partitioning votes by `thing_id`**, which creates a hot partition on exactly the viral posts and makes
   "have I voted on these 25 posts" a 25-way scatter.
7. **Treating vote counts as exact**, and thereby requiring a linearizable counter that nothing in the
   product actually needs. Fuzzing is what buys the simplification.
8. **Returning a nested JSON tree** whose size and depth are unbounded, instead of a flattened list with
   depth markers and explicit continuation tokens.
9. **Assuming a comment tree has one page two.** It has one per truncation point, and a design with a
   single linear cursor has not thought about what the user actually sees.

---

## 10. Variants

**Hacker News.** A single global community, so the merge of §7.5 disappears entirely and there is exactly
one listing. That listing is small enough (a few thousand items) to rescore on a timer, which means the
score can use a straightforward decay form — `(votes − 1)^0.8 / (age + 2)^1.8` — rather than needing §7.2's
clock-independent trick. A useful illustration that the trick is a scale accommodation, not a law.

**Stack Overflow.** The tree is only two levels — answers, with comments beneath them — so §7.1 and §7.4
collapse to a simple ordered fetch. Ranking is by score with accepted-answer pinning and almost no time
decay, because the product's goal is a durable reference rather than a fresh front page. Different
objective, different formula, same machinery.

**Discourse and classic forums.** Threads are linear and paginated by post number, which is a stable
integer, so pagination becomes trivial and the entire comment-tree deep dive evaporates. Worth naming
because it shows how much of this chapter's complexity is bought by the decision to nest replies.

**Facebook Groups.** Communities plus a personalized ranked feed: this chapter's materialization with
Chapter 22's ranking layered on top, and the merge in §7.5 becomes a full candidate-generation-and-ranking
problem rather than a normalization heuristic.

**Twitter replies and comment sections under articles.** A tree with vastly more skew — a few threads
enormous, the rest trivial — and often only two rendered levels. The interesting variation is that ranking
the top reply matters far more than paginating the tail, so effort moves from §7.4 to §7.2.

---

## 11. Further reading

- Chapter 20 for the fanout this chapter declines to do; Chapter 22, §7.1 for per-source quotas, which
  §7.5 rediscovers
- Amir Salihefendic, "How Reddit Ranking Algorithms Work" — the clearest public walkthrough of the hot and
  Wilson formulas, written against Reddit's open-sourced ranking code
- Evan Miller, "How Not To Sort By Average Rating" — the canonical derivation of the Wilson score lower
  bound and why ratio sorting fails at low counts
- Joe Celko, *Trees and Hierarchies in SQL for Smarties* — the reference treatment of adjacency lists,
  nested sets, and materialized paths, with the write-cost analysis §7.1 summarizes
- The Hacker News ranking function, published in the `news.arc` source, for the decay-form contrast in §10
