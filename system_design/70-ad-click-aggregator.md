# Chapter 70 — Ad Click Aggregator

> **Prerequisites:** Chapters 01 (partitioning, columnar storage), 03 (logs, delivery semantics,
> idempotency, event time, watermarks, checkpointing, Lambda vs Kappa), 04 (probabilistic structures)
> **Patterns:** stream processing, event-time windowing, exactly-once as a mechanism, append-only
> corrections, batch reconciliation

---

## 1. The problem

An advertiser runs a campaign. Every time a user clicks one of its ads, the click must be counted: shown
on a dashboard within about a minute so the advertiser can pause a campaign that is burning money, and
totaled at the end of the day so the advertiser can be billed for exactly the clicks that happened and
none of the ones that did not.

This is the canonical stream-processing question, and several other chapters defer to it — Chapter 71 for
approximate counting, Chapter 73 for metrics, Chapter 40 for view counts. It arrives wearing other
costumes: an analytics product, an IoT rollup, "count events per entity per minute." The costume changes
the tolerances, not the machinery.

**The property that makes it hard:** the same pipeline serves two consumers with incompatible
requirements. The dashboard wants an answer in seconds and tolerates being slightly wrong. Billing wants
an answer that is exactly right and tolerates waiting a day for it — and "exactly right" here is not an
aesthetic, it is a number on an invoice that an advertiser's finance team will reconcile against their
own tracking and dispute if it disagrees.

Optimize only for freshness and the numbers quietly drift from the raw log and cannot be defended;
optimize only for correctness and the dashboard is an hour old, which is useless for its actual purpose.
**You need both paths, and the content is in how they are reconciled.**

---

## 2. Requirements

### Functional

1. Record a click. The endpoint sits on the user's redirect path to the advertiser's landing page.
2. Query aggregated counts for an ad or campaign over a time range, at minute granularity recently and
   coarser granularity for history.
3. Slice by a small set of dimensions: country, device, placement.
4. Produce daily billable totals per advertiser, reconciled against the raw event log.
5. Invalidate clicks judged fraudulent, reflected in both the dashboard and the bill.

### Non-functional

- **Ingest rate** — 10,000 clicks/second steady state. The same pipeline carries impression events at
  roughly 100× click volume, so **1,000,000 events/second is the design target**, and §7.8 is what
  changes to get there.
- **Freshness** — a click is visible on the dashboard within 60 seconds at p99. This rules out batch.
- **Query latency** — p99 under 1 s for a 24-hour range at minute granularity; under 3 s for 90 days.
- **Correctness** — dashboard numbers may be provisional. Billing numbers, once marked final, must equal
  a recomputation from the raw log and stay reproducible for as long as the invoice can be disputed.
- **Durability** — a click is money. Losing one is lost revenue; counting one twice is fraud committed
  against the advertiser. Both unacceptable, and they are different failures with different fixes.
- **Retention** — raw events 30 days minimum; aggregates for years.

### Explicitly out of scope

Ad serving, targeting, and the real-time auction (Chapter A4 covers the retrieval-and-ranking half);
conversion attribution, a stream-to-stream join over days; budget pacing, which consumes this system's
output. Name them so the interviewer knows you saw them and chose not to build them.

---

## 3. Estimation

Assume 10,000 clicks/second steady state and 100,000 ads with meaningful daily traffic.

**Event volume and throughput**

```
10,000/s × 86,400 s        = 864,000,000 clicks/day
raw event ≈ 200 B (click_id, ad_id, campaign_id, user hash, ts, ip, ua, geo, placement)
864e6 × 200 B              ≈ 173 GB/day   → ×30 days ≈ 5.2 TB
10,000/s × 200 B           = 2 MB/s       (steady state)
1,000,000/s × 200 B        = 200 MB/s     (design target)
```

Two megabytes per second is nothing; two hundred is an ordinary Kafka cluster — a dozen brokers, RF3,
600 MB/s of inter-broker traffic. **Throughput is not the constraining resource**, and saying so early
keeps the discussion off broker counts.

**Aggregate size — the number that actually constrains the design**

The instinct is that aggregation shrinks data. It does not, automatically. Count the cells.

```
(ad_id, minute)                        100,000 × 1,440           = 144e6 cells if dense
                                       non-empty, power-law      ≈  20e6 rows/day → 43× ✓
(ad_id, minute, country, device, plc)  100,000 ×1,440×200×3×5    ≈ 4.3e11 if dense
                                       bounded by events         ≤ 864e6 rows/day → ~1× ✗
(ad_id, hour,   country, device, plc)  100,000 ×   24×200×3×5    ≈ 1.44e9 if dense
                                       non-empty, measured       ≈  50e6 rows/day → 17×,
                                                                    ≈ 3 GB/day at 60 B/row
```

The middle case is the point. With that many dimensions at minute granularity the average cell holds
barely more than one click, so **the aggregate is the same size as the raw stream and you have gained
nothing.** Widening the time bucket is what restores compression.

**The constraining number is not throughput and not storage — it is that the time granularity you can
afford is set by the dimensionality, not by the freshness requirement.** You get minute granularity on
`(ad_id, minute)` and hourly on the full cube; a request for minute-by-country-by-device is a request for
a table larger than the raw log, which is a reason to say no rather than a reason to buy disks.

**Compute.** A keyed windowed sum sustains roughly 100,000 events/second per core with in-memory state, so
1M/s is ~10 cores of aggregation plus shuffle and serialization — call it 40 vCPU. Also small. Every
resource here is boring **except when the key distribution is skewed**, which is §7.4.

---

## 4. API

```
GET  /click?cid=<click_id>&ad=<ad_id>&sig=<hmac>&r=<dest>
       -> 302 Location: <advertiser landing page>

POST /v1/events                                  # collector ingest, batched, internal
       body: {events:[{click_id, ad_id, event_ts, ...}], batch_id}   -> 202

GET  /v1/metrics?ad_id=&from=&to=&granularity=minute|hour|day&group_by=country,device
       -> 200 {rows:[{bucket_ts, dims, clicks, invalidated, net}],
               as_of, finality: "streaming" | "reconciled", watermark_ts}

GET  /v1/billing/daily?advertiser_id=&date=
       -> 200 {gross_clicks, invalidated_clicks, net_billable, finality, reconciled_at, run_id}
       -> 409 if the day is not yet reconciled and the caller demanded finality
```

**The click endpoint does nothing but append and redirect.** It sits inside a page transition the user is
already waiting through, so its budget is a few milliseconds: no counter increment, no call to the
aggregation service, no validation a downstream job could do instead.

**`click_id` is minted when the ad is rendered, not when it is clicked, and it is signed.** Minting at
render time means impression and click share an identifier, which the fraud pipeline needs for its join;
signing means a script cannot manufacture clicks for an ad it was never shown. And because it is unique
per rendered ad instance, it is the **idempotency key** for the whole pipeline (Chapter 03 §4) — a
browser retrying the redirect, a collector re-sending a batch, and a job reprocessing a partition all
produce events that collapse.

**Every response carries `finality` and `as_of`.** This is the API decision that shows you have thought
about the two-consumer problem. A number from the streaming path can still change — late events, fraud
invalidation, a reconciliation correction — and a client that renders it as final will show an advertiser
a figure that later moves. Exposing `watermark_ts` lets a careful client see how much of the range is
still open.

---

## 5. Data model

**The raw log — the source of truth.**

```
Kafka topic `clicks`, 256 partitions, retention 7 days
  key   = hash(click_id)          # NOT ad_id
  value = {click_id, ad_id, campaign_id, advertiser_id, user_hash,
           event_ts, received_ts, country, device, placement, ip, ua}
Archived continuously to object storage, Parquet, partitioned by receipt date, 30+ days.
```

**Partition by `click_id`, not `ad_id`.** Keying by `ad_id` is tempting — aggregation becomes
partition-local with no shuffle — and it is wrong, because it puts a viral ad's entire traffic on one
partition, which no downstream parallelism can fix. Key by `click_id` for a uniform spread and let the
job's `keyBy(ad_id)` do the shuffle. A shuffle is cheap; a saturated partition is not.

**The serving store — an append-only delta table.**

```
clicks_agg   (ClickHouse / Druid / Pinot)
  ad_id UInt64, bucket_ts DateTime, granularity Enum(min,hour,day)      -- sort key
  country / device / placement  LowCardinality(String)
  delta        Int64        # SIGNED. corrections append negatives.
  reason       Enum(stream, late, fraud, reconcile)
  result_uuid  UUID         # dedup key, §7.3
  written_at   DateTime
```

**Every row is a signed delta and every query is a `SUM`.** Nothing is updated in place: a late event
appends `+1`, a fraud verdict appends `−1` with `reason = fraud`, reconciliation appends whatever
difference it computed. Three consequences, all wanted — ingest is a pure append with no
read-modify-write; the `reason` column *is* the audit trail, so gross, invalidated, and net are three
separately reportable numbers rather than one counter that can only state its conclusion; and columnar
stores of this family implement mutation as a part or segment rewrite, so append-only is also the only
shape that performs.

**The dedup index.** `processed_results(result_uuid PK, written_at)` with a 7-day TTL — 50M rows/day ×
40 B ≈ 2 GB/day, 14 GB total — in the OLAP store's own dedup mechanism or a KV store in front of it.

---

## 6. Architecture, derived

### Attempt 1: increment a counter on the click path

```
GET /click -> UPDATE ad_counters SET clicks = clicks + 1 WHERE ad_id = ? -> 302
```

Exactly consistent and dead on arrival. All clicks for one ad hit one row, and row-level lock throughput
on a hot row is 500–1,000 updates/second once contention sets in, because the lock is held across the
transaction's fsync. A modestly popular ad exceeds that alone; the 1M/s target exceeds it by three orders
of magnitude. And it puts a database on the user's redirect path, so every database hiccup becomes a slow
ad click — for a write nobody needed to be synchronous. Note what it does get right: it is exactly once,
trivially. Everything below is an effort to regain that property after trading it for throughput.

### Attempt 2: log the click, aggregate asynchronously

```
GET /click -> append to Kafka -> 302     (~2 ms)
consumer   -> UPDATE ad_counters SET clicks = clicks + 1
```

Latency and durability fixed; two new defects, and both are the content of this chapter.

**`clicks = clicks + 1` is not idempotent.** Consumption is at-least-once (Chapter 03 §3), so a consumer
that writes and dies before committing its offset reprocesses and double-counts. A consumer group
rebalance during a routine deploy does this every time. No setting turns it off.

**The bucket is wrong whenever the pipeline is behind.** Stamping each click into the current wall-clock
minute means a ten-minute lag — rebalance, slow deploy, broker failover — files ten minutes of clicks
into one minute. At 10,000/second that is a **six-million-click spike in a single minute followed by a
ten-minute hole**, in a dashboard whose purpose is to let someone react to a spike. The pipeline is wrong
exactly when it is under stress, which is exactly when the numbers matter.

### Attempt 3: window on event time

Key by `(ad_id, truncate(event_ts, 1 minute))` and let a watermark decide when a window is complete
(Chapter 03 §7). A click arriving ten minutes late is still counted in the minute it happened, so the
dashboard is *incomplete* under lag rather than *wrong* — recoverable, in the same sense as Chapter 20's
stale timeline.

Still broken: the sink is at-least-once, so a restart replays windows already written; and a single hot
`ad_id` still lands on a single keyed operator instance.

### Attempt 4: exactly-once processing, and an idempotent sink

Two mechanisms, applied together and described properly in §7.2 and §7.3. The job checkpoints operator
state; the sink buffers its output and commits it through a two-phase commit coordinated with the
checkpoint; the Kafka read offsets stored in the checkpoint stay in lockstep with the committed output,
so restarting from a checkpoint neither loses nor duplicates. Independently, every emitted aggregate
carries a `result_uuid` derived deterministically from its identity, so a duplicate delivery collapses at
the sink even where the transactional guarantee is unavailable or has been traded away for latency. And
the hot key is salted (§7.4), with the salts summed back at query time.

The streaming path is now correct with respect to its own inputs. It is still not correct about money.

### Attempt 5: reconcile against the raw log

Three things the streaming path structurally cannot get right: events later than the allowed-lateness
bound (a phone offline for six hours; no operable watermark policy covers that); fraud verdicts, which
arrive hours later and reference individual clicks the job has long since discarded; and **bugs in the
streaming job**, which is the most likely of the three and the only one with no in-band detection.

So a batch job reads the retained raw events for a completed day, recomputes from scratch, diffs against
the serving store, and appends corrections. This is §7.5, and it is the most important argument here.

### Final architecture

```
                              ┌──── raw archive (S3/Parquet, 30d) ────┬──► fraud pipeline
                              │                                       │    (async, hrs)
 browser ─► /click ─► collector ─► Kafka `clicks` ──┐            ┌────▼─────────┐   │
            (302, ~2 ms)  (edge pre-agg §7.8)       │            │reconciliation│   │
                          256 parts, key=click_id   ▼            │ batch, D−1   │   │
                              ┌─────────────────────────────┐    └────┬─────────┘   │
                              │ Flink streaming job         │         │             │
                              │  keyBy(ad_id, salt)         │   corrections    invalidations
                              │  event-time tumbling 1 min  │  (reason=reconcile) (reason=fraud)
                              │  watermark −30 s            │         │             │
                              │  lateness 5 min ─► side output ───────┤             │
                              │  checkpoint 30 s, 2PC sink  │         │             │
                              └──────────────┬──────────────┘         │             │
                                             │ delta rows + result_uuid│            │
                                             ▼                         ▼            ▼
              ┌──────────────────────────────────────────────────────────────────────┐
              │ ClickHouse `clicks_agg` — append-only signed deltas                   │
              │   minute rollup (ad_id)  |  hourly rollup (full dimensional cube)     │
              └──────────────────────────────┬───────────────────────────────────────┘
                                             ▼   query service: SUM(delta), finality
```

---

## 7. Deep dives

### 7.1 Event time, and the lateness policy

The key is `(ad_id, truncate(event_ts, 1 minute))` and never `(ad_id, now())`. Chapter 03 §7 establishes
why; this is about the parameters, because the policy is where the judgment lives.

**Which timestamp is `event_ts`?** Not the device clock — a meaningful fraction of consumer devices are
wrong by minutes and a nonzero fraction by years, and a client reporting 2019 will either be dropped as
impossibly late or, worse, open a window in 2019. Use the **receipt timestamp stamped by the collector**,
NTP-synchronized and under your control, carrying the device-reported time alongside for the fraud
pipeline. This costs accuracy for genuinely delayed uploads — exactly the case §7.5 exists to catch, so
a cost already paid for.

**Lateness is bimodal, which is what makes the policy easy.** Measure `received_ts - event_ts` and it
splits: ~99.9% within a few seconds (network, buffering), and a thin tail arriving minutes to hours later
(retries, offline queues, a partitioned collector). No single bound covers both. So:

- **Watermark** at `max(event_ts) - 30 s`, closing ordinary windows half a minute after the fact, well
  inside the 60-second freshness budget.
- **Allowed lateness of 5 minutes** — window state retained, re-emitting an updated result when a
  straggler lands. State cost is five minutes of open windows across 100,000 active keys: a few hundred
  megabytes, free.
- **Side output beyond 5 minutes**, routed to a separate topic, not dropped. The daily reconciliation
  folds them in and does not care about lateness at all, because it processes a closed day.

A policy in that shape — a watermark, a bounded lateness with a re-emit, and a named destination for
everything beyond it — is far stronger than "we handle late events," and costs one sentence.

**The re-emit only works because the sink is a delta table.** A window that emitted 1,000 and later sees
three stragglers emits `+3` with a fresh `result_uuid`, not a corrected `1,003` that must replace the
earlier row. §5's append-only model and this lateness policy are one decision seen from two directions.

### 7.2 Exactly-once, stated as a mechanism

"Exactly-once" is a slogan until you can say what commits what. Concretely:

1. Checkpointing is enabled. Periodically the coordinator injects a **barrier** into the source stream.
   The barrier flows through the operator graph; each operator, on receiving barriers from all inputs,
   snapshots its state — the open windows and their partial sums — to durable storage and forwards it.
2. **The source's state is the Kafka read offsets**, captured in the same snapshot as the window sums.
   This is the pivot: offsets and aggregates are taken at the same logical instant.
3. **The sink is transactional and participates in a two-phase commit.** Between checkpoints its output
   goes into an open transaction — a Kafka transaction, a staged file set, a held insert — invisible to
   readers. On the barrier it *pre-commits*: flush, make the transaction ready, do not commit. When the
   coordinator confirms every snapshot succeeded, it notifies the sinks, which commit.
4. On failure the job restores the last complete checkpoint. State reverts, offsets rewind, and any
   transaction opened after that checkpoint is aborted and its output discarded. Reprocessing the rewound
   offsets regenerates exactly what was discarded.

**Nothing is lost and nothing is duplicated because the offsets and the output move together.** That is
the whole idea, and it is worth being able to say in those words.

Three things it does not cover. **The producer side** — the collector's write to Kafka needs the
idempotent producer (producer ID plus sequence number, so broker-side retries do not duplicate), and a
transaction if a batch spans partitions; without it, duplicates enter the log before the job ever sees
them and the job faithfully counts them twice. **Anything downstream of the sink** reading without
`read_committed` sees aborted transactions. And **non-determinism inside the job** — a window that calls
`now()`, samples randomly, or joins a mutable table produces different output on replay, and the
guarantee silently evaporates.

**The cost, usually skipped: end-to-end latency is bounded below by the checkpoint interval.** The sink
commits only at checkpoints, so a 60-second interval makes a result computed at second one invisible
until second sixty — the entire freshness budget. Drop to 30 seconds and the latency is affordable, but
checkpoint overhead rises (state serialization, barrier alignment stalls, transaction churn), and the
Kafka transaction timeout must exceed the interval by a wide margin or in-flight transactions expire
during a slow checkpoint and the job enters a restart loop.

**The alternative, argued honestly:** run the sink at-least-once and put the correctness in an idempotent
sink (§7.3). You lose the transactional guarantee and gain a latency floor set by the flush interval —
often 5 seconds instead of 30. For a dashboard that is the better trade; for billing, the transactional
path plus reconciliation is worth the latency. **Running both from the same job — a low-latency
at-least-once feed for the dashboard and a transactional feed for billing — is a legitimate design and a
good answer**, because it makes §1's two-consumer tension explicit in the topology instead of pretending
one setting satisfies both.

### 7.3 Idempotency at the sink

Independent of the transactional machinery, every aggregated result carries a UUID used as a dedup key.

```
result_uuid = uuid5(NAMESPACE_CLICKS, f"{ad_id}|{bucket_ts}|{dims}|{granularity}|{generation}")
```

**Derive it, do not generate it randomly.** A random UUID identifies a *delivery*; a derived UUID
identifies a *result*. Under the transactional path the difference is invisible, since an aborted
transaction's output never existed. Under at-least-once it is everything: a replay that recomputes the
same window produces the same UUID and the sink recognizes it, whereas a random UUID presents the
recomputed row as new and doubles the count. The derived key is what lets you drop the two-phase commit
and stay correct.

`generation` is what makes re-emits work. A window that emits provisionally and then re-emits after a
late arrival must not have the second emission swallowed as a duplicate; a per-emission counter gives
each a distinct UUID while keeping every emission reproducible from the window's own state.

Three implementations, in increasing strictness. **`ReplacingMergeTree` on `result_uuid`** deduplicates
at merge time, so queries between merges see duplicates unless they use `FINAL` — fine for a `SUM` merged
long before anyone bills it, not fine if someone reads it in the first minute and calls it final.
**Native insert deduplication** on a block hash is exact but bounded to the last ~100 blocks: good for
retry storms, useless for replaying yesterday. **An explicit KV dedup table** checked per batch is exact,
costs one round trip, and is the 14 GB from §5 — name this one if pushed on strictness.

### 7.4 Hot keys

At 1M events/second, a single viral ad taking 30% of traffic delivers 300,000 events/second to one
aggregation key. A keyed operator instance sustains roughly 100,000/second (§3), so that instance is 3×
oversubscribed. It backpressures upstream, which backpressures the source, which stalls *every*
partition: one ad's popularity degrades the freshness of all 100,000 ads, and adding parallelism does
nothing, because parallelism partitions by key and there is only one key.

**Salt the key:**

```
aggregation key = (ad_id, bucket_ts, salt)   where salt = hash(click_id) mod N
query           = SELECT SUM(delta) ... GROUP BY ad_id, bucket_ts     -- sums the salts back
```

At N = 32 the viral ad's 300,000/second becomes 9,400/second across 32 instances. The sum at query time
is **exact** — salting partitions the events, it does not approximate them, which is why this is a
strictly better tool here than in Chapter 71, where per-shard state is a sketch and merging is lossy.

**The cost is row multiplication.** Salting uniformly turns §3's 20 million minute-rows into 640 million,
erasing the compression that justified the aggregate at all. So salt adaptively: run a heavy-hitter
detector over `ad_id` — a Count-Min Sketch is exactly right, and Chapter 71 §7.1 is the treatment — salt
only keys above a threshold, publish the hot set to the job through a broadcast stream, and have the
query layer always `GROUP BY ad_id` so the sum is a harmless no-op for unsalted keys.

**Do the cheaper thing first: pre-aggregate before the shuffle.** A local combiner in the map task —
accumulate into a small bounded map, flush every second — reduces the volume crossing the network by the
average events per key per flush. For an ordinary ad at 5 events/second that is 1× and useless; for the
viral ad at 300,000/second it is **300,000×**, and the shuffle sees one record per subtask per second.
Local pre-aggregation is therefore *self-targeting*: its benefit is proportional to skew, which is exactly
what you want from a mitigation for skew. Salting is the fallback for when even one subtask's
pre-aggregated stream exceeds what one keyed instance can absorb.

### 7.5 Lambda versus Kappa, for money

Chapter 03 §7 states the position; this is the argument, because it is the one that separates candidates.

**Kappa's claim** is that a batch layer is redundant: the log is retained, so if you need different or
corrected numbers, replay through a new version of the job and overwrite. One codebase, one mental model.
For a dashboard this is right and a batch layer is pure cost.

**For money, keep the batch reconciliation.** Four sources of divergence, none of which the streaming job
can observe about itself:

1. **Events beyond the lateness bound.** §7.1's side output is real data the streaming path deliberately
   excluded. Something must fold it in.
2. **Fraud invalidation** (§7.6). Verdicts arrive hours to days later, referencing individual `click_id`s
   the job discarded when the window closed.
3. **Bugs in the streaming job.** This one is decisive: **the streaming job cannot detect its own bugs,
   so no amount of exactly-once machinery inside it produces confidence in its output.** The only thing
   that catches a deploy that mis-parsed a field for forty minutes is an independent recomputation, and
   the only thing that makes it independent is running different code over the same raw input later.
4. **Sink-level partial failure** — an expired dedup TTL mid-replay, a merge that collapsed rows it
   shouldn't have, an operator who ran a manual backfill twice. In practice this happens.

The reconciliation job reads the archived raw events for event-time day D, applies the fraud verdicts
known as of D+1, recomputes every aggregate from scratch, diffs against `SUM(delta)` in the serving
store, and **appends the difference as correction rows** tagged `reason = reconcile` with a `run_id`. It
then marks the day `finality = reconciled`. Nothing is overwritten, so the gap between what streaming
said and what is true stays queryable forever — which is what an auditor wants and what tells you whether
the streaming path is drifting.

**Now the honest objection.** Classic Lambda's real defect is *two implementations of the same logic*,
which drift: someone fixes a bucketing rule in the streaming job, forgets the batch job, and now the
reconciliation "corrects" correct numbers into wrong ones. That objection is fatal to naive Lambda.

The resolution is that this is not two implementations. **The aggregation is expressed once, in a
framework that runs the same operator graph in both streaming and batch mode over the same schema** —
Flink's unified runtime, Beam's model, or a SQL definition executed by both engines. The batch run
differs only in being fed a bounded, complete input whose watermark jumps to infinity: no lateness
policy, no salting concern, no checkpoint interval. Same logic, different completeness guarantee.

**Kappa-shaped code, Lambda-shaped operations.** That is the position, and it is defensible in a way
"we run a batch job too" is not.

One more property worth naming: because reconciliation exists, **you are allowed to ship a streaming job
that is occasionally wrong**, which is what makes it possible to iterate on it. Take reconciliation away
and every change to the streaming job becomes a change to a billing system.

### 7.6 Fraud invalidation, and why the store must support corrections

Click fraud — bots, click farms, a publisher refreshing its own ads, a competitor draining a rival's
budget — is detected by a pipeline with a fundamentally different shape. It looks at sessions, at the
distribution of inter-click intervals per user hash, at IP reputation, at the impression-to-click ratio
for a placement, at whether the `click_id` was ever actually served. These are windows of hours over
per-entity state, and some are batch model inferences.

So it is a **separate asynchronous pipeline** consuming the same raw topic and emitting verdicts keyed by
`click_id`. It must not live inside the aggregation job: coupling a minute-latency counter to an
hours-latency classifier means the counter runs at the classifier's latency. Its output is retroactive,
which imposes §5's requirement. **Append corrections; never mutate in place.**
A verdict invalidating 4,000 clicks for `ad_id=X` at `12:34` appends `(X, 12:34, delta = −4000, reason =
fraud)`; it does not find the existing row and subtract. Three reasons, in increasing importance:
performance, since columnar stores implement updates as part or segment rewrites and a steady stream of
small updates is the standard way to make one unusable; idempotency, since `SET clicks = clicks − 4000`
run twice is wrong while an append with a `result_uuid` run twice is deduplicated by machinery that
already exists; and **auditability**, since the advertiser will ask what they were charged for, and
gross, invalidated, and net must be three separately reportable numbers attributable to a run. A mutable
counter can represent the answer but never the argument for it.

The consequence for the product is that a dashboard number can go *down*, and the UI must be built to say
why.

### 7.7 Retention and rollup

```
Kafka `clicks`            7 days      replay window for a bad deploy;             ~1.2 TB × RF3
Raw archive (Parquet)     30–90 days  reconciliation, fraud lookback, disputes;    173 GB/day
Minute aggregates         14 days     the dashboard's zoom-in range;              ~1.2 GB/day
Hourly aggregates (cube)  13 months   slicing, year-over-year;                    ~3 GB/day
Daily aggregates          forever     billing history;                            negligible
```

The rollup is itself an aggregation over the delta table and inherits every rule above: re-runnable,
idempotent (derive a `result_uuid` per rolled-up row the same way), and — the operational rule that
matters — **it must only roll up days that are already reconciled.** Rolling up a provisional day and
then expiring the minute-granularity source destroys the ability to apply a correction at the granularity
it was computed at. Reconcile, then roll up, then expire. Note also that rolling up a delta table is
`SUM` per coarser bucket, so corrections fold in for free; rolling up absolute counts would require
knowing which version of each row was current. Another reason the store is append-only deltas.

### 7.8 Pre-aggregation at the edge, from 10k/s to 1M/s

At 10,000/second the collector is a stateless proxy that appends and returns. At 1,000,000/second that is
200 MB/s of Kafka ingress, 600 MB/s of replication, 5.2 TB of retention, and a shuffle carrying every
individual event — all affordable, none free, and dominated by impressions, which nobody bills singly.

**Pre-aggregate in the collector.** Each collector holds a bounded map from `(ad_id, minute, dims)` to a
count, flushes every `F` seconds, and publishes pre-summed records. The job is unchanged: summing partial
sums is the same operation as summing ones. The compression is not free, and the arithmetic says exactly
what you get:

```
reduction = events per (key, flush window) per collector

200 collectors, 10 s flush, 1M/s:  5,000/s each →  50,000 per flush over ~5,000 keys → 10×
 50 collectors, 10 s flush, 1M/s: 20,000/s each → 200,000 per flush over ~8,000 keys → 25×
```

**The reduction improves with fewer collectors and longer flush intervals — both of which are exactly
what a stateless, horizontally-scaled, low-latency edge does not want.** That tension is the content of
this dive: longer flushes cost freshness, fewer collectors cost availability and blast radius. A
ten-second flush across a deliberately modest fleet is a reasonable landing point, spending 10 seconds of
a 60-second budget.

**The serious objection: pre-aggregation destroys the raw events, and §7.5 and §7.6 both require them.**
A pre-summed count of 137 cannot be fraud-scored or reconciled against anything.

**The resolution is to emit both.** The collector writes raw events to a cheap, high-volume,
high-latency path — batched, compressed, straight to object storage, never through Kafka — and
pre-aggregated counts to the low-latency path. Fraud and reconciliation read the archive; the dashboard
reads the aggregates. This also disposes of the other objection: a collector crash loses up to `F`
seconds of in-memory counts, which would be unrecoverable loss, except that the raw path already has
them, so **the daily reconciliation heals it automatically.** The batch layer that existed for
correctness turns out to be what licenses a lossy fast path. That is the design closing on itself.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Stream job lag | Recent windows incomplete; dashboard under-reports | Event-time windowing makes this staleness, not corruption; alert on watermark lag, not consumer lag |
| Job restart / rebalance | Reprocessing from last checkpoint | Checkpoint + 2PC sink (§7.2); derived `result_uuid` dedup as a second net (§7.3) |
| Hot `ad_id` | One operator backpressures the whole job | Local pre-aggregation (self-targeting); adaptive salting above a CMS-detected threshold |
| Collector crash | Up to `F` seconds of pre-aggregated counts lost | Raw archive path is independent; daily reconciliation restores the difference |
| Late events beyond bound | Undercounted minute in the streaming store | Side output → reconciliation appends the delta with `reason = late` |
| Fraud verdict arrives | Counts must decrease | Append negative deltas; report gross / invalidated / net separately |
| Streaming job bug | Silently wrong numbers for the duration | Daily reconciliation diff; page when correction/gross exceeds a threshold |
| OLAP store unavailable | Dashboard down; ingest must not stop | Sink buffers in Kafka; aggregation is decoupled from the serving store |
| Duplicate at the producer | Double-counted before the job sees it | Idempotent producer (PID + sequence); `click_id` dedup in reconciliation as backstop |

**Monitoring:** watermark lag — the leading indicator for freshness and the only one that means anything
under event-time semantics; checkpoint duration and failure rate; end-to-end lag from `event_ts` to
queryable; per-key skew as the busiest subtask's throughput over the median; fraud invalidation rate;
dedup hit rate, where a rise means something upstream is replaying. And above all, **the reconciliation
delta as a fraction of gross, per day, per advertiser** — the single most important metric in the system,
because a drift from 0.1% to 3% is the only signal that the streaming path has broken in a way nothing
else reports.

---

## 9. Common mistakes

1. **Windowing on processing time.** Correct under normal conditions, wrong under lag — the worst
   possible failure profile, since it works in the demo and breaks in the incident. Naming
   `(ad_id, truncate(event_ts, 1 min))` as the key is the most load-bearing sentence in the answer.
2. **Saying "exactly-once" without a mechanism.** If you cannot say what the checkpoint contains, what
   the sink commits, and why offsets and output move together, you are quoting marketing. The follow-up
   is always "how?"
3. **Incrementing a counter in the sink.** `count = count + n` reintroduces the non-idempotency all the
   upstream machinery just removed. Append deltas and `SUM`.
4. **Mutating rows for fraud invalidation.** Expensive in a columnar store, non-idempotent under retry,
   and it destroys the audit trail that is the entire point of a billing system.
5. **No reconciliation, because "Kafka gives us exactly-once."** Exactly-once processing means the job
   faithfully computes whatever it computes. It says nothing about whether that is right.
6. **Two separate implementations for stream and batch** — classic Lambda's fatal defect. Express the
   aggregation once and run it in two modes.
7. **Keying the ingest topic by `ad_id`.** Feels like it saves a shuffle; guarantees a hot partition that
   downstream parallelism cannot fix.
8. **Ignoring the hot key.** The distribution is power-law by construction, the same as Chapter 20's
   follower counts; a design assuming uniform keys has assumed away the problem.
9. **Reporting one number.** Gross, invalidated, and net are three different facts.

---

## 10. Variants

**Impression counting.** Same pipeline, roughly 100× the volume, and nobody bills per impression at the
same precision — which licenses sampling and the probabilistic structures of Chapter 04 §5. The
interesting question is where the tolerance boundary sits; the answer is "wherever money changes hands."

**Unique reach.** "How many distinct users saw this campaign" is not a sum and cannot be derived from the
aggregates above. HyperLogLog per `(campaign, day)` gets it in 12 KB per sketch at ~2% error. Store the
sketch, not the number, so arbitrary ranges merge after the fact.

**Conversion attribution.** A click stream joined to a purchase stream over a multi-day window.
Genuinely harder: state is enormous, and the attribution rule (last click? first click? multi-touch?) is
a business decision that changes the answer by a factor of two. Usually batch over the raw archive,
precisely because the streaming version's state requirement is unbounded.

**YouTube view counts (Chapter 40).** Structurally identical, with no billing and therefore no mandatory
reconciliation; a "view" is a windowed predicate over playback heartbeats rather than a single event; and
the displayed number is deliberately delayed and smoothed to make inflation harder.

**Metrics and monitoring (Chapter 73).** The same shape with the tolerances inverted: lossy is fine,
cardinality binds instead of skew, and nobody reconciles. **Top-K over the same stream (Chapter 71).**
"Which ads are hottest right now" is a different question from "how many clicks did ad X get," and the
structures that answer it cheaply are approximate in ways this chapter would never accept.

---

## 11. Further reading

- Chapter 03 §7 for the streaming model; Chapter 04 §5 for sketches; Chapter 71 for the top-K variant
- [Uber Engineering — Real-Time Exactly-Once Ad Event Processing with Apache Flink and Kafka](https://www.uber.com/us/en/blog/real-time-exactly-once-ad-event-processing/)
- Akidau et al., "The Dataflow Model" (VLDB 2015) — the definitive treatment of event time, windowing,
  watermarks, and the completeness/latency/cost trade-off
- Carbone et al., "Lightweight Asynchronous Snapshots for Distributed Dataflows" (2015) — the Flink
  checkpointing algorithm of §7.2
- Yang et al., "Druid: A Real-time Analytical Data Store" (SIGMOD 2014)
- Apache Flink documentation: event time and watermarks, checkpointing, two-phase-commit sinks;
  ClickHouse documentation on `ReplacingMergeTree` and insert deduplication
- Martin Kleppmann, *Designing Data-Intensive Applications*, chapter 11
