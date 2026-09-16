# Chapter A4 — Recommendation System

> **Prerequisites:** Chapters 01 (storage), 02 (caching), 20 and 22 (the feeds this ranks), 61 (retrieval then ranking), 70 (the event pipeline that produces training data)
> **Patterns:** multi-stage retrieval and ranking, feature stores and training-serving skew, feedback loops, exploration, silent degradation

---

## 1. The problem

From a catalog of millions of items, choose the few dozen to show a particular user right now.

Every ranked surface in this book ends here: Chapter 20's feed once it stops being chronological,
Chapter 51's search results, Chapter 42's homepage, Chapter 52's candidate ordering. The pattern is always
the same and the mistake is always the same — treating it as a modeling problem when it is a systems
problem with a model inside it.

Two things make it unlike everything else in Part II. **The correctness criterion is statistical and
delayed.** There is no assertion that fails when the recommendations are bad; the system returns 200 OK,
the latency is fine, every dashboard is green, and engagement declines over the following week. A
recommender does not break, it **degrades silently**, and the entire operational design exists to make
degradation visible.

**And the system changes the data it learns from.** A recommender is trained on logged user behavior, and
that behavior was produced by showing users what the previous model chose. The training set is not a
sample of the world; it is a sample of the model's own past decisions. This closes a loop that no amount of
modeling effort escapes from the inside.

**The property that makes it hard:** scoring millions of candidates with a good model is computationally
impossible inside a request, and scoring a few hundred requires having already chosen the right few
hundred — so the quality ceiling is set by a cheap stage that cannot afford to be good, while the system's
own output determines what it will learn tomorrow.

---

## 2. Requirements

### Functional

1. Return a ranked list of N items for a user in a given context.
2. Ingest interaction events (impression, click, dwell, conversion) as training and feature data.
3. Support ranking-model deployment and evaluation without breaking the serving path.

Defer, but name: the model architecture itself, content moderation and eligibility, ads and paid
placement, and the specific surface's UI logic.

### Non-functional

- **Latency** — p99 under 150 ms for the whole pipeline, which is the budget that forces the multi-stage
  design; §3 itemizes it.
- **Throughput** — 50,000 recommendation requests/sec at peak.
- **Catalog size** — 10^7 items; user base 10^8.
- **Freshness** — a new item must be recommendable within minutes, not days. This is the requirement that
  makes cold start a systems problem rather than a modeling one (§7.3).
- **Availability** — must degrade rather than fail. A recommender returning a popularity-ranked list is a
  worse product; one returning an error is a broken page. State the fallback explicitly — it is a design
  element, not an afterthought.
- **Consistency** — irrelevant between users; **read-your-writes matters within a session**, because a user
  who dismisses an item and sees it again immediately experiences the system as broken.

### Explicitly out of scope

Model training infrastructure, the feature engineering itself, and the experimentation platform — though
§7.6 argues the last of these is the only thing that tells you whether any of it works.

---

## 3. Estimation

Assume 10^7 items, 10^8 users, 50,000 requests/sec at peak.

**The impossibility that forces the architecture**

```
ranking model cost: ~0.5 ms per item on a GPU-backed scorer, batched
scoring the full catalog: 10^7 items × 0.5 ms = 5,000 seconds per request
```

**Eighty-three minutes per request against a 150 ms budget.** Six orders of magnitude. This is not an
optimization problem; no amount of hardware closes a 10^6 gap, so the candidate set must shrink before the
model sees it. That is the entire justification for the multi-stage design and it should be stated as
arithmetic rather than as received wisdom.

**Working backwards from the budget**

```
total budget                              150 ms
  candidate generation (parallel sources)  30 ms
  feature fetch (batched)                  40 ms   ← usually the largest, §7.2
  ranking model inference                  50 ms
  re-ranking + business rules              10 ms
  hydration + serialization                20 ms
                                          ──────
                                          150 ms

ranking budget 50 ms ÷ 0.5 ms/item = ~100 items... 
with batching efficiency on a GPU, realistically 500-1,000
```

**So candidate generation must reduce 10^7 to ~1,000.** A factor of 10,000, performed in 30 ms, by
something cheap. That number is the specification for the retrieval stage, and it is why retrieval is
optimized for *recall* rather than precision (§7.1).

**Feature volume**

```
1,000 candidates × ~200 item features
  + ~300 user features
  + 1,000 × ~50 cross features
                       ≈ 250,000 feature values per request
50,000 req/sec × 250k  = 1.25 × 10^10 feature reads/sec
```

Twelve billion feature reads per second is not achievable as individual lookups. It is achievable as
batched reads of precomputed vectors from an in-memory store, which is why the feature store is a distinct
system rather than a cache in front of a database (§7.2).

**Training data**

```
50,000 req/sec × 20 impressions each = 10^6 impression events/sec
plus clicks, dwell, conversions      ≈ 1.2 × 10^6 events/sec
                                     ≈ 10^11 events/year
```

This is Chapter 70's pipeline, and the volume explains why impressions are logged sampled or aggregated
while conversions are logged exactly.

---

## 4. API

```
GET /recommendations
  ?userId=&surface=home&context={device,time,location}&limit=20
  -> 200 {items:[{itemId, score, sourceTag, reason?}],
          requestId, modelVersion}

POST /events
  {requestId, userId, itemId, type: impression|click|dwell|convert|dismiss,
   position, timestamp}
  -> 202
```

Four decisions:

**`requestId` on the response and required on every event.** This is the join key between what was shown
and what happened, and without it there is no training data — only a stream of clicks with no record of the
alternatives the user declined. **The unclicked impressions are the negative examples**, and they are the
majority of the training signal. A design that logs only clicks has quietly made the model untrainable.

**`position` is required on impression events.** Users click the top item far more often regardless of
quality, so a model trained on raw clicks learns to reproduce the previous model's ordering rather than to
predict relevance. Position is the correction, and it must be logged to be corrected for (§7.4).

**`sourceTag` identifies which candidate generator produced each item.** It is how you measure whether a
retrieval source is contributing anything, and how you attribute a quality regression to a stage. Without
it, the pipeline is a single opaque box.

**`modelVersion` is returned.** When a metric moves, the first question is which model was serving, and
answering it from the response rather than by correlating deploy timestamps saves hours.

---

## 5. Data model

```
Offline / batch
  interactions        user_id, item_id, type, position, request_id, ts   -- append-only
  item_features       item_id -> {category, age, quality, popularity, embedding}
  user_features       user_id -> {long-term interests, demographics, embedding}
  training_examples   materialized joins of the above, per model

Online / serving
  feature store (in-memory)
    user:{id}        -> feature vector           ~1 KB
    item:{id}        -> feature vector           ~800 B
  ANN index          item embeddings, for embedding retrieval
  recent-actions     user_id -> last N interactions (session context)
  seen-set           user_id -> Bloom filter of recently shown items
  model registry     version -> artifact, metadata, traffic allocation
```

Three deliberate decisions:

**Features are stored precomputed as flat vectors, not computed at request time.** Computing "this user's
click-through rate on this category over the last 30 days" inside a request is a query; reading a float
from a vector is not. Everything that can be precomputed is, in a batch or streaming job, and the serving
path only reads.

**The same feature definitions produce both the offline training data and the online serving vectors.**
This is the structural defense against training-serving skew (§7.2), and it is a *systems* property — one
definition, two execution paths, verified to agree — rather than a discipline that developers are asked to
maintain.

**Session state is separate from long-term features.** What a user did in the last five minutes is the
strongest available signal and the most volatile. Keeping it in a small, fast, separately-updated store
means the batch feature pipeline does not need to run every five minutes, and it is what makes the
within-session read-your-writes requirement satisfiable.

---

## 6. Architecture, derived

### Attempt 1: score everything

Run the ranking model over the catalog, take the top N. Optimal by construction and, per §3, 5,000 seconds
per request. Rejected on arithmetic.

### Attempt 2: precompute recommendations offline

Compute each user's recommendations nightly, store them, serve by lookup. Microsecond serving.

Three failures, in increasing order of severity:

- **No context.** The same list regardless of time of day, device, or what the user did thirty seconds ago —
  and recent in-session behavior is the strongest signal available.
- **No freshness.** An item added this morning cannot appear until tomorrow, violating §2's minutes
  requirement.
- **Cost.** 10^8 users × a nightly ranking pass is the full-catalog scoring problem again, merely moved
  offline and multiplied by the user count.

Precomputation is not useless — it is exactly right for *candidate generation* (§7.1), where the results
are stable and context-free. It fails as the whole answer.

### Attempt 3: two stages — cheap retrieval, expensive ranking

```
10^7 items ──[retrieval: cheap, recall-oriented]──► ~1,000
          ──[ranking: expensive, precision-oriented]──► top 20
```

The ranking model now scores 1,000 items in 50 ms, which is achievable. The quality question moves to
whether the right items are among the 1,000 — because **anything retrieval misses, ranking cannot
recover.** The two stages have genuinely different objectives, and conflating them is the most common
design error here.

### Attempt 4: many retrieval sources, with quotas

No single retrieval strategy has good recall across all cases. Collaborative filtering fails for new items;
content similarity fails for serendipity; popularity fails for niche interests. Run several in parallel:

```
├─ embedding ANN on user vector      → 300   (semantic similarity)
├─ collaborative filtering           → 300   (users like you)
├─ recent-interest expansion         → 200   (from this session)
├─ trending / popular in segment     → 100   (fresh, high prior)
├─ fresh items (< 24h)               → 100   (cold start, §7.3)
└─ exploration                       → 50    (§7.4)
                                     ─────
                            dedupe → ~1,000
```

**Per-source quotas rather than a merged score.** Sources produce scores on incompatible scales, so ranking
them against each other requires a calibration that does not exist. Fixed quotas guarantee each source's
contribution, make the ablation experiment trivial (drop a source, measure), and make `sourceTag`
attribution meaningful.

### Attempt 5: a third stage for everything ranking cannot express

The ranking model optimizes a predicted engagement probability per item. Several requirements are
properties of the *list*, not of any item:

- **Diversity** — twenty items from the same creator is a bad list even if each is individually optimal.
- **Freshness quotas** — some fraction of the list should be new content.
- **Business rules** — eligibility, regional restrictions, promotional slots.
- **Deduplication against the seen-set.**

These are applied after ranking, over ~200 items, producing 20. Keeping them out of the model is deliberate:
they change frequently, they must be auditable and explainable, and expressing "no more than three items
per creator" as a loss term is both harder and less reliable than expressing it as a rule.

### Attempt 6: make degradation visible

The system now works and can silently get worse. Three mechanisms, all of which are architecture rather
than process: online/offline feature parity checks (§7.2), shadow evaluation before any model takes
traffic, and a fallback path that serves popularity-ranked results when the model or feature store is
unavailable — because §2 requires degrading rather than failing.

### Final architecture

```
  Request (user, context)
        │
        ▼
  ┌── Candidate generation (parallel, ~30 ms) ────────────────┐
  │  ANN index │ CF │ session expansion │ trending │ fresh │  │
  │      └─────┴────┴──────────┬───────┴──────────┴───────┘   │
  │                     dedupe, seen-set filter                │
  └────────────────────────────┬───────────────────────────────┘
                               │ ~1,000 candidates
                               ▼
  ┌── Feature fetch (~40 ms) ──────────────┐   ◄── Feature store (in-memory)
  │  batched: user vector + 1,000 item     │        ▲            ▲
  │  vectors + cross features              │        │            │
  └────────────────┬───────────────────────┘   batch job    streaming job
                   │                            (daily)     (Ch. 70, seconds)
                   ▼
  ┌── Ranking (~50 ms) ──────────────┐
  │  model server, batched inference │  ◄── model registry (version, traffic %)
  └────────────────┬─────────────────┘
                   │ ~200 scored
                   ▼
  ┌── Re-ranking (~10 ms) ───────────┐
  │  diversity, quotas, rules, dedupe│
  └────────────────┬─────────────────┘
                   ▼
              20 items + requestId + modelVersion
                   │
                   ▼
   impression/click/dismiss events ──► Chapter 70 pipeline
                   │
                   ├──► streaming features (session, trending)
                   └──► training data ──► offline training ──► shadow ──► registry
```

The loop at the bottom — serving produces logs, logs produce training data, training produces the model
that serves — is the feedback loop of §7.4, drawn deliberately so it is visible rather than implicit.

---

## 7. Deep dives

### 7.1 Retrieval optimizes recall; ranking optimizes precision

The stages have different objectives, and treating them as "a fast ranker and a slow ranker" is the error
that caps a system's quality.

**Retrieval's job is to not lose the right answer.** Its metric is recall@1000: of the items the user would
have engaged with, what fraction appear in the candidate set? Its ordering barely matters, because ranking
will reorder everything. A retrieval stage tuned for precision — returning 1,000 items it is confident
about — is worse than one returning 1,000 items with high recall and poor ordering, because it has
narrowed the ceiling ranking can reach.

**Ranking's job is to order well.** Its metric is NDCG or AUC over the candidate set. It can afford
hundreds of features and a deep model because it sees a thousand items, not ten million.

**The system's quality ceiling is retrieval's recall.** This is worth saying plainly, because effort
overwhelmingly flows to ranking — it is where the interesting modeling is — while the binding constraint is
usually upstream. Measuring recall@K per source is the diagnostic that reveals it, and most systems do not
measure it.

**Embedding retrieval** deserves specifics since it is usually the largest source. Users and items are
mapped into a shared vector space by a two-tower model; retrieval is approximate nearest neighbor search on
the user's vector. ANN indexes (HNSW, IVF-PQ) return ~300 neighbors from 10^7 items in single-digit
milliseconds.

Two operational consequences that get skipped: **the index must be rebuilt when item embeddings change**,
which is expensive and therefore periodic, so embedding freshness lags model updates. And **user and item
embeddings must come from the same model version** — serving a user vector from model v3 against an index
built with v2 produces results that are not wrong in any detectable way, just quietly poor. Version the
index alongside the model and refuse mismatched pairs.

### 7.2 The feature store and training-serving skew

**The central real-world failure of production ML**, and the one an experienced interviewer will probe.

A feature is computed twice: offline over historical logs to build training examples, and online in the
serving path. If the two computations disagree, the model is trained on one distribution and applied to
another. It performs well offline and poorly in production, and **the discrepancy does not appear in any
error metric** — the serving path returns a valid float that is simply the wrong one.

Ways they diverge, all common:

- **Two implementations.** A Spark job in Scala and a serving path in Go, each implementing "clicks in the
  last 7 days". They differ on timezone boundaries, on whether the current partial day counts, on how
  deleted items are handled. Every one of these is a real bug that has shipped.
- **Time travel.** The offline job computes a feature using data from after the event it is labeling.
  "User's total purchase count" computed at training time includes purchases made after the impression
  being trained on. The model learns to use information it will never have at serving time, and offline
  metrics look excellent.
- **Different freshness.** Offline features are computed over complete daily partitions; online features
  are as fresh as the streaming pipeline. The model learns a relationship on complete data and applies it
  to partial data.
- **Different defaults.** Offline fills missing values with the column mean; serving fills with zero.

**Mitigations, in descending order of effectiveness:**

**One definition, one implementation, two execution modes.** A feature is defined once; the framework
executes it in batch over historical data and in streaming over live data. This is the actual purpose of a
feature store, and it is why one is a distinct system rather than a Redis cluster with feature-shaped keys.

**Log the features as served.** Rather than recomputing features offline for training, write the exact
feature vector used at serving time into the log, joined by `requestId`. Training then consumes precisely
what serving produced, and skew becomes structurally impossible for logged features. The cost is log
volume — 250 KB per request at §3's numbers — so it is usually sampled. **This is the strongest available
defense** and it should be the default recommendation.

**Point-in-time correctness in the offline join.** Every feature must be computed as of the event's
timestamp, never later. Enforcing this in the framework rather than trusting each pipeline is what
prevents time travel.

**Continuous parity monitoring.** Sample serving requests, recompute the features offline, and alarm on
distribution divergence per feature. This catches the drift that the other measures miss and is the only
one that keeps working as the system evolves.

### 7.3 Cold start, on both sides

**New items** have no interaction history, so collaborative filtering cannot place them and their learned
embedding is untrained. Left alone, they are never recommended, so they never accumulate history — a
permanent trap rather than a temporary handicap.

- **Content features.** Category, text, creator, and media-derived embeddings are available immediately.
  A content-based retrieval source can place a new item on its first minute.
- **A dedicated fresh-items quota** in candidate generation (§6, attempt 4). Guaranteed exposure, bounded
  cost — a small, fixed fraction of the list underwrites the entire item lifecycle.
- **Creator priors.** A new item from a creator whose previous items performed well starts with a
  reasonable prior rather than a null one.
- **Fast graduation.** After a few thousand impressions the item has real signal; the streaming feature
  pipeline should surface that within minutes, not at the next daily batch.

**New users** have no history either. The available responses are popularity within an inferred segment
(device, locale, referrer), any onboarding-declared interests, and rapid adaptation — a session-based model
using only in-session behavior often beats a personalized model with no history, and switching between them
based on history depth is a simple and effective policy.

**The asymmetry worth naming:** a bad recommendation to a new user costs one impression; failing to ever
show a new item costs the item its entire future. Cold start is more urgent on the item side, and most
systems under-invest there because the user-side metric is the one on the dashboard.

### 7.4 The feedback loop, and exploration

**The model can only learn from what it showed.** An item never recommended generates no engagement data,
so the model has no evidence it is good, so it continues not recommending it. The model's confidence in its
own ranking is self-confirming.

This compounds into three named pathologies:

**Popularity concentration.** Small initial advantages amplify: a slightly higher-ranked item gets more
impressions, more engagement, higher predicted engagement, and more impressions. Exposure distribution
becomes far more skewed than any measure of underlying quality — the same dynamic as Chapter 52's exposure
problem, and the same fix.

**Position bias.** Users click position 1 far more than position 10 regardless of content. A model trained
on raw clicks learns *what the previous model ranked highly*, not what users prefer. Corrections:
inverse-propensity weighting (weight each example by the inverse of its estimated examination probability
at that position), or randomizing position within a small window for a fraction of traffic to collect
unbiased data. The `position` field in §4 exists for this.

**Filter bubbles.** Optimizing short-term engagement narrows what a user sees, which narrows what they
engage with, which narrows the model's estimate of their interests.

**Exploration is the systematic response.** Reserve a fraction of slots for items the model is *uncertain*
about rather than confident about. The gain is not the immediate impression — it is the data, which is the
only way the model learns anything it does not already believe.

Approaches, briefly: **epsilon-greedy** (a fixed random fraction — crude and effective, and the right
starting point); **Thompson sampling** (sample from the posterior over each item's value, so exploration is
proportional to uncertainty); **upper confidence bound** (optimism in the face of uncertainty). The
sophistication matters less than doing any of it — the common failure is doing none, and it is invisible
because a pure-exploitation system's short-term metrics look best.

**The cost is real and must be argued.** Exploration reduces today's engagement to improve tomorrow's, and
the reduction appears in the metric a team is measured on this quarter while the benefit appears later and
diffusely. This is an organizational problem as much as a technical one, and the honest interview answer
says so.

### 7.5 Serving the model within the latency budget

Fifty milliseconds for ~1,000 items.

**Batch the whole candidate set into one inference call.** Per-item calls waste the accelerator entirely;
the arithmetic intensity argument is the same as Chapter A3's batching.

**Precompute the user tower.** In a two-tower architecture the user representation does not depend on the
candidate, so compute it once per request rather than 1,000 times. In some designs it can be cached across
requests within a session.

**Feature fetch is usually the bottleneck, not inference** (§3's budget puts it at 40 ms against 50 ms for
the model). One batched multi-get for 1,000 item vectors, not 1,000 lookups. This is the single most
common source of latency in a real pipeline and it is a data-access problem wearing a machine-learning
costume.

**Model size is a latency decision.** A larger model that is 2% better offline and pushes p99 past budget is
worse than the smaller one, because the timeout fallback serves popularity-ranked results and that is a far
bigger regression than 2%. Distillation and quantization exist for this reason.

**Cascade if needed.** A cheap model scores 1,000 down to 200; the expensive one ranks those. The same
retrieval/ranking split applied recursively, and worth reaching for only when a single ranking stage cannot
meet the budget.

### 7.6 Evaluation: offline metrics, online experiments, and their disagreement

**Offline metrics** — AUC, NDCG, recall@K over held-out logged data — are cheap, fast, and computable
before deployment.

**They routinely disagree with online results**, and understanding why is the substance:

- Offline evaluation scores the model on **items the old model chose to show**. A new model that would have
  surfaced different items cannot be credited for them, because there is no logged outcome. This biases
  evaluation toward models that agree with the incumbent.
- Offline metrics measure prediction accuracy; the product cares about behavior. A model that predicts
  clicks perfectly may promote clickbait and reduce retention.
- Offline data cannot capture the feedback loop, novelty effects, or how users adapt.

**The online A/B test is the arbiter**, and offline metrics are a filter that decides what is worth
testing. Stating that ordering is the point.

**The deployment path:**

```
train → offline eval (a gate, not a verdict)
      → shadow: serve real traffic, log predictions, serve NOTHING to users
      → 1% A/B → 10% → 50% → 100%
      → automatic rollback on guardrail breach
```

**Shadow mode is the most under-used step.** The model runs on live traffic and its output is logged and
discarded. It catches latency regressions, feature-fetch failures, crashes on real inputs, and prediction
distributions that have shifted — all without exposing a single user. It costs compute and nothing else.

**Guardrail metrics matter as much as the target.** A model that raises clicks while lowering session
length, raising unfollows, or shifting the exposure distribution is a regression. Define the guardrails
before the experiment; adding them after a positive result is how bad models ship.

**Long-term effects require holdout groups.** Some effects — filter bubbles, novelty decay, creator-side
consequences — take weeks. A small permanent holdout on an older model is the only way to measure the
cumulative effect of a year of improvements, and it is worth its cost.

### 7.7 How this system fails, and how to see it

Nothing here throws an exception. The failure modes and their detection:

| Failure | Symptom | Detection |
|---|---|---|
| Feature pipeline stalled | Stale features; quality decays over hours | Feature freshness per feature, alarmed — not job success |
| A feature becomes all-null | Model silently loses a signal | Null rate and value distribution per feature |
| Training-serving skew | Offline good, online bad | Continuous online/offline parity sampling |
| Embedding index stale | Retrieval recall drops; ranking looks fine | Index build age; recall@K on a held-out set |
| Model/index version mismatch | Subtly poor results, no error | Refuse to serve mismatched versions — make it a hard failure |
| Feedback loop tightening | Exposure concentrating over weeks | Gini coefficient of impressions across items, tracked over time |
| Candidate source silently empty | One source returns nothing; list still full | Per-source candidate counts, alarmed on zero |

**The unifying rule: instrument the pipeline's *inputs and intermediate outputs*, not just its result.** By
the time engagement moves, the cause is days old and buried. Per-source candidate counts, feature null
rates and distributions, and prediction-score distributions are the metrics that catch problems while they
are still attributable.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Model server down | No ranking | Fall back to popularity or the previous model; degrade, never error (§2) |
| Feature store down | No features | Serve retrieval order with default features; alarm loudly — this is severe degradation that looks like success |
| Feature store slow | p99 breach, timeouts | Hard timeout with partial features; the model must tolerate missing inputs by design |
| ANN index rebuild failure | Retrieval quality decays silently | Alarm on index age, not on job status |
| Bad model deployed | Quality regression across all traffic | Shadow mode, staged rollout, automatic rollback on guardrails |
| Training data pipeline broken | Models stop improving; nothing breaks | Alarm on training data volume and label distribution |
| Seen-set unavailable | Repeated recommendations within a session | Client-side dedup as a second layer |
| Runaway exploration | Measurable quality drop | Cap the exploration fraction; monitor it as a served quantity, not a config value |

**Monitoring:** latency per stage, since the aggregate hides which stage regressed; per-source candidate
counts and recall@K; feature freshness, null rate, and distribution per feature; prediction score
distribution (a shift is the earliest signal of a data problem); online/offline parity sampling;
exposure concentration over items; and the guardrail metrics for whatever experiment is running.

---

## 9. Common mistakes

1. **Not deriving the impossibility of full-catalog scoring.** The 10^6 gap is why the architecture exists,
   and stating the arithmetic is the difference between deriving the design and reciting it.
2. **Treating retrieval as "fast ranking".** Different objectives — recall versus precision — and retrieval's
   recall is the system's quality ceiling.
3. **Merging candidate sources by score.** The scores are incomparable; quotas are the honest mechanism.
4. **Ignoring training-serving skew.** The dominant real-world failure, and it is invisible in every
   error metric.
5. **Not logging impressions with position and `requestId`.** No negatives and no position correction means
   no trainable data, and it is discovered months later.
6. **No exploration**, so the model can only confirm what it already believes and popularity concentrates.
7. **Trusting offline metrics.** They are a filter for what to test, not a verdict.
8. **Skipping shadow mode**, which is nearly free and catches most deployment failures before users see
   them.
9. **Putting business rules in the model** rather than in re-ranking, where they can be changed and audited.
10. **No fallback path.** When the model is unavailable, popularity-ranked is a worse product; an error page
    is a broken one.
11. **Monitoring only the output metric.** By the time engagement moves, the cause is days old.

---

## 10. Variants

**Feed ranking (Chapters 20, 22).** Candidates come from the follow graph rather than the whole catalog, so
retrieval is largely solved and the emphasis shifts entirely to ranking and diversity. The chronological
fallback is unusually good, which makes the model's incremental value harder to prove and the holdout group
more important.

**E-commerce.** The objective is conversion and revenue rather than engagement, which changes the label
(sparse, delayed, high-value) and adds inventory and price as first-class features. Delayed conversion
means the training label may arrive days after the impression, requiring a delayed-label pipeline — a real
complication with no equivalent in engagement-based systems.

**Video and music.** Consumption is long-form, so dwell and completion matter more than clicks, and
sequential context (what was just watched) dominates. Autoplay makes the recommender's decisions
compounding rather than independent, and the feedback loop tightens accordingly.

**Search ranking (Chapter 61).** A query provides strong explicit intent, so retrieval is query-driven
rather than user-driven and the recall problem largely dissolves. Personalization is a smaller signal
relative to relevance, which is the opposite balance from this chapter.

**Two-sided marketplaces (Chapter 52).** Recommendations must satisfy both sides, so the objective is
mutual rather than one-sided and exposure fairness becomes a first-class constraint rather than a
mitigation.

**LLM-based recommendation.** An emerging shape: use a language model for retrieval by semantic
understanding, or for re-ranking with reasoning over item descriptions. Chapter A3's latency and cost
constraints apply directly, and at 50,000 requests/sec the cost arithmetic is currently prohibitive for
the ranking stage — which is why the realistic near-term uses are offline (generating features or
embeddings) rather than inline.

---

## 11. Further reading

- Chapters 20 and 22 for feeds, 61 for search ranking, 52 for two-sided matching, 70 for the event
  pipeline, A3 for model serving economics
- Covington, Adams, and Sargin, "Deep Neural Networks for YouTube Recommendations" (RecSys 2016) — the
  clearest public description of the retrieval/ranking split and why each stage has a different objective
- Sculley et al., "Hidden Technical Debt in Machine Learning Systems" (NIPS 2015) — the paper that named
  training-serving skew and the surrounding failure modes
- Joachims et al., "Unbiased Learning-to-Rank with Biased Feedback" (WSDM 2017), for the position-bias
  correction in §7.4
- Chapelle and Li, "An Empirical Evaluation of Thompson Sampling" (NIPS 2011), for exploration in practice
