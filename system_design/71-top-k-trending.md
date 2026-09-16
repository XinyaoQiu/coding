# Chapter 71 — Top-K and Trending

> **Prerequisites:** Chapters 03 (stream processing, event time, windowing), 04 §5 (Count-Min Sketch,
> HyperLogLog), 70 (event-time aggregation, hot keys)
> **Patterns:** probabilistic counting, bounded-memory state, retrieval then ranking, approximate merge

---

## 1. The problem

Given an unbounded stream of items — search queries, hashtags, product views, song plays, source IP
addresses — continuously report the K most frequent, for a few values of K around 100, over a few time
windows, sliced by a few categories. Then report the ones that are *trending*, which is a different
question that everyone conflates with the first one.

**The property that makes it hard:** the answer is tiny and the state required to compute it exactly is
enormous. You are asked for one hundred rows, and the naive way to be sure which hundred is to keep a
counter for every one of a billion distinct items — most of them typos, one-off queries, and bot noise
never seen again. The output is bounded by the product requirement; the input's cardinality is bounded by
nothing.

Everything here is a way of buying a correct-enough answer for the top of the distribution without paying
for the tail — and the second half is about the fact that **"most frequent" is almost never what the
product actually wants.**

---

## 2. Requirements

### Functional

1. Return the top K items by frequency for a given window and category, K ≤ 1,000.
2. Support several windows: 5 minutes, 1 hour, 24 hours.
3. Support a small set of dimensions: global plus ten categories or regions.
4. Return *trending* items, defined as items whose current rate is anomalous relative to their own
   historical baseline — not simply the most frequent.

### Non-functional

- **Ingest** — 100,000 events/second sustained, 5× that during a spike, which is exactly when anyone
  looks at a trending list.
- **Distinct cardinality** — on the order of 10^9 distinct items per day. This, not the event rate, is
  the number that shapes the design.
- **Freshness** — the top-K list reflects events up to 10 seconds ago.
- **Query latency** — p99 under 50 ms. The list is read by every home page load, so it is read far more
  than it is written.
- **Accuracy** — approximate is acceptable for the *ordering*, and a true heavy hitter must never be
  omitted. Displayed counts, if displayed at all, must be labeled as estimates.
- **Ingest is geographically distributed** across five regional data centers, and raw events are not
  shuffled between them. This forces §7.4; it is a real constraint, not a simplification.

### Explicitly out of scope

Personalized trending (a per-user ranking is a recommendation problem — Chapter A4). Deep manipulation
and spam resistance. Exact counts for billing, which is Chapter 70's problem and has different tolerances
by construction.

---

## 3. Estimation

**Event and cardinality volume**

```
100,000 events/s × 86,400   = 8.64 × 10^9 events/day
distinct items/day          ≈ 10^9   (the long tail: typos, one-off queries, unique URLs)
K = 100 (up to 1,000), windows {5 min, 1 h, 24 h}, dimensions {global + 10 categories}
```

**Exact counting — the number that rules it out**

```
per key: ~40 B item string + 8 B count + ~50 B hash-table overhead  ≈ 100 B
10^9 keys × 100 B                    ≈ 100 GB  per window per dimension
× 3 windows × 11 dimensions          ≈ 3.3 TB  of hot, mutable, in-memory state
```

Three point three terabytes of RAM, mutated a hundred thousand times a second, so a service can return a
hundred rows. **This is the constraining number**, and note where it comes from: not the event rate,
which is modest, but the multiplication of distinct cardinality by windows by dimensions.

**Count-Min Sketch sizing.** The sketch is a `d × w` array of counters, one hash function per row. The
standard bound is that the estimate exceeds the true count by at most `ε·N` with probability `1 − δ`,
where `w = ⌈e/ε⌉` and `d = ⌈ln(1/δ)⌉`.

```
choose eps = 1e-5, delta = 1e-3
w = ceil(e / 1e-5) = 271,829  -> round up to 2^18 = 262,144
d = ceil(ln(1000)) = 7
counters = 262,144 x 7 = 1,835,008 x 4 B  = 7.3 MB   <- independent of cardinality
```

The error is *absolute* and scaled to the stream size, which is what you have to check:

```
over 24 h:  eps.N = 1e-5 x 8.64e9 = 86,400 worst-case over-count | 100th item ~1e7   -> <1%   ok
over  1 h:  eps.N = 1e-5 x 3.6e8  =  3,600                       | 100th item ~4e5   -> ~1%   ok
over 5 min: eps.N = 1e-5 x 3.0e7  =    300                       | 100th item ~3,000 -> ~10%  marginal
100 GB (exact) / 7.3 MB (sketch)  = 14,000x reduction
```

**The accuracy of a sketch is not a property of the sketch; it is the ratio between the K-th item's count
and the total stream volume**, and that ratio degrades as the window shortens. Short windows want a wider
sketch, which is affordable precisely because they cover fewer events.

**Rolling-window state (§7.3), per dimension**

```
60 x 1-minute sketches = 440 MB   (finest granularity kept)
24 x 1-hour sketches   = 176 MB   (each merged from 60 minute sketches)
 1 x daily sketch      =   7 MB
                       ~ 620 MB per dimension -> x 11 dimensions ~ 6.8 GB per shard
```

Seven gigabytes fits comfortably in one process. That, not the event rate, is why the design works. And
the published answer is 100 entries × ~60 B ≈ 6 KB per (window, dimension) — 33 blobs, ~200 KB total,
rewritten every few seconds and read as a cache `GET`. **The read path here is trivial; all the
difficulty is in maintaining the answer** — the inverse of Chapter 10, worth noticing out loud.

---

## 4. API

```
GET /v1/top?window=1h&k=100&category=music&region=us
  -> 200 {
       items: [{rank, item_id, estimated_count, error_bound}],
       window_start, window_end, as_of,
       approximate: true, k_max: 1000
     }

GET /v1/trending?window=1h&k=50&category=music
  -> 200 {items:[{rank, item_id, score, current_rate, baseline_rate, support}], as_of}

POST /v1/events            # internal, batched from the collectors
```

**`approximate: true` and `error_bound` are not decoration.** A sketch-derived count is an upper bound
with known slack, and a client rendering "1,234,567 posts" from it publishes a number wrong by up to
`ε·N` in a direction it cannot detect. Return the estimate with its bound, or return only the ranking and
no counts — the second is usually the better product decision and should be offered.

**`k_max` is a hard server-side cap, and §7.4 is why.** The design assumes K is small relative to where
the count distribution flattens. A caller asking for the top million is asking a question this
architecture answers badly, and the API should refuse rather than return a plausible-looking list that is
mostly noise.

**Trending is a separate endpoint, not a sort order on the first.** The two return different items
computed by different machinery, and folding them into one `sort=` parameter hides that the second needs
a baseline the first does not have.

---

## 5. Data model

State lives in the stream-processing workers; the serving store holds only published answers.

```
per (dimension, sub-window):
  sketch      uint32[7][262144]     # 7.3 MB, Count-Min
  total_n     uint64                # needed to compute the error bound
per (dimension, window):
  heap        min-heap[K] of (est_count, item_id)   # K = 1,000 internally
  membership  hash map item_id -> heap index        # O(1) "is it in the heap"
published (Redis):
  topk:{dimension}:{window}  -> JSON blob, ~6 KB, TTL 60 s, rewritten every 5 s
baselines (§7.6), only for items that have entered a candidate set:
  baseline:{item_id}  -> {ewma_rate, ewma_variance, updated_at, first_seen}
```

Two decisions worth stating. **The heap is maintained at K = 1,000 internally though the API default is
100**, because the extra 900 entries are nearly free and they are the margin that makes §7.4's shard
merge sound. **The baseline table is keyed only by items that have entered a candidate set**, bounding it
by the number of items ever briefly popular — millions, not billions — which keeps trending from
reintroducing the per-item state the sketch removed.

---

## 6. Architecture, derived

### Attempt 1: sort the log

Run a batch job over the day's events: group by item, count, sort, take K. Exactly correct, easy to
reason about, and — for a daily "top searches of the day" report — **the right answer.** Say so; a
candidate who reaches for a sketch when the requirement is a nightly report is solving the wrong problem.

It dies on freshness: a full pass over 8.64 billion rows takes minutes and the requirement is 10 seconds.
It dies again on cost, since each run rescans the whole window.

### Attempt 2: exact counting in a stream processor

Key the stream by item, keep a counter per key, emit to a store, and query `ORDER BY count DESC LIMIT K`.

Two failures, at quantified points. **State**: §3's 100 GB per window per dimension, 3.3 TB total, all of
it hot. **Query**: a global sort over 10^9 rows, which no store does in 50 ms, so the answer must be
precomputed anyway — meaning the store's ordering capability was never what you needed. Note what it does
get right: exact, exactly mergeable across shards, any K. §7.5 is about when to come back to it.

### Attempt 3: Count-Min Sketch plus a min-heap

Replace the per-item counters with a fixed-size sketch, and keep a size-K min-heap of the current
leaders. Per event: increment the sketch (7 counter updates), read back the estimate, and — if the item
is already in the heap, update it in place; else if the estimate beats the heap's root, evict the root
and insert. Membership is O(1) through a hash map, eviction is O(log K).

State drops from 100 GB to 7.3 MB per sketch. §7.1 and §7.2 are the details.

**What breaks: windows.** A Count-Min Sketch only accumulates, and no decrement is safe — subtracting on
expiry corrupts the counts of every item that collided with the expiring one. So a single sketch answers
"most frequent since the process started," which nobody asked.

### Attempt 4: rolling sub-window sketches

Keep a sketch per fine-grained sub-window and answer a query by merging the sub-windows overlapping the
requested range. Sketches of identical dimensions merge by element-wise addition, and — the result that
makes this work — **merging does not degrade accuracy** (§7.3). Expiry becomes dropping the oldest
sub-window: a pointer move, not a decrement.

**What breaks: shards.** Ingest is regional (§2), so five regional lists must be combined into one global
list. That combination is approximate, and §7.4 says when it is fine and when it lies.

### Final architecture

```
 region us/eu/ap ──►  events, partitioned by hash(item_id) *within* each region
                          │
    ┌─────────────────────▼───────────────────────────┐  × 5 regions
    │ counting worker, per event:                     │
    │   sketch[dim][current_minute].add(item)   ──────►│ 60 × 1-min sketches (440 MB)
    │   est = sketch_window.estimate(item)             │   merged hourly ─► 24 × 1-h
    │   heap[dim][window].offer(item, est)      ──────►│ min-heap, K = 1,000
    └─────────────────────┬───────────────────────────┘
                          │ every 5 s: local top-1,000 + τ (the K-th count)
                          ▼
    ┌─────────────────────────────────────────────────┐
    │ merger: sum per item across regions, rank,      │  §7.4 — approximate,
    │         truncate to K, attach error bounds      │        bounded by Σ τ_s
    └───────┬──────────────────────────────┬──────────┘
            ▼                              ▼
  Redis topk:{dim}:{window}        baseline store (EWMA per candidate, §7.6)
            │                              │
            │                    trending scorer (candidates only)
            ▼                              ▼
      query service ◄──────── Redis trending:{dim}:{window}
```

---

## 7. Deep dives

### 7.1 The Count-Min Sketch, and why the error direction is the whole point

The structure is a `d × w` array of counters and `d` pairwise-independent hash functions, one per row.

```
add(x):     for i in 0..d-1:  C[i][ h_i(x) mod w ] += 1
estimate(x):  return min over i of  C[i][ h_i(x) mod w ]
```

Every row's counter for `x` holds `x`'s true count **plus** the counts of every other item that collided
with it in that row. Collisions only add, so every row over-estimates, and the minimum across rows is the
row where `x` was luckiest. Hence:

**The sketch over-estimates and never under-estimates.** This asymmetry is not a footnote; it is the
property that makes the structure usable. Two consequences follow directly:

- **"This item is below the threshold" is trustworthy.** If the estimate says 40 and the threshold is
  1,000, the true count is at most 40 and the item is genuinely not a heavy hitter. You can discard it
  with confidence.
- **"This item is above the threshold" may be a collision.** A cold item that happens to share cells with
  several hot ones can be inflated. So a sketch is safe as a **filter that admits extra candidates and
  never drops real ones**, and unsafe as a final answer.

That is exactly the shape of a retrieval stage: reduce 10^9 items to a few thousand candidates, then
verify or re-score them by more expensive means (§7.6 with baselines, §7.4 with a second round trip). A
design that displays sketch estimates as final counts has used a filter as an oracle.

**Sizing, as a rule:** `w` controls the collision rate and therefore the error magnitude; `d` controls
the probability that every row was unlucky at once. Widening `w` buys accuracy; deepening `d` past 5–10
buys little and costs a random memory access per row per event. Seven rows is a reasonable default, and
those seven scattered reads are the real per-event cost — the structure is memory-latency-bound.

**The conservative-update variant**, which increments only the rows currently equal to the minimum,
reduces over-estimation substantially at no memory cost, and is tempting. **It is not linearly
mergeable.** Two conservatively-updated sketches added element-wise do not equal a conservatively-updated
sketch of the combined stream. Since §7.3 and §7.4 both depend on merging, conservative update is off the
table here — and being able to say *why* you rejected an optimization is worth more than the optimization.

### 7.2 The heap, and why the pairing works

The sketch answers "how many times have I seen `x`?" It does not answer "which items are the top 100,"
because you cannot enumerate a sketch — the items are not stored in it.

So carry a **fixed-size min-heap of K entries**, ordered by estimated count, **with the smallest at the
root**, plus a hash map from `item_id` to heap position:

```
offer(x, est):
  if x in membership:              # O(1)
      heap[membership[x]].count = est ;  sift_down(...)        # O(log K)
  elif len(heap) < K:
      push(x, est)                                              # O(log K)
  elif est > heap.root.count:      # O(1) — the check that runs on almost every event
      membership.remove(heap.root.item) ; replace_root(x, est)  # O(log K)
  # else: do nothing. This is the common case.
```

The root comparison is why this is cheap: for the overwhelming majority of events the item is cold, the
estimate loses to the root, and the work is one hash lookup and one integer compare. Only the rare event
that changes the leaderboard pays the O(log K), and log₂(1,000) is ten sift steps.

**The subtlety that makes the pairing correct** — the answer to "why not just a heap?" — is that an item
evicted from the heap loses its slot but **keeps its count in the sketch**, so when it becomes popular
again its estimate resumes from its full history rather than restarting at one. A bare heap has no memory
of evicted items and must re-earn its count from scratch each time, systematically suppressing bursty
items: exactly the ones a trending system exists to find. The sketch is long-term memory; the heap is an
index over it.

**Redis ships this as a native type**: `TOPK.RESERVE key k width depth decay`, then `TOPK.ADD`,
`TOPK.LIST`, `TOPK.QUERY`. It is implemented with **HeavyKeeper** rather than a plain CMS-plus-heap — a
probabilistic-count hash table paired with a min-heap, where a collision decrements the incumbent counter
with a probability that decays exponentially in that counter's current value, so heavy items are almost
never displaced while cold items decay out of their cells fast. The effect is much better precision at
the same memory; the price is that HeavyKeeper is **not a pure over-estimator** — it can under-count — so
§7.1's clean "below-threshold is trustworthy" guarantee is traded away. Name that trade too.

### 7.3 Sliding windows as rolling sub-window sketches

A sketch cannot forget, and decrementing on expiry is unsound because the counter for an expiring item
also holds the counts of everything that collided with it. So implement the window by **partitioning time
into sub-windows, one sketch each, and merging on read**:

```
1-hour window, 1-minute sub-windows:
  keep sketches S[t-59 .. t]
  estimate(x over the hour) = min over rows of ( Σ_{i=t-59}^{t} S_i[row][h(x)] )
  advance: allocate S[t+1], drop S[t-59]
```

Sketches of identical dimensions and hash functions are **linearly mergeable**: element-wise addition
produces exactly the sketch you would have built from the concatenated stream. Two things follow, and the
second is the non-obvious one — expiry becomes a pointer move rather than a decrement, with no
corruption; and **merging costs no accuracy**, since the merged error is `Σ_i ε·N_i = ε·N`, identical to
one sketch of width `w` over the whole window. Sub-windowing buys granularity for memory alone.

The corollary is the constraint: **each sub-window sketch must be full width.** You cannot shrink the
per-minute sketch by 60× on the grounds that it covers 1/60 of the events, because the merged error is
the sum of the sub-window errors and shrinking `w` inflates each of them proportionally. Memory therefore
scales linearly with the number of sub-windows, which sets the design:

```
1-min sub-windows for the 1-hour view:   60 × 7.3 MB = 440 MB
merge each completed hour into one       24 ×  7.3 MB = 176 MB   for the 24-hour view
                                                       ≈ 620 MB per dimension
```

The hierarchy is what makes 24 hours affordable: minute resolution only for the last hour, older time
compacted into hour-granularity sketches. The cost is that the 24-hour window advances in one-hour steps
at its trailing edge, so it is really "the last 24 to 25 hours" — say that rather than pretending the
edge is exact; every rolling-window implementation has this seam.

**The alternative worth naming and rejecting:** one sketch with exponential decay, all counters
periodically multiplied by a factor slightly below 1. Cheap, one sketch instead of sixty, and genuinely
used. But it does not answer "the last hour" — it answers "a recency-weighted count over all history,"
which cannot be explained to a product manager, cannot be reconciled against a batch job, and has no
window boundary to test against. Take it when the consumer is a ranking function needing only a monotone
recency signal; reject it when the consumer is a UI that says "trending in the last hour."

### 7.4 Merging per-shard top-K, and where it stops being honest

Five regions each report their local top-K; the merger sums per item and truncates. **This is
approximate, and the interesting content is the boundary.**

The failure is specific: an item ranking just below K on *every* shard is reported by *none*, so the
merger never learns of it, even though the sum of its five sub-threshold counts might place it in the
global top-K. Nothing in the reports reveals this; the item is simply absent.

The bound is what makes the approximation manageable. Let `τ_s` be the K-th (smallest reported) count on
shard `s`. If `x` is absent from shard `s`'s report, its count there is at most `τ_s`. So:

```
true_global(x)  ≤  Σ_{s reports x} c_s(x)  +  Σ_{s omits x} τ_s
under-report of any omitted item  ≤  Σ_s τ_s      (the "slack")
```

Have each shard report `τ_s` alongside its list — one integer — and the merger can state a bound on its
own error rather than guessing. **Now plug in numbers, which is where the answer lives.**

*Global top-100 from regional top-100s.* Each region processes ~1.7 × 10^9 events/day, so under a power
law the 100th item in a region sits near 10^6 and the slack is `5 × 10^6`, against a global 100th item
near 10^7. That is ~10% of the boundary item's count: it perturbs positions near rank 100 and leaves the
top 20 untouchable. For a trending list this is **fine**, and reporting top-1,000 per region (§5) drops
`τ_s` by an order of magnitude. Cheap insurance.

*Global top-1,000,000 from regional top-1,000,000s.* The millionth item globally has a count in the low
hundreds, and `τ_s` for a million-deep report is the same order, so the slack is comparable to the counts
being ranked: **the ordering of the tail is noise.** Worse, the mechanism has stopped saving anything —
five million entries shipped per merge cycle is not a "top-K merge," it is a full shuffle of the
distribution performed badly.

**The rule: merging local top-K is sound when K is well inside the steep part of the count distribution,
and unsound once the K-th and (K+1)-th items have nearly equal counts.** The distribution's shape, not
K's absolute value, decides. This is also why §4 caps `k_max` — the API should refuse the question it
answers badly. Three ways out:

1. **Report `c·K` and truncate to `K`**, with `c` between 3 and 10. Shrinks `τ_s`, costs `c×` network on
   a payload that was already tiny. Always do this.
2. **Two-pass verification.** Merge the local top-`cK` lists into a candidate set, then ask every shard
   for its count of each candidate — a point lookup per candidate per shard. The result is exact over the
   candidate set; the only remaining error is an item in no shard's top-`cK` at all. Costs one round trip
   and `|candidates| × shards` lookups. This is the answer when someone challenges the approximation.
3. **Partition by item instead of by region, so there is no merge.** If every occurrence of an item lands
   on one shard, that shard's count is complete and the global top-K is exactly the merge of per-shard
   top-Ks. Right *inside* a data center — which is why §5 partitions by `hash(item_id)` within a region —
   and unavailable *across* regions, since it would mean shuffling raw events over the WAN, which costs
   more than the rest of the system combined.

Note the contrast with Chapter 72 §7.2, where merging per-shard top-N *is* exact, because each player
exists on exactly one shard and nothing is split. **Merging top-K lists is exact when the entity is
partitioned and approximate when the entity's counts are split** — one sentence covering both chapters.

### 7.5 When exact counting is simply better

The sketch is justified by §3's 100 GB. Change the cardinality and the justification evaporates.

```
10^4 distinct items (hashtags in one country)   × 100 B  =   1 MB     exact, trivially
10^6 distinct items (products in a catalog)     × 100 B  = 100 MB     exact, comfortably
10^7 distinct items                             × 100 B  =   1 GB     exact, still fine
10^8 distinct items                             × 100 B  =  10 GB     borderline per shard
10^9 distinct items                             × 100 B  = 100 GB     sketch territory
```

**The crossover is between 10^7 and 10^8 distinct keys per shard per window**, and below it a hash map
beats a sketch on every axis that matters: exact, so no error bounds and no "approximate" flag; exactly
mergeable across shards by summing per key; supports any K, arbitrary threshold queries, and deletion;
and when the numbers look wrong, you can print it. A sketch adds a tunable-error structure, a second data
structure to keep in sync, and a class of bug that only appears at the tail — to save memory you had.

Two caveats. The relevant cardinality is **per shard per window**, so sharding can pull you below the
crossover: 10^9 items across 100 shards is 10^7 each, and if the partitioning is by item then exact
counting is back on the table *and §7.4's approximation disappears with it* — a genuinely good design
move, and the one most people miss. And the tail is *adversarial*: bots and random garbage inflate
distinct cardinality far above product intuition, so if you choose exact counting, cap the map with an
explicit eviction policy, making the failure a bounded loss of tail accuracy rather than an OOM kill.

### 7.6 "Most frequent" is not "trending"

Run the machinery above on a social network and the top-10 hashtags are the same ten every day. Raw
frequency returns the perennial giants: correct, useless, and the most common way this problem is
answered wrongly. **Trending is a rate anomaly relative to a baseline**, where the reference is the
item's own history, not the other items. Three scoring functions:

- **Ratio**, `current_rate / baseline_rate`. Intuitive and catastrophic alone: an item going from 1 to 20
  scores 20×, so the list fills with tail noise. It needs a **minimum support floor** — which the sketch
  already provides, since it can certify "this item is below 1,000" with confidence (§7.1).
- **Z-score**, `(x − μ) / σ` over the item's per-window history. Better behaved, because it accounts for
  how variable the item normally is: a hashtag that always oscillates needs a bigger move to register.
  Requires per-item `μ` and `σ`.
- **Log-likelihood ratio / chi-square** between the current window's distribution and the baseline's. The
  most defensible statistically, and the hardest to explain to someone asking why their topic is not
  trending.

**All three require per-item state, which is what the sketch was built to avoid.** The resolution is the
two-stage structure that runs through this book: **retrieval, then ranking** (Chapters 22 and 61). The
sketch-plus-heap produces a few thousand candidates clearing a support floor; baselines and scores are
computed only over those, so per-item state is bounded by the items that have ever been briefly popular —
millions, in a small store — not by the billion distinct items in the stream. Maintain the baseline as an
EWMA with a half-life of about a day, `baseline ← α · observed_rate + (1 − α) · baseline`, updated once
per window.

**The hard case is not an edge case: an item with no baseline.** A brand-new hashtag has no history, and
a brand-new hashtag with a hundred thousand mentions is the most trending thing on the platform. A ratio
against a zero baseline is infinite, so you must decide what a missing baseline means. The workable
answer is a floor: treat an unseen item's baseline as a small global prior derived from the observed rate
distribution of items in their first hour, then score and cap. What you must not do is silently skip
items without baselines, which drops precisely the results the feature exists for.

Two operational notes. **The baseline must be windowed to the same dimension as the score** — an item
trending in one region scored against a global baseline looks anomalous purely because of time zones. And
**trending is gamed**, because being on the list is valuable, and a coordinated campaign produces a
genuine rate anomaly. The structural defense is to rank by **distinct participants** rather than events,
so one account posting a hashtag 10,000 times contributes 1. That is a different counting problem: keep a
HyperLogLog per *candidate* item per window (Chapter 04 §5), ~12 KB each at ~2% error, mergeable exactly
as the sketches are. Over the whole item space it would cost `10^9 × 12 KB` and is absurd; over the few
thousand candidates it costs tens of megabytes and is free. **A cheap approximate structure over
everything, an expensive accurate one over the survivors** — the same layering, one level down.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Sketch saturates (ε·N grows past the K-th count) | Tail of the list becomes noise | Alert on `ε·N / count_of_Kth`; widen `w` or shorten the window |
| Counter overflow in a 32-bit cell | Wraparound, silently wrong | Size cells for `max_count × collision factor`; use 64-bit for daily windows |
| Worker restart | Loses in-memory sketches and heap | Rebuild from the retained event log (Chapter 03 §2); the window is short, so recovery is bounded by the window, not by all history |
| One region unreachable | Global list silently missing that region's contribution | Report per-region freshness; fail the merge loudly rather than publishing a partial global list as if it were whole |
| Clock skew across regions | Sub-windows misaligned; merged window edges wrong | Window on event time with a watermark (Chapter 03 §7); align sub-window boundaries to absolute epoch minutes |
| Baseline store cold after deploy | Everything looks like it is trending | Persist baselines; refuse to publish trending until baseline coverage clears a threshold |
| Coordinated manipulation | A fake item ranks | Distinct-participant counting (§7.6), account weighting, a review queue for the top entries |

**Monitoring:** `ε·N` against the K-th item's estimated count — the direct measure of whether the sketch
still fits the window it serves; heap churn rate, where a spike means either a real event or a sketch
that has begun over-estimating; merge slack `Σ τ_s` as a fraction of the K-th global count, which is
§7.4's honesty metric; per-region report freshness; candidate-set size, whose collapse means the support
floor is mis-tuned; and the overlap between consecutive published lists, since a list that turns over
completely every cycle is not measuring anything real.

---

## 9. Common mistakes

1. **Reaching for a sketch when exact counting fits.** Below roughly 10^7 distinct keys per shard, a hash
   map is exact, mergeable, debuggable, and smaller than the argument for the alternative. Interviewers
   notice when a probabilistic structure is deployed as a reflex.
2. **Not knowing which way the error goes.** "Approximate" is not an answer. Count-Min over-estimates and
   never under-estimates, which is what licenses using it as a filter and forbids using it as an oracle.
3. **Trying to decrement the sketch to implement a window.** Unsound, because the counter being
   decremented is shared with colliding items. Roll sub-window sketches instead.
4. **Assuming merged shard top-Ks are exact.** They are exact only when the entity is partitioned so that
   its counts are not split. Across regions they are approximate, and the size of the approximation
   depends on K relative to the shape of the distribution.
5. **Displaying sketch estimates as counts.** The number is an upper bound with slack `ε·N`. Show the
   ranking, or show the bound.
6. **Confusing most-frequent with trending.** They return different items. A trending feature that ranks
   by raw count ships the same list every day and will be rebuilt within a quarter.
7. **Reintroducing per-item state for the baseline.** Baselines belong to the candidate set, not the
   stream; computing them for every distinct item undoes the memory saving that motivated the design.
   The mirror-image mistake is **ignoring items with no baseline**, which are exactly the ones the
   feature exists to surface.
8. **Sizing the sketch once and never rechecking.** The error bound scales with stream volume; a sketch
   accurate at launch degrades silently as traffic grows.

---

## 10. Variants

**Autocomplete (Chapter 62).** Top-K completions per prefix: the same counting problem with a different
retrieval structure — a trie whose nodes cache their own top-K — and a much longer acceptable staleness,
which usually makes an offline rebuild correct instead of a streaming one.

**Heavy-hitter detection for rate limiting and abuse (Chapter 90).** Top-K source IPs by request rate.
Same machinery, but the consumer is automated enforcement, so the over-estimation direction turns
dangerous: a collision can get an innocent client throttled. Verify a candidate exactly before acting on
it — §7.4's two-pass idea applied to a different purpose.

**Top-K by weight rather than count.** "Top products by revenue" instead of "by views." Count-Min handles
weighted increments natively — add `w` instead of `1` — but the error bound becomes `ε · Σw`, so one
enormous transaction inflates the error for everything. Cap or winsorize the weights.

**Hot-key detection inside another system.** Chapter 70 §7.4 uses this structure to decide which ad IDs
to salt; Chapter 91 uses it to find keys worth replicating in a cache. In both the list is
machine-consumed and never displayed, relaxing every accuracy requirement except "never miss a true
heavy hitter."

**Exact daily top-K as a batch job.** Attempt 1, unmodified — and if you run the streaming path too, the
batch result is a free accuracy audit of it, the same relationship Chapter 70 §7.5 sets up between
streaming and reconciliation.

---

## 11. Further reading

- Chapter 04 §5 for the structures; Chapter 70 for exact event-time aggregation; Chapter 62 for the
  prefix-scoped variant
- Cormode and Muthukrishnan, "An Improved Data Stream Summary: The Count-Min Sketch and its
  Applications" (*Journal of Algorithms*, 2005) — the original, and short
- Gong et al., "HeavyKeeper: An Accurate Algorithm for Finding Top-k Elephant Flows" (USENIX ATC 2018) —
  the algorithm behind Redis's `TOPK` type
- Metwally, Agrawal, and El Abbadi, "Efficient Computation of Frequent and Top-k Elements in Data
  Streams" (ICDT 2005) — the Space-Saving algorithm, the main alternative to CMS-plus-heap
- Redis documentation: the `TOPK` and `CMS` data types in Redis Stack
- Charikar, Chen, and Farach-Colton, "Finding Frequent Items in Data Streams" (ICALP 2002) — Count-Sketch,
  which is unbiased rather than one-sided, and the contrast is instructive
