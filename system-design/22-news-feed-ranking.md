# Chapter 22 — Ranked News Feed

> **Prerequisites:** Chapter 20 (retrieval, materialized timelines), 02 (caching), 03 (streams, event time),
> 04 (Bloom filters, observability)
> **Patterns:** retrieval then ranking, per-source quotas, feature stores, multi-stage funnels, feedback loops

---

## 1. The problem

Facebook's News Feed, LinkedIn's feed, Twitter's "For you," an app's home screen: given everything a user
*could* see, choose fifty things and put them in an order. The retrieval half is Chapter 20 and is assumed
here. This chapter is about what happens after the candidates exist.

**The property that makes it hard:** you cannot precompute the answer. In Chapter 20 the ordering function
was `created_at` — a property of the item alone, immutable, shared by every viewer — which is exactly why a
materialized per-user timeline of appended IDs worked. A ranking score is a function of five arguments:
viewer, item, context, time, and model version. Three change continuously and one changes every time
somebody deploys. Precomputing would mean materializing viewer × candidate scores — 500 million users ×
500 candidates = 2.5 × 10¹¹ numbers — and invalidating all of them on every daily model push.

So ranking happens inside the request, on a hard latency budget, and **that budget forces every other
decision in this chapter**: why the candidate set is hundreds rather than millions, why there are two
models instead of one, why features are fetched in one batch, why re-ranking is a cheap post-pass. There
is a second, quieter difficulty that §8 returns to: a ranking system that breaks does not return errors.
It returns 200s containing worse content, and the only instrument that detects it has a confidence
interval measured in days.

---

## 2. Requirements

### Functional

1. Return a ranked, paginated feed for a viewer from a supplied candidate set.
2. Ingest engagement signals — impressions, clicks, likes, dwell, hides — and feed them back into ranking.
3. Serve multiple models and configurations concurrently, bucketed by experiment.

Defer, but name: ad insertion and auction, notification ranking (a threshold decision, not a sort — §10),
comment ranking, the retrieval layer itself (Chapter 20), and the model architecture.

### Non-functional

- **End-to-end feed p99 under 250 ms**, of which **ranking gets at most 150 ms**. The remaining 100 ms is
  auth, hydration (Chapter 21, §7.3), and serialization. §7.3 spends the 150 ms line by line.
- **Scoring throughput** — 35,000 feed requests/sec × 500 candidates = **17.5 million item-scorings/sec**.
  Every model-architecture decision is a per-item cost multiplied by that number.
- **Feature freshness** — engagement counters in served features within 5 minutes, viewer session context
  within seconds, batch features daily. Three tiers, deliberately.
- **Graceful degradation** — ranking is a *degradable* dependency: if the model server is down the feed
  still returns, ordered chronologically. Write this into the requirements, because it is what lets you put
  a stateful ML system on a synchronous read path at all.
- **Training/serving consistency** — the feature values a model is trained on must equal the values it is
  served. The one non-functional requirement candidates never state, and the one that breaks (§7.2).
- **Experiment isolation** — a bad model must stay in its bucket and be reversible in one config change.

### Explicitly out of scope

Model architecture and training algorithms, integrity and spam classification (assumed as an upstream
filter), and the ads auction, which shares the infrastructure but optimizes revenue under different
constraints.

---

## 3. Estimation

500 million daily active users, matching Chapters 20 and 21.

**Request rate and candidate volume**

```
500M × 2 sessions/day = 1B feed requests/day ≈ 11,600/sec average, ~35,000/sec peak

candidates per request:  followed-graph 250 | groups+pages 100 | unconnected 100
                       | trending+exploration 50   =  500 after dedup and quotas
```

**Why 500 and not 500,000** — the arithmetic that produces the whole architecture:

```
eligible corpus (posts from the last 3 days that pass visibility) ≈ 10^8
scoring the corpus:   35,000 × 10^8  = 3.5 × 10^12 scorings/sec
scoring 500:          35,000 × 500   = 1.75 × 10^7 scorings/sec
```

**Retrieval must cut the corpus by five orders of magnitude before the model sees anything.** That factor
of 10⁵ is the reason two-stage architectures exist; there is no clever model that closes it.

**Model cost and feature traffic, which size the two fleets**

```
17.5M scorings/sec × 100 µs/item (small model, CPU)  =  1,750 cores
17.5M scorings/sec ×   1 ms/item (deep model)        = 17,500 cores

per request: 500 items × 150 item features × 4 B = 300 KB
           + 500 × 30 interaction features × 4 B =  60 KB
           +   1 × 500 viewer features × 4 B     =   2 KB   ≈ 362 KB
35,000/sec × 362 KB                                         ≈ 12.7 GB/sec
```

An order of magnitude in per-item cost is an order of magnitude in fleet size, which is why §6 splits
ranking into a cheap pass and an expensive one rather than making one model faster. And 12.7 GB/sec of
feature reads is the second constraining number; §7.2 and §7.3 attack it with quantization, batching, and
the observation that item features are viewer-independent and therefore cacheable in-process.

**Training data:** 1B requests/day × 50 logged impressions × ~1 KB = **50 TB/day**, sampled down for
training and retained briefly in full for debugging. The log exceeds the content corpus by an order of
magnitude — in a ranking system the exhaust is bigger than the product.

---

## 4. API

```
GET  /feed?cursor=<opaque>&limit=10
  headers: Authorization
  ->  200 {items: [{postId, source, position}], nextCursor, rankingSessionId, modelVersion}

POST /feed/events
  body: {rankingSessionId, impressions: [{postId, position, visibleMs, dwellMs}],
         actions: [{postId, type, ts}]}
  ->  202
```

**The cursor cannot be a timestamp or an ID.** There is no stable global ordering to resume from: a
score-ordered list is recomputed on every request against changing features, so "everything after post X"
is undefined. Two options. *Re-rank per page* — score fresh candidates each time, excluding what was shown
— is always current, but you pay full ranking cost per page and the seen-set filter must be perfect or the
user gets duplicates mid-scroll, the most visible ranking bug there is. *Rank once per session* — score
~500 candidates on the first request, persist the ordered list under a `rankingSessionId` with a short
TTL, page through it — costs one ranking per session instead of five, is duplicate-free by construction,
and leaves a replayable debugging artifact, at the price of intra-session staleness and a little state.

**Choose the ranking session**, refreshed when exhausted or after a few minutes. A typical session reads
two or three pages, so this cuts ranking cost roughly 3× for one short-lived cache entry.

**The impressions endpoint is not telemetry; it is half the training set.** A model learns from contrast,
and the negatives — shown, not engaged with — exist nowhere else. Without a client-reported impression log
you train on positives alone, and a model trained on positives learns popularity rather than preference.
`position` and `visibleMs` are required fields: they are the inputs to position debiasing (§7.5) and the
difference between "did not like it" and "never saw it." **`modelVersion` and `rankingSessionId` come back
to the client** so every logged event is attributable to the exact model and candidate set that produced
it; ranking without attribution is unanalyzable.

---

## 5. Data model

```
online feature store       (KV, sub-10 ms batch reads)
  item:{post_id}    -> {age_sec, author_id, media_type, embedding[64],
                        ctr_1h, ctr_24h, like_rate, hide_rate, ...}     ~150 features
  user:{user_id}    -> {embedding[64], topic_affinity[32], session_ctx} ~500 features
  edge:{user,author}-> {affinity, last_interaction_at, historical_ctr}   ~30 features

training_log               (append-only, partitioned by day)
  request_id, viewer_id, post_id, position, served_at,
  model_version, experiment_bucket,
  feature_vector AS SERVED,          <-- the critical column
  label_click, label_like, label_dwell_ms, label_hide, label_share

ranking_session            (Redis, TTL ~20 min)
  session_id -> [post_id...] ordered, cursor offset, model_version, bucket

seen_set                   (per user, capped — Bloom filter, ~12 KB each)

model_registry
  model_id, version, artifact_uri, feature_schema_hash, status, rollout_pct
```

**The training log snapshots feature values as served, not as recomputed.** The most important schema
decision in the chapter. If training re-derives features from a data lake, any divergence between the
offline and online computation silently corrupts every model you train (§7.2); logging the served vector
eliminates that entire class of bug for every feature you log, at the cost of a wide, ugly column.

**Labels arrive later than features.** The impression is logged at serve time; the like comes four minutes
later, the share an hour later. A training row is therefore a *stream join* of impressions against actions
with a watermark (Chapter 03, §7) — typically a 24-hour attribution window, after which unmatched
impressions become negatives. That window is a real decision: too short and slow positives are mislabeled
negative, too long and the training set lags the model by a day.

**Three feature entity types, not one flat vector.** Item features are viewer-independent and therefore
shared and cacheable; user features are fetched once per request rather than once per item; edge features
are the only genuinely per-(viewer, item) data and the smallest set by design. Collapsing them into one
keyspace multiplies the fetch by 500. And **`feature_schema_hash`** lets serving refuse to load a model
whose expected features do not match what the store provides — cheap, and it catches the most common
deploy-time failure.

---

## 6. Architecture, derived

### Attempt 1: score everything at read time

Score every eligible post per request and take the top fifty. From §3 that is 3.5 × 10¹² scorings/sec,
seven orders of magnitude beyond any fleet. Dead on arrival, but worth stating: it establishes that
retrieval is not an optimization, it is a precondition.

### Attempt 2: precompute the ranked feed offline

A nightly batch job scores each user's candidates and writes an ordered list. Storage is trivial
(500M × 500 × 8 B = 2 TB) and serving is one key read. It fails four ways, and the first is fatal.

**Freshness.** With 100M posts created per day and the feed computed at 03:00, by 22:00 roughly 79% of the
day's content has never been considered. Engagement with a post concentrates in its first hours; a feed
that cannot include today's posts is not a feed.

**Context.** The batch job does not know it is 7 p.m., that the user is on cellular, that they opened the
app three times in ten minutes, or that they just hid two posts from the same author. **Model deploys.**
Every model change invalidates 2.5 × 10¹¹ scores, so shipping a model means a full recompute and a day
between "we have a better model" and "users see it." **Exhaustion.** A user who scrolls past the
precomputed list has nothing behind it.

### Attempt 3: split retrieval from ranking

Retrieval is cheap, precomputable, and viewer-independent enough to materialize (Chapter 20); ranking is
expensive, viewer-dependent, and per-request. Put the boundary at 500 candidates and the arithmetic works:
17.5M scorings/sec at 100 µs is 1,750 cores, which is a fleet. Everything after this is refinement.

### Attempt 4: the model grew, so split ranking too

A model good enough to be worth deploying is not a 100 µs model. A deep network with embeddings costs
~1 ms per item, and 17.5M/sec × 1 ms = 17,500 cores — an order of magnitude more machines to serve one
model. Making the model smaller sacrifices exactly the quality you built it for.

Instead, make the funnel narrower before the expensive stage:

```
500 candidates ─► light ranker (20 µs, ~20 features) ─► 100 ─► heavy ranker (1 ms) ─► 50

light: 35,000 × 500 × 20 µs =   350 cores
heavy: 35,000 × 100 ×  1 ms = 3,500 cores
total                        ≈ 3,850 cores   vs 17,500
```

A 4.5× saving, and it cuts the feature fetch 5× as well, since features are only fetched for the 100 the
light ranker keeps. **This is why every production ranker is a funnel rather than a model.** The cost is
that the light ranker can discard an item the heavy ranker would have loved, and that error is invisible —
you never see the counterfactual. Mitigate by keeping the light ranker's features a strict subset of the
heavy ranker's, and by measuring recall@100 of the light stage against a periodic full-scoring sample.

### Attempt 5: the feature fetch

12.7 GB/sec of feature reads (§3) becomes 2.5 GB/sec after the funnel cut, still a large number. Three
stacking reductions: fetch item features only for funnel survivors; quantize to int8 or fp16, halving or
quartering the bytes at negligible accuracy cost; and cache item vectors in-process on the ranking
servers, which works because **item features are viewer-independent and different viewers' candidate sets
overlap heavily on popular items**. A modest LRU absorbs most of it; the origin sees the long tail.

### Attempt 6: the top 50 by score is a bad feed

Score-optimal is not product-optimal: the top fifty by predicted engagement is routinely twelve posts from
one author, all the same media type, all from the last hour, and nothing from accounts the user explicitly
subscribed to but rarely clicks. Add a **re-ranking pass** over the 100 survivors applying diversity
penalties and hard business rules (§7.4) — 100 items, under 5 ms.

### Attempt 7: close the loop, and admit it is a loop

The model is trained on data the model produced: impressions are logged, joined to labels, and train the
next model, which decides the next impressions. Left alone this converges to a narrower and narrower
policy (§7.5). Reserve a fraction of slots for exploration and log position so the bias can be corrected.

### Final architecture

```
GET /feed
   ├─► Ranking session cache ─ hit ─► page from stored order ────────────────┐
   └─ miss ─► Candidate generation (parallel, 40 ms deadline, quotas)        │
                 ├─ followed-graph timeline (Ch 20)  ├─ ANN (unconnected)    │
                 └─ groups / pages                   └─ trending + explore   │
                          │                                                  │
                    dedup + filter (seen-set, blocked, integrity)  ─► 500    │
                    Light ranker                                   ─► 100    │
                    Feature store (batch read, int8, in-process LRU)         │
                    Heavy ranker (multi-task, dynamic batching)              │
                    Re-ranker (diversity, business rules, ad slots) ─► 50    │
                          └──► store as ranking session ────────────────────►┤
                                                                             ▼
                                                             hydration (Ch 21) ─► JSON

POST /feed/events ─► Kafka ─┬─► near-line aggregation ─► online feature store (< 5 min)
                            └─► impression×label join (watermark) ─► training log
                                              └─► daily training ─► registry ─► rankers
```

---

## 7. Deep dives

### 7.1 Candidate generation with per-source quotas

Retrieval is a scatter-gather over heterogeneous sources: the materialized follow timeline, group and page
content, an ANN index over embeddings for unconnected content, trending lists, and an exploration pool.
They differ in cost, in candidate volume, and in the precision of the scores they will eventually receive.

**Why not take the union and let the ranker sort it out?** Three reasons, and only the third is obvious.
*Volume asymmetry*: the ANN source can return ten thousand candidates as easily as one hundred, while the
follow graph returns whatever the user's followees actually posted — perhaps two hundred. Under a pure
score ordering the source that can generate more candidates wins more slots, so the winner is decided by
index configuration rather than by quality. *Calibration*: predicted engagement is estimated from logged
data, and logged data for unconnected content comes from a different exposure distribution than logged
data for followed content, so two scores of 0.4 from those populations are not the same 0.4 — comparing
them is a category error no amount of model quality fixes. *Product guarantees*: quotas are the only place
you can promise that a user who subscribed to something will see it; under pure engagement competition,
low-engagement subscriptions disappear, and users experience that as the product ignoring their choices.

So: fixed quotas per source (250 / 100 / 100 / 30 / 20 in §3), with a floor for subscribed content.
**The cost is that you have hard-coded a policy the model could in principle learn**, and each number is a
parameter someone must tune with an experiment; after two years there will be a dozen of them and no one
will know why each is what it is.

**Retrieval is a scatter-gather, so its p99 is the p99 of the slowest source.** With five sources at
p99 = 30 ms each, the union's p99 is materially worse than 30 ms. Impose a hard deadline — 40 ms — and
proceed with whatever returned. A feed missing one source is imperceptible; a feed 400 ms late is not.
This is the single highest-leverage latency decision in the retrieval stage and it must be a deadline on
the *gather*, not a timeout on each call.

### 7.2 The feature store, and training–serving skew

This is the central real-world failure mode of ranking systems, and it is worth more interview time than
the model.

**The mechanism.** Training reads features computed by an offline pipeline over a data lake; serving reads
features computed by a streaming pipeline into a KV store. Any difference between the two computations
shifts the served feature distribution away from the trained one and the model degrades. The sources of
difference are mundane and endless: a different default for a missing value (0 offline, −1 online), a
window boundary in UTC in one and local time in the other, a bug fixed in the batch job and not the
streaming job, a feature clipped at serving but not in training, a data source that starts returning nulls
for 3% of items.

**It is dangerous because it is silent.** No exception, no latency change, no error rate — 200s with worse
content, detected by an A/B metric with a multi-day confidence interval. By the time anyone notices,
several model versions have trained on skewed data. Three defenses, used together.

*(a) Log features as served and train only on those.* Eliminates skew by construction for every logged
feature, because training and serving read literally the same numbers. This is the foundation and it is
why §5 has a `feature_vector AS SERVED` column. **The cost is real and usually understated:** you can only
train on features that were served, so adding a feature means shipping it to serving in shadow mode,
waiting days or weeks for the log to accumulate, and only then training. Feature iteration becomes a
deploy-and-wait cycle rather than a batch-job change.

*(b) A single feature definition compiled to both batch and streaming implementations* — the feature store
proper, with point-in-time-correct offline joins and online materialization from one definition. It fixes
the root cause and allows backfilling new features without waiting, but it is heavy infrastructure, does
not cover transformations inside the model server, and is only worth building when many teams share
features.

*(c) Continuous skew monitoring.* Sample served vectors, recompute the same features offline for the same
entity and timestamp, alert on per-feature divergence (population stability index, or KL against a
reference window). It detects rather than prevents — but it is the only defense that catches an upstream
data source silently changing while (a) and (b) are both correct.

**Choose (a) as the foundation and (c) as the alarm; add (b) when the organization is large enough.**

**Point-in-time correctness deserves its own warning.** Joining an impression from three weeks ago against
the *current* value of `author_follower_count` leaks the future into the past; the model shows excellent
offline AUC and does nothing online, because at serving time that future does not exist. This is the most
common way an offline evaluation lies, and logging features as served makes it impossible by construction.

### 7.3 The latency budget, and why hundreds

The 150 ms ranking budget, spent:

```
auth, request setup, experiment assignment          5 ms
candidate generation (parallel, hard deadline)     40 ms
dedup + seen-set + integrity filter                10 ms
light ranker over 500                               8 ms
feature fetch for 100 (single batch)               25 ms
heavy ranker over 100 (batched)                    35 ms
re-rank, diversity, business rules                  5 ms
serialization of the ranked order                   2 ms
                                                  ------
                                                  130 ms   with 20 ms of headroom
```

Two stages are worth expanding. **The feature fetch must be one round trip.** Fetching per item is 100
concurrent lookups, and at 2 ms each the p99 is the slowest of 100, which is not 2 ms; one batched read of
100 keys is one p99. Same tail-amplification argument that killed the pull design in Chapter 20, §6, at a
much smaller scale.

**Model batching is a latency/throughput trade with a specific answer.** Accelerators are efficient at
large batches, so the serving layer can hold requests briefly to accumulate one. At 35,000 requests/sec a
10 ms window collects 350 requests — enormous batches, excellent utilization, and 10 ms added to every p99
including requests that arrived at the window's start. A 3–5 ms window collects 100–175, already past the
point of diminishing returns on utilization, for a third of the latency. Take 3–5 ms. Batching *within* a
request is free — 100 candidates is already a batch — which is why per-item cost is quoted as amortized.

**This is also the answer to "why not score more candidates."** Doubling to 1,000 costs 35 ms more in the
light ranker and doubles the feature fetch, pushing the budget past 200 ms and the fleet past 7,000 cores,
in exchange for candidates retrieval already judged worse. The candidate count is set by the budget, not
by the model.

### 7.4 Re-ranking for diversity and business rules

The score-optimal list fails in ways users describe as "the feed is broken": one author dominating, five
consecutive videos, three near-duplicate posts about the same event, nothing from a subscribed source.
None of these are model errors — each item genuinely has the highest predicted engagement — because
**the model scores items and the user experiences a slate.** Two approaches.

*Learned slate ranking* trains a listwise model over a whole ordering, capturing substitution effects
directly. Theoretically correct, practically fragile: the labels you have are per-item engagements, not
slate satisfaction, so the objective is built on a proxy, and the training set contains only slates your
previous policy produced. Worth pursuing once you have session-level labels; not the place to start.

*Rule-based post-processing* is a greedy pass over the survivors emitting the highest-scoring item subject
to constraints: at most 3 per author, at most 2 consecutive items of the same media type, a similarity
penalty against already-selected items (a marginal-relevance discount over retrieval's own embeddings), at
least one subscribed-source item per 5 positions, plus integrity demotions and ad slots.

**Choose the rules**, and be honest about why: they are auditable, individually testable, instantly
changeable when a policy or legal requirement lands, and explainable to a user or a regulator. A learned
diversity objective is none of those. The cost is that the rule set grows monotonically, the rules
interact in ways nobody models, each needs its own experiment, and in three years there will be a
constraint nobody can remove because nobody can prove it is safe to. A real permanent burden, and still
the right trade.

### 7.5 The feedback loop, exploration, and choosing what to optimize

The model is trained on data the model generated. Three named pathologies follow.

**Position bias.** An item at position 1 receives far more clicks than the identical item at position 20,
so training on raw clicks teaches the model to predict position. Fixes: log position and either include it
as a feature pinned to a constant at inference (the model factors position out and never ranks on it), or
reweight examples by inverse propensity. Both require that position was logged, which is why §4 demands it.

**Popularity feedback.** Items the model ranks highly get shown, get engagement, accumulate better
engagement features, and get ranked higher, while new and niche items never accumulate the features that
would let them compete. The model is not wrong; it is correctly describing a world it created. **Policy
narrowing** is the same effect on the viewer: you stopped showing a category, they stopped engaging with
it, and the model infers a narrow preference. The training set then has no support outside the current
policy, so every counterfactual estimate — "what if we had shown X" — is unfounded.

**Exploration is the fix, and its cost is quantifiable.** Reserve a small share of slots for under-exposed
content: ε-greedy at 4% of positions, Thompson sampling over the predicted-engagement posterior, or a
boost for low-impression items that decays as impressions accumulate. If explored items convert at half
the rate of exploited ones, 4% of slots costs about 2% of short-term engagement. **Spend it.** The
justification is not fairness; it is that without exploration the training data has no coverage outside the
current policy, so nothing can be measured counterfactually and the system cannot discover it is wrong.

**Finally: what the score optimizes is a product decision, not a learned one.** A single-objective model
trained on clicks produces clickbait, reliably and fast, because clicks are the cheapest signal and exactly
the one low-quality content is best at generating. The standard structure is a **multi-task model**
predicting p(click), p(like), p(comment), p(share), p(hide), p(long dwell), with a final score that is a
hand-tuned weighted combination — typically with a large negative weight on p(hide). Hand-tuned is not a
hack: it is where the product's values are written down, and they must be legible, reviewable, and
changeable by people who do not train models. Learning the weights needs a ground truth you do not have.

### 7.6 What ranking does to Chapter 20's timeline cap

Chapter 20 capped each materialized timeline at ~800 entries and served the newest 50. Ranking breaks
three assumptions in that sentence.

**The cap must be reconsidered as a time window, not a count.** Ranking needs ~250 graph candidates after
filtering removes 30% for seen, blocked, and deleted items, so ~400 usable entries. For a user following
200 accounts posting once a day, 800 entries is four days of content — ample. For a user following 5,000
accounts, 800 entries is under four hours, so yesterday's excellent post is not merely ranked low, it was
never a candidate. Cap by *both* count and age, or cap per source, and accept the extra storage for
high-following users.

**A seen-set becomes mandatory.** A chronological feed self-deduplicates: the cursor is a timestamp and
the past does not change. A ranked feed has no such invariant — the same high-scoring item wins again on
the next request — so "shown to this user" must be stored and filtered on. Exact storage costs
500M × 10,000 post IDs × 8 B = 40 TB; a Bloom filter (Chapter 04, §5) at a 1% false-positive rate holds
10,000 entries in ~12 KB, giving 6 TB. **Choose the Bloom filter and name the cost:** roughly 1% of
eligible items are silently suppressed per user, permanently and undetectably, and an individual complaint
about a missing post cannot be debugged. An acceptable trade at this scale, and a genuine loss of
correctness that should be said out loud rather than glossed.

**Pagination stops being a position and becomes a session** (§4). And the timeline stops being purely
disposable: the candidate list is still derived and rebuildable, but the seen-set is not — losing it makes
every user see repeats — so it needs real persistence even though everything around it is a cache.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Model server slow or down | Ranking stage times out | Fall back to light-ranker order, then to chronological. Serve a worse feed, never no feed |
| Feature store down | All features null; model outputs garbage that still looks like scores | Refuse to score on a null rate above threshold and fall back; never rank on defaults silently |
| Feature staleness (streaming job lagged) | Scores drift on recency-sensitive features; no error anywhere | Freshness timestamp per feature group; alert on lag, not on absence |
| Skew introduced by a pipeline change | Model quality decays over days | Log-as-served (§7.2a); per-feature distribution monitoring (§7.2c) |
| Label pipeline broken | Training set becomes all-negative; next model is worse and nothing failed | Alert on daily positive-label counts and label-rate ratios, not just row counts |
| Bad model deployed | Engagement drops across a bucket | Staged rollout by percentage; automatic rollback on guardrail metrics; registry pins the previous artifact |
| Retrieval source timeout | That source's quota goes unfilled | Hard gather deadline; backfill the quota from cheaper sources |
| Seen-set loss (or ranking-session loss) | Widespread repeats — the most visible ranking bug | Replicate the seen-set; treat it as durable state, not cache (§7.6) |
| Exploration bucket misconfigured | Either no exploration (loop tightens) or too much (engagement drops) | Alert on realized exploration share, not on the config value |

**ML systems fail differently, and this is the paragraph to say it in.** A stateless service that breaks
returns 500s and pages someone in ninety seconds. A ranking system that breaks returns 200s at normal
latency containing worse content, and the instrument that detects it is an engagement metric whose
confidence interval is measured in days. Therefore the monitoring cannot be latency and error rate; it
must include **prediction distribution and calibration** — mean predicted engagement versus mean realized
engagement per bucket, which catches a broken model within hours — **per-feature null rates and
distribution drift**, **feature freshness lag**, **daily label volume and positive rate**, and **a
permanent holdout** of a small user population served a chronological or randomly ordered feed, which is
the only way to measure what the whole ranking system is worth rather than what the last change was worth.
That holdout costs real engagement on real users and is worth it, because without it nobody can answer
whether the system is better than sorting by time.

---

## 9. Common mistakes

1. **Trying to precompute ranked feeds.** The score depends on viewer, context, time, and model version;
   materializing it means 2.5 × 10¹¹ values invalidated on every deploy, and a feed that cannot include
   today's posts.
2. **Not separating retrieval from ranking.** The corpus is 10⁸ and the budget affords 10³. That factor of
   10⁵ is the reason the architecture has two stages, and a design that does not name it has not been
   derived.
3. **One model over all candidates.** A quality model at 1 ms/item over 500 candidates is 17,500 cores.
   Production rankers are funnels, and the funnel is a capacity decision, not a research preference.
4. **Unioning candidate sources without quotas**, letting whichever source can generate the most candidates
   win, and comparing scores that were calibrated on different exposure distributions.
5. **Ignoring training–serving skew**, the failure that actually happens, produces no errors, and corrupts
   every subsequent model.
6. **Recomputing features offline for training** using current values, leaking the future into the past and
   producing a model with excellent offline metrics and no online effect.
7. **Not logging impressions**, leaving the training set with no negatives and a model that learns
   popularity instead of preference.
8. **Serving the raw top-K by score**, producing a slate dominated by one author and one media type,
   because the model scores items while users experience slates.
9. **No exploration**, tightening the feedback loop until the training data has no support outside the
   current policy and every counterfactual claim about the system is unfounded — and **treating the ranker
   as a hard dependency**, so that a model outage becomes a feed outage instead of a chronological feed.

---

## 10. Variants

**TikTok-style feeds.** The follow graph contributes almost nothing, so candidate generation is dominated
by the unconnected source and §7.1's quotas mostly allocate among recommendation strategies. The unit of
consumption is one full-screen item, so ranking is effectively a sequence decision and session-level
signals (watch-through, replays, skips) are both fast and abundant — which makes the feedback loop tighter
and exploration more important, not less.

**YouTube home.** The canonical published two-stage system: candidate generation narrows millions to
hundreds, ranking orders them, and the objective is expected watch time rather than click probability —
a deliberate choice made precisely to avoid the clickbait dynamic of §7.5.

**LinkedIn / professional feeds.** Lower volume, so the latency budget is easier, but creator-fairness and
"did the poster get any distribution" constraints become core, moving work out of the model and into
§7.4's re-ranking pass. **E-commerce ranking** is structurally identical with a purchase label — rare,
delayed, monetarily valued — so §7.5's multi-task weights are calibrated against revenue rather than
negotiated as a product preference, and offline evaluation is far more trustworthy.

**Notification ranking.** Superficially similar and structurally different: the decision is *send or do not
send*, a threshold on a predicted value, not a sort over a slate. Diversity is irrelevant; frequency
capping and fatigue models replace it. See Chapter 32.

---

## 11. Further reading

- Chapter 20 for retrieval; Chapter 21, §7.5 for retrieval without a graph edge; Chapter A4 for the
  recommendation system this chapter specializes
- Covington, Adams, Sargin, "Deep Neural Networks for YouTube Recommendations" (RecSys 2016) — the clearest
  public description of the two-stage funnel
- Sculley et al., "Hidden Technical Debt in Machine Learning Systems" (NeurIPS 2015) — skew, feedback
  loops, and why the model is the small part
- Martin Zinkevich, "Rules of Machine Learning" (Google) — §7.2 and §7.5 in checklist form
- He et al., "Practical Lessons from Predicting Clicks on Ads at Facebook" (ADKDD 2014) — calibration and
  online feature pipelines
- Gomez-Uribe and Hunt, "The Netflix Recommender System: Algorithms, Business Value, and Innovation" (2015)
