# Chapter 72 — Leaderboard

> **Prerequisites:** Chapters 01 (partitioning), 02 (caching), 03 §4 (idempotency), 04 §5 (t-digest)
> **Patterns:** sorted sets and rank queries, sharded ranking, approximate rank, write coalescing

---

## 1. The problem

A game has a hundred million players, each with a score. The client shows three things: the global top
100, the player's own rank ("you are #4,281,993"), and the handful of players immediately above and below
them. Add daily, weekly, and all-time boards, and per-region boards, and the design is complete.

It looks like one query with three parameters. It is not.

**The property that makes it hard:** rank is a *global aggregate over every player*, so it cannot be
derived from any single player's state, and yet the top-N query and the arbitrary-rank query — which look
like the same query with a different offset — have completely different cost profiles. Returning the top
100 is cheap under almost any design, because it touches 100 rows. Returning the rank of the player at
position 4,281,993 requires knowing how many players are above them, which in the naive formulation means
counting four million rows, per request, for a number that changes continuously.

Everything below is about keeping the first query cheap while making the second one *affordable* rather
than exact — and about being precise about where that boundary sits.

---

## 2. Requirements

### Functional

1. Submit or update a player's score.
2. Return the top N for a board, N ≤ 100.
3. Return a given player's rank and score.
4. Return the players immediately around a given player.
5. Boards scoped by time window (daily, weekly, all-time) and by region.

### Non-functional

- **Scale** — 100 million registered players, 50 million daily active, up to 5 million concurrently in a
  match with scores changing every second.
- **Top-N latency** — p99 under 50 ms. It is on the game's home screen.
- **Rank latency** — p99 under 100 ms. This is the expensive one.
- **Freshness** — the top-N may lag by a second; a player's own score must be reflected immediately after
  their match ends, because they will look.
- **Correctness** — the top 100 must be exactly right, because it is a public ranking with prizes
  attached and players screenshot it. An arbitrary mid-table rank may be approximate, and §7.3 is about
  making that a deliberate product decision rather than a bug.
- **Determinism** — equal scores must produce a stable, explainable order (§7.6). "Arbitrary" is not an
  acceptable answer to a player who is tied for a prize.
- **Durability** — the ranking structure is a derived index. The authoritative score lives elsewhere.

### Explicitly out of scope

Validating that a submitted score is legitimate — anti-cheat is a separate system, and this chapter
assumes the score arriving at the API has already been authorized by the game server. Matchmaking
(Chapter A1). Friend-scoped leaderboards, which turn out not to need any of this machinery (§10).

---

## 3. Estimation

**Memory for one sorted set**

A Redis sorted set is a skiplist plus a hash table, so each member is stored twice by reference:

```
member string "p:123456789"  ≈ 12 B + sds header/alloc overhead  ≈ 28 B
skiplist node: score (8 B) + backward ptr (8 B) + avg 1.33 levels × (ptr 8 B + span 8 B) ≈ 38 B
dict entry: key ptr + value ptr + next ptr + bucket share             ≈ 40 B
                                                                  ─────────
                                                          per member ≈ 106 B → call it 110 B
100,000,000 × 110 B ≈ 11 GB, ~13 GB with allocator fragmentation
```

Worth saying out loud, because it contradicts the reflex: **one board of a hundred million players fits
on a single large instance.** Thirteen gigabytes is not a sharding argument.

**Then multiply, which is the actual argument**

```
5 game modes × all-time (100M members)  = 500M entries ≈ 55 GB
5 modes × weekly  (~30M active members) = 150M entries ≈ 17 GB
5 modes × daily   (~20M active members) = 100M entries ≈ 11 GB
                                                        ───────
                                                         ≈ 83 GB
```

Eighty-three gigabytes, plus replicas, plus headroom for the fragmentation that a workload of constant
`ZADD`s produces. That exceeds what one instance should hold, so the boards are sharded — but note the
reason, because it will be asked: **not one enormous board, but many boards.**

**Write rate — the constraining number**

```
5,000,000 concurrent players × 1 score update/s      = 5,000,000 ZADD/s
each update must land on 3 boards (daily, weekly, all-time) = 15,000,000 ops/s
Redis is single-threaded; ZADD into a 100M-member set is O(log N) ≈ 27 skiplist
comparisons plus a dict write, so budget ~100,000 ops/s per instance
15,000,000 / 100,000                                  = 150 instances, for writes alone
```

**One hundred and fifty Redis instances to maintain a leaderboard nobody is reading that fast.** This is
the number that shapes the design, and §7.4 removes almost all of it by refusing to write every update.

**Read rate**

```
50M DAU × 10 leaderboard views/day = 500M/day ≈ 5,800/s average, ~20,000/s peak
```

Top-N is one identical query for every viewer, so a one-second cache collapses it to roughly one Redis
operation per second per board. Rank queries are per-player and therefore uncacheable, which makes 20,000
QPS of `ZREVRANK` the real read load — and §7.2 explains why that number gets multiplied by the shard
count if you are not careful.

---

## 4. API

```
POST /v1/boards/{board_id}/scores
       body: {score, match_id}            # player from the auth token
       -> 204

GET  /v1/boards/{board_id}/top?limit=100
       -> 200 {entries:[{rank, player_id, score}], as_of}

GET  /v1/boards/{board_id}/players/{player_id}
       -> 200 {score, rank, percentile, exact: true|false, as_of}

GET  /v1/boards/{board_id}/around/{player_id}?radius=5
       -> 200 {entries:[{rank, player_id, score}], center_rank, exact: true|false}
```

**The player comes from the token and the score from the game server, never from the client.** Stated
once, and it is the difference between a leaderboard and a random number generator.

**`match_id` is the idempotency key.** The submission path is at-least-once like everything else, and
without a key a retried submission for a cumulative board double-counts. §7.4 shows that the natural
Redis operation here — `ZADD GT`, which keeps the maximum — is idempotent on its own for best-score
boards, which makes the key belt-and-braces there and load-bearing for cumulative ones.

**`exact` is a first-class field, not an implementation leak.** A rank inside the exact tier (§7.3) is
guaranteed; a rank in the approximate tier carries a percentile whose error is bounded and known. A
client that renders both identically is fine; a client that awards a prize based on rank #4,281,993 is
not, and the field is what lets it tell the difference.

---

## 5. Data model

```
Redis sorted sets, one per board:
  lb:{mode}:alltime                       # e.g. lb:solo:alltime
  lb:{mode}:daily:2026-09-03              # TTL 48 h
  lb:{mode}:weekly:2026-W36               # TTL 16 d
  lb:{mode}:region:{region}:alltime
  member = player_id
  score  = packed double: (score << 23) | (SEASON_SECONDS - t_achieved)     # §7.6

  lb:{mode}:leaders                       # top 10,000 only, single instance, exact tier (§7.3)

Redis strings / hashes:
  hist:{board_id}      -> 1,000 quantile bucket boundaries + cumulative counts (~16 KB, §7.3)
  cache:top:{board_id} -> serialized top-100 JSON, TTL 1 s

Durable source of truth (Postgres / DynamoDB / the game's own store):
  player_scores(player_id, board_id, score, achieved_at, match_id)   PK (player_id, board_id)
```

**The sorted sets are a derived index, not the record.** Everything in them is reconstructible from
`player_scores`, which means Redis can be run without durability guarantees and a node loss costs a
rebuild rather than data — the same argument Chapter 20 §5 makes about materialized timelines, and it
licenses the same aggressiveness about keeping the whole structure in memory.

**The score field is a packed composite, not the displayable score.** §7.6 derives the bit budget. Note
the consequence now, so it does not surprise anyone later: a `ZSCORE` returns a number the player has
never seen, and every tool that inspects Redis directly must decode it. Document it next to the key
naming convention, because someone will run `ZRANGE ... WITHSCORES` at 3 a.m. and conclude the data is
corrupt.

---

## 6. Architecture, derived

### Attempt 1: a relational table with an index on score

```sql
SELECT player_id, score FROM scores WHERE board_id=? ORDER BY score DESC LIMIT 100;
SELECT COUNT(*)+1 AS rank FROM scores WHERE board_id=? AND score > ?;
```

**The first query is fine.** A descending index on `(board_id, score)` makes the top 100 an index scan of
100 entries. If the product only needed a top-N board, this would be the whole answer, and reaching for
Redis would be over-engineering.

**The second query is the failure.** `COUNT(*)` over an index range has no shortcut: the engine walks
every entry above the player's score. For a player at rank 4,281,993 that is four million index entries.
At a generous 10 million index entries scanned per second, that is roughly 400 ms for one player, and
several seconds for a player at rank 50,000,000. It cannot be cached, because the answer is different for
every player and changes on every write. At the required 20,000 rank QPS this is not slow, it is
impossible — the design fails at roughly 100 rank queries per second.

### Attempt 2: a Redis sorted set

```
ZADD   lb:solo:alltime GT <packed> <player>      # O(log N)
ZREVRANGE lb:solo:alltime 0 99 WITHSCORES        # O(log N + 100)
ZREVRANK  lb:solo:alltime <player>               # O(log N)  ← the whole point
```

`ZREVRANK` is O(log N) rather than O(N) because the skiplist stores, on every forward pointer, the
**number of nodes that pointer skips** — its span. Rank is accumulated during the descent instead of
counted afterwards (§7.1). For 10^8 members that is a few dozen pointer hops: microseconds, against the
relational plan's hundreds of milliseconds.

The rank problem is solved. Two new ones appear: §3's 83 GB across all boards, and §3's 15 million writes
per second.

### Attempt 3: stop writing so much, and cache the top-N

Coalesce score updates per player (§7.4) and the 15,000,000 ops/s becomes 500,000/s at a 30-second
coalescing window, or 42,000/s if scores are published only at match end. Cache the serialized top-100
for one second and 20,000 read QPS becomes one Redis call per second per board.

The write problem is now a rounding error. **Sharding is still required, but only for the 83 GB and for
blast radius** — and that distinction matters, because it determines *how* you shard.

### Attempt 4: shard, and lose something

Two shard keys, with different consequences:

- **By `hash(player_id)`.** Every player lives on exactly one shard. The global top-N is exactly the
  merge of per-shard top-Ns, because no player's score is split across shards. `ZREVRANK` on a shard
  returns rank *within that shard*, which is not the global rank.
- **By region.** The same property, plus data locality and per-region boards as a free product feature,
  plus the sharding is stable under player growth in a way hashing is not.

Either way: **top-N stays exact, and a single `ZREVRANK` stops being the global rank.** §7.2 works
through exactly what is recoverable and at what cost.

### Attempt 5: two tiers for rank

Global rank splits into two regimes that deserve different mechanisms:

- **The top ~10,000**, where players care about the exact integer and prizes depend on it. Maintain a
  small consolidated `leaders` sorted set on one instance, fed by the shards. `ZREVRANK` on it is exact
  and cheap because the set is tiny.
- **Everyone else**, where the honest product requirement is "top 3%," not "#4,281,993." Serve from a
  quantile histogram (§7.3): 16 KB, broadcast to every query server, rank answered locally with no
  network call at all.

### Final architecture

```
 game server ─► POST /scores ─► ingest ─► coalescing buffer (30 s or match-end, §7.4)
                                   │              │
                                   ▼              ▼
                        durable player_scores   Kafka (compacted, key=player_id)
                        (source of truth)         │
                                                  ▼
                        ┌──────────────── board writer ────────────────┐
                        │  ZADD GT into: daily | weekly | alltime      │
                        └───┬─────────────────┬──────────────────┬─────┘
                            ▼                 ▼                  ▼
                     shard A (region us)  shard B (eu)     shard C (ap)     ← 83 GB total
                            │                 │                  │
              ┌─────────────┴─────────────────┴──────────────────┘
              │  every 1 s: per-shard top-100                every 60 s: per-shard
              ▼                                              bucket counts
   ┌──────────────────────┐                        ┌───────────────────────────┐
   │ merge → exact global │                        │ quantile histogram        │
   │ top-100 (§7.2)       │                        │ 1,000 buckets, 16 KB      │
   │ → cache:top, TTL 1 s │                        │ → pushed to query servers │
   │ → lb:leaders (10k)   │                        └───────────────────────────┘
   └──────────┬───────────┘                                    │
              ▼                                                ▼
        query service:  top-N from cache | rank ≤ 10,000 from lb:leaders (exact)
                                         | rank > 10,000 from histogram (approximate)
```

---

## 7. Deep dives

### 7.1 Why `ZREVRANK` is O(log N) and `COUNT(*)` is O(N)

Both structures are ordered. Only one of them counts as it goes.

A skiplist is a linked list with additional express lanes; each node appears in level `i` with
probability `p^i` (Redis uses `p = 0.25`, up to 32 levels). Redis stores with every forward pointer the
**span**: how many level-0 nodes that pointer jumps over.

```
L3  head ────────────────────────────────────► [ 61 ] ──────────────► NIL
              span = 6                                   span = 3
L2  head ──────────────► [ 88 ] ──────────────► [ 61 ] ──► [ 40 ] ──► NIL
        span = 3                    span = 3      span=2      span=1
L1  head ──► [ 95 ] ──► [ 88 ] ──► [ 74 ] ──► [ 61 ] ──► ... ──────► NIL
        1          1          1          1
```

To find a member's rank, descend from the top level, moving forward whenever the next node still sorts
before the target, **adding each traversed pointer's span to a running total.** When you land on the
target, the accumulated span *is* its rank. The nodes skipped over are never visited — their count was
precomputed and stored on the pointer.

This is the entire trick, and it is what a B-tree index does not give you. A standard B-tree stores keys
in order but not subtree cardinalities, so "how many entries precede this one" requires walking them.
(Counted B-trees exist and solve this; mainstream relational engines do not maintain them, which is why
`COUNT(*) WHERE score > ?` is a scan.) Span maintenance costs a little work on every insert and delete —
it is why `ZADD` is meaningfully more expensive than `SET` — and that cost is what §3's 100,000 ops/s
budget already accounts for.

`ZREVRANGE 0 99` is the same descent to the boundary followed by 100 sequential level-0 hops: O(log N + M).

### 7.2 Sharding: what stays exact, and what does not

This is the heart of the chapter, and the boundary is sharper than most answers make it.

**Top-N survives sharding exactly.** Shard by player, so each player's score lives on exactly one shard.
The global top 100 is a subset of the union of the per-shard top 100s — an entry in the global top 100
must be in the top 100 of its own shard, because its shard contains a subset of all players. So: fetch
`ZREVRANGE 0 99` from each of the `S` shards, merge `S × 100` entries, take 100. **Exact, not
approximate.**

Compare Chapter 71 §7.4, where merging per-shard top-K is *approximate*. The difference is not the merge;
it is what was partitioned. Here each entity's complete value lives on one shard. There, an item's
*count* is split across shards, so a shard's local view of an item is a fragment and a fragment can sit
below the reporting threshold on every shard while the sum is large. **Merging local top-K is exact when
the entity is partitioned and approximate when the entity's value is split.** Say that sentence and both
chapters are covered.

**Global rank does not survive sharding as a single operation** — but it is not lost either, and the
usual claim that it becomes "impossible" is wrong. It becomes *expensive*:

```
global_rank(p) = 1 + Σ_s  ZCOUNT(shard_s, (my_packed_score, +inf)
```

`ZCOUNT` is O(log N) per shard, the same span trick as §7.1. So an exact global rank is `S` parallel
O(log N) operations. With `S = 16` that is 16 microsecond-scale calls. It works. Three reasons it is
still the wrong default:

1. **Fan-out amplification.** 20,000 rank QPS × 16 shards = 320,000 Redis operations per second devoted
   entirely to ranks — three instances' worth of capacity for a number the product could have expressed
   as a percentile.
2. **Tail latency.** The response time is the *maximum* of 16 calls, not the average, so the p99 of the
   rank query is roughly the p99.9 of a single call. This is the standard scatter-gather tax and it is
   the reason a fan-out read is more expensive than its arithmetic suggests.
3. **It is not a consistent rank.** The 16 `ZCOUNT`s execute at 16 different instants while millions of
   scores are changing. The returned integer is a composition of sixteen snapshots and corresponds to no
   single moment in the system's history. It is "exact" in the sense that each term is exact, and
   *not* exact in the sense a player means when they compare their rank to a friend's.

Point 3 is the one worth making, because it converts the design decision from "we cannot afford exactness"
into "exactness at this position is not well-defined anyway, so we should spend the budget where the
answer is meaningful" — which is the top of the board, where §7.3's exact tier puts it.

**Regional sharding adds one wrinkle:** regions have wildly different populations, so shards are
unbalanced. Accept it (the small regions are cheap) or split large regions into sub-shards, which
preserves every property above because the merge is over shards, not over regions.

### 7.3 The mid-table rank, approximated with a score histogram

The requirement, honestly restated: a player at position four million wants to know they are "in the top
4%," and the UI displays a rank because ranks feel more concrete. Nobody can verify the last four digits
and nothing depends on them.

**Maintain a histogram of the score distribution.** Periodically — every 60 seconds is ample, since the
shape of a 100-million-player distribution does not move quickly — compute bucket boundaries and the
cumulative count below each. A rank query becomes a binary search over the boundaries plus a linear
interpolation inside the containing bucket.

```
1,000 buckets × (8 B boundary + 8 B cumulative count) = 16 KB
```

Sixteen kilobytes. Push it to every query server on a 60-second refresh and **the rank query stops
touching Redis at all** — it becomes a local binary search, sub-microsecond, infinitely scalable, and
immune to the fan-out tax of §7.2.

**Equal-width buckets are the wrong choice and equal-population buckets are the right one.** Score
distributions are heavily skewed: with 1,000 equal-width buckets spanning the score range, the buckets
near zero hold tens of millions of players and the buckets near the top hold none, so rank error is
enormous exactly where the population is. With **equal-population (quantile) buckets**, each bucket holds
exactly 1/1,000 of the players by construction, so:

```
rank error ≤ one bucket = 100,000,000 / 1,000 = 100,000 players = 0.1 percentile, uniformly
```

A bounded, uniform 0.1-percentile error is a specification you can put in the API contract. Compute the
boundaries with a mergeable quantile sketch — **t-digest (Chapter 04 §5), whose per-shard sketches merge
into a global one**, which is precisely the property needed here since the shards must be combined. This
is the same mechanism Chapter 73 §7.4 uses for percentile latency, and noticing that a leaderboard and a
metrics system need the identical primitive is a good thing to say out loud.

**The two-tier design follows.** Exact `ZREVRANK` for the top 10,000 out of a small consolidated
`leaders` set (a 10,000-member sorted set is about a megabyte and lives on one instance), histogram
below. The cost is a discontinuity at the boundary: a player oscillating around rank 10,000 sees their
rank flip between an exact integer and an interpolated one. Widen the exact tier until the boundary lands
somewhere nobody is watching, and interpolate the first histogram bucket against the exact tier's floor
so the two agree at the seam.

**"Players around me"** is the same split: inside the exact tier, `ZREVRANGE rank-5 rank+5` is exact and
trivial; below it, the honest implementation is to seek by *score* (`ZREVRANGEBYSCORE` around the
player's own packed score) rather than by rank, which is exact for the neighbors — you genuinely are next
to those players — even though the displayed rank numbers are approximate. That distinction is worth
stating: **neighbors are exact, their labels are not.**

### 7.4 Write amplification, and how much of it is real

From §3: 5 million concurrent players updating once per second, on 3 boards each, is 15 million `ZADD`/s
and roughly 150 Redis instances. Derive the reductions rather than asserting them.

**Coalesce per player.** A player's score changes 30 times in 30 seconds and only the last value matters
for a best-score board. Buffer in the ingest tier, keyed by `(player_id, board_id)`, and flush every 30
seconds:

```
5,000,000 / 30 s   = 167,000 updates/s
× 3 boards         = 500,000 ops/s   → 5 instances     (30× reduction)
```

**Publish only at meaningful events.** If scores are written only at match end, and a match lasts six
minutes:

```
5,000,000 players / 360 s = 14,000 match completions/s
× 3 boards                = 42,000 ops/s   → well under one instance   (350× reduction)
```

The trade is freshness, and it is a product question with a good answer: a leaderboard that updates
mid-match is a distraction, and nearly every shipped game publishes at match end. **The cheapest write is
the one the product does not need**, and this is the place to say so.

**The buffer is safe to lose**, which is what makes it acceptable. A crash drops up to 30 seconds of
buffered updates, but the durable `player_scores` write happens on the request path, so the sorted set
can be repaired from it. Derived index, again.

**`ZADD GT` makes the whole path idempotent.** `GT` updates the score only if the new value is greater
than the existing one, which means the operation is a **maximum**: idempotent, commutative, and
insensitive to ordering. A retried submission, a duplicate Kafka delivery, and an out-of-order arrival
from a slow shard are all harmless without any deduplication machinery. This is Chapter 03 §4's "prefer
absolute over relative" applied exactly, and forgetting the flag is the single most common production bug
in this design: plain `ZADD` is last-write-wins, so a stale in-flight update silently erases a personal
best that the player watched themselves earn.

Cumulative boards ("total points this season") cannot use `GT` — `ZINCRBY` is relative and therefore not
idempotent — so those genuinely need the `match_id` key from §4. Knowing which boards need which is the
point.

**Multi-board writes multiply, and Redis Cluster makes it worse.** Daily, weekly, and all-time are three
different keys, hashed to three different slots, likely on three different nodes: three round trips per
update, not one. Pipelining hides the latency but not the operation count. Hash-tagging the keys so they
share a slot (`lb:{solo}:daily`, `lb:{solo}:weekly`) collapses them into one node's `MULTI` — and
immediately conflicts with §7.2, because a hash tag forces every board for that mode onto one node, which
is the thing sharding existed to avoid. The resolution is to shard by player and hash-tag by player, so a
single player's three board entries co-locate and update atomically while the boards themselves remain
spread — at the cost that the per-shard top-N merge of §7.2 now covers every mode, which it already did.

### 7.5 Time-windowed boards

Daily, weekly, and all-time are **separate sorted sets with different lifecycles**, not one set with a
filter. A filter is impossible anyway: the sorted set has one score per member and cannot answer "best
score today" from a structure ordered by all-time best.

```
lb:solo:daily:2026-09-03    TTL 48 h    (keep yesterday for the "yesterday's winners" screen)
lb:solo:weekly:2026-W36     TTL 16 d
lb:solo:alltime             no TTL      never rebuilt from scratch in normal operation
```

Three consequences worth naming:

**Rollover is a timezone decision, and it will be asked.** Choose UTC and state it. Per-region local
midnight means twenty-four separate rollovers, a player who travels gets two "days" or none, and the
"daily winners" job runs all day. If the product insists on local time, define the board's timezone as a
property of the *region shard*, not of the player, so a board still has exactly one boundary.

**Creation is implicit and expiry is explicit.** The daily key is created by the first `ZADD` of the day
and its TTL is set at creation. Setting the TTL on every write resets it and the key never expires;
setting it only on creation risks a key with no TTL if the creating client crashes between the two
commands. Use `ZADD` followed by `EXPIRE NX` (or a small Lua script) so the TTL is applied exactly once.

**Recovery differs by board.** A lost daily board is recomputable from the retained score-submission log
as long as retention exceeds the window — one day of events is small. A lost all-time board must be
rebuilt from `player_scores`, which is 100 million rows: minutes of bulk `ZADD` with pipelining, during
which the top-N cache serves a stale but correct list and rank queries fall back to the last published
histogram. **The degraded mode is stale, not wrong**, which is the property to design for and to state.

### 7.6 Ties and deterministic ordering

Two players with the same score is not an edge case. On day one of a season, ten million players are tied
at zero; after a scoring change, thousands are tied at the cap.

Redis breaks score ties **lexicographically by member**, which is deterministic but meaningless, and
worse than meaningless in practice: it systematically advantages players whose ID sorts early. If prizes
attach to rank, that is a fairness bug with a paper trail.

The product rule is almost always "whoever reached the score first ranks higher." Encode it in the score
itself, because the sorted set has exactly one ordering key.

```
IEEE-754 double: integers are exact up to 2^53 = 9,007,199,254,740,992
max game score 10^9                   → needs 30 bits (2^30 = 1,073,741,824)
remaining for a tiebreaker: 53 - 30   = 23 bits = 8,388,608 distinct values
season length 90 days                 = 7,776,000 seconds  <  8,388,608     ✓

packed = score × 2^23 + (SEASON_SECONDS − seconds_since_season_start)
max packed = 10^9 × 8,388,608 = 8.39 × 10^15  <  9.007 × 10^15              ✓
```

It fits, with about 7% of the mantissa to spare — and showing that it fits is the point, because the
failure mode if it does not is silent: doubles beyond 2^53 round, so two distinct composites become equal
and ties reappear invisibly at the top of the board, which is the worst possible place.

Higher score wins because it dominates the high bits. Among equal scores, an earlier achievement yields a
larger `SEASON_SECONDS − t` and therefore a larger composite, so it ranks higher. Second-resolution
tiebreaking leaves ties within the same second; if that matters, spend more bits by capping the maximum
score, or accept member-lexicographic ordering as the third-level tiebreak and document it.

**The alternative, and why to reject it:** keep the raw score in the sorted set and resolve ties in the
application by fetching the tied group and sorting by `achieved_at`. It works and it is more readable —
until day one, when the tied group at score zero has ten million members and the query returns all of
them. Any tie-breaking scheme whose cost scales with the size of the tie is unusable on a leaderboard,
because leaderboards have enormous ties by construction.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Redis shard loss | That shard's slice of every board is gone | Sorted sets are a derived index; rebuild from `player_scores`; serve stale top-N cache and last histogram meanwhile |
| Coalescing buffer crash | Up to 30 s of score updates not yet in Redis | Durable write happens on the request path; a repair job re-`ZADD`s from `player_scores` |
| Plain `ZADD` instead of `ZADD GT` | A stale update erases a personal best | Enforce in a shared client wrapper; alert on any score that decreases on a best-score board |
| Packed score exceeds 2^53 | Silent rounding, ties reappear at the top | Assert the bound at write time; fail the write loudly rather than corrupting order |
| One shard slow | Rank fan-out p99 tracks the slowest shard | Histogram path avoids the fan-out entirely; exact tier is a single small instance |
| Histogram stale after a scoring change | Percentiles wrong across the board | Refresh on a 60 s cycle; force a rebuild on any scoring-rule deploy |
| Daily key created without a TTL | Unbounded key growth, silent | `EXPIRE NX` at creation; alert on any `lb:*:daily:*` key without a TTL |
| Hot key on a top-N read | One node saturated by identical reads | 1-second cache in the query tier collapses ~20,000 QPS to ~1 op/s |

**Monitoring:** `ZADD` rate per shard against the 100,000 ops/s budget; coalescing ratio (updates
received ÷ updates written — a collapse toward 1 means the buffer is being bypassed); top-N cache hit
rate; rank-query mix between the exact tier and the histogram tier; histogram age; memory per shard with
a projection to the instance limit, since sorted sets grow monotonically for all-time boards and the
failure mode there is a hard eviction rather than a slowdown.

---

## 9. Common mistakes

1. **Computing rank with `COUNT(*)`.** Correct, and O(N) per query with no cache. The point of a sorted
   set is not that it is fast at ordering — indexes order fine — but that it maintains **spans**, which
   is what makes rank O(log N).
2. **Claiming exact global rank after sharding by player.** Per-shard `ZREVRANK` is a rank within the
   shard. The global version is a scatter of `ZCOUNT`s, which is affordable but is a fan-out, and is not
   a consistent snapshot.
3. **Claiming merged top-N is approximate.** It is exact, because each player lives on exactly one shard.
   Confusing this with Chapter 71's approximate top-K merge is the same error in the other direction.
4. **Omitting `ZADD GT`.** Plain `ZADD` is last-write-wins, so an out-of-order or replayed update silently
   destroys a personal best. This is the most common real bug in shipped implementations.
5. **Treating the sorted set as the source of truth.** It is a derived index. Without a durable store
   behind it there is no rebuild path, and Redis persistence is not a ledger.
6. **Ignoring write amplification.** Every update lands on three boards, and updating on every score
   change rather than every meaningful event is a 350× cost multiplier for freshness nobody asked for.
7. **Leaving ties undefined.** "Redis sorts by member" advantages early-sorting IDs. Pack a tiebreaker
   into the score and prove the bit budget fits inside 2^53.
8. **One giant board instead of many boards.** A hundred million players fit on one instance; five modes
   times three windows do not. Shard for the multiplication, and know that is the reason.

---

## 10. Variants

**Friend leaderboards.** A player has at most a few hundred friends. Fetch their scores with one `ZMSCORE`
(or a batch read of the durable store) and sort in the application: microseconds, exactly correct, no
sorted set required. **Building per-user sorted sets for friend boards is the classic over-engineering
trap here** — it is `N` sets of 500 members, maintained on every score change, to replace a sort of 500
elements.

**Rating systems (ELO, TrueSkill, MMR).** Scores move down as well as up, so `ZADD GT` is wrong and plain
`ZADD` is right — the newest rating is authoritative by definition. The distribution is roughly Gaussian
rather than power-law, which makes §7.3's histogram unusually accurate and makes percentile the natural
display unit.

**Cumulative boards.** "Total points this season" uses `ZINCRBY`, which is relative and therefore not
idempotent, so the `match_id` idempotency key of §4 becomes load-bearing rather than defensive. This is
the one variant where the deduplication store is mandatory.

**Seasons and decay.** A season is an all-time board with an end date: archive it, start a fresh key. A
decaying board (weighting recent play) cannot be expressed as one number in a sorted set, so it is
recomputed periodically from an event log and bulk-loaded — closer to Chapter 71's windowed counting than
to anything here.

**Very large N.** "Show me ranks 1,000,000 through 1,000,100" is `ZREVRANGE` with a large offset, which
is O(log N + M) in a skiplist and genuinely cheap — one of the few places deep pagination is not a
problem. Across shards, though, it is not composable at all, which is another way of restating §7.2.

---

## 11. Further reading

- Chapter 04 §5 for t-digest; Chapter 71 §7.4 for the contrasting case where a top-K merge is not exact;
  Chapter 73 §7.4 for the same quantile-merging problem in a metrics system
- Redis documentation: `ZADD` (the `GT`, `LT`, `NX`, `XX`, and `CH` flags), `ZRANGE`, `ZRANK`, `ZCOUNT`,
  and the sorted-set implementation notes
- William Pugh, "Skip Lists: A Probabilistic Alternative to Balanced Trees" (*CACM*, 1990) — the original
  structure; Redis's span-augmented variant is in `t_zset.c`
- Redis Cluster specification, on hash tags and the constraints they place on multi-key operations
- Dunning and Ertl, "Computing Extremely Accurate Quantiles Using t-Digests" — the sketch behind §7.3's
  equal-population buckets
