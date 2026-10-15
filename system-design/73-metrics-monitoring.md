# Chapter 73 — Metrics and Monitoring System

> **Prerequisites:** Chapters 01 (storage engines, partitioning), 03 (streaming, event time), 04 (§5 probabilistic structures, §7 observability), 70 (the aggregation pipeline this specializes)
> **Patterns:** time-series storage, cardinality as the binding constraint, rollups and retention tiers, mergeable sketches, alert evaluation at scale

---

## 1. The problem

Thousands of machines emit numeric measurements continuously — request counts, latencies, queue depths,
CPU. A monitoring system ingests them, stores them cheaply enough to keep for a year, answers arbitrary
queries over them in under a second, and evaluates thousands of alerting rules against them every minute.

This is Chapter 70's aggregation pipeline with three requirements changed, and each change is
consequential. The data is **numeric and regular** rather than event-shaped, which permits compression
ratios that are impossible for general data. The query pattern is **exploratory** — a human at a dashboard
slicing by arbitrary dimensions — rather than a fixed set of known aggregations. And the system is
**load-bearing during incidents**, which means it must be more available than the things it monitors, and
must not fail in the same ways they do.

**The property that makes it hard:** the cost of the system is driven not by the volume of data but by the
number of distinct time series, and that number is a product of label values chosen by application
developers who are not thinking about it. A single well-intentioned line of instrumentation — adding a
`user_id` or a raw URL as a label — can multiply the storage and memory requirements by six orders of
magnitude overnight. **Cardinality is the binding constraint, it is controlled by people outside the
system, and it fails suddenly rather than gradually.** Every other design decision here is downstream of
managing that.

---

## 2. Requirements

### Functional

1. Ingest numeric samples tagged with a metric name and a set of key-value labels.
2. Query with aggregation over arbitrary label subsets and time ranges.
3. Evaluate alerting rules continuously and fire notifications on threshold breach.

Defer, but name: logs and traces (different systems with different storage shapes — §10), long-term
capacity forecasting, and the notification delivery itself (Chapter 32).

### Non-functional

- **Ingest rate** — 10^7 samples/sec. Derived in §3.
- **Query latency** — p99 under 1 second for a dashboard panel over 24 hours; under 10 seconds for a
  quarter. Dashboards issue dozens of queries at once, so per-query latency is multiplied by panel count.
- **Alert evaluation latency** — every rule evaluated every 15–60 seconds, with the *total* evaluation
  round completing inside the interval. This is a throughput requirement disguised as a latency one.
- **Retention** — full resolution for days, downsampled for a year. Not one number; §7.3.
- **Availability** — **must exceed the availability of everything it monitors.** A monitoring system that
  fails when the datacenter is unhealthy is worse than useless, because it removes visibility exactly when
  visibility is needed. This requirement drives the isolation decisions in §7.7.
- **Consistency** — eventual, and lossy is acceptable. Dropping 0.1% of samples changes no decision;
  blocking an application's request path to guarantee a sample's delivery is unacceptable. State this
  explicitly, because it licenses fire-and-forget ingestion.

### Explicitly out of scope

Distributed tracing, log aggregation, and profiling. They share the "observability" label and share almost
no design.

---

## 3. Estimation

Assume 10,000 hosts running 50 services.

**Series count — the number that matters**

```
per host:     ~500 series (CPU, memory, disk, network, per-core, per-mount)
per service:  ~2,000 series (RED metrics × endpoint × status × instance)

10,000 hosts × 500                      = 5,000,000 host series
50 services × 200 instances × 2,000     = 20,000,000 service series
                                        ─────────────────
                                        ≈ 25,000,000 active series
```

**Twenty-five million active time series.** Hold this number; §7.1 shows how one careless label makes it a
hundred times larger.

**Ingest rate**

```
25e6 series ÷ 15 s scrape interval ≈ 1.7 × 10^6 samples/sec
with a 10 s interval               ≈ 2.5 × 10^6 samples/sec
```

Call it 2 million samples/second sustained. Each sample is a `(series_id, timestamp, float64)` triple.

**Raw storage, and why compression is not optional**

```
naive: 2e6/sec × (8 B series + 8 B ts + 8 B value) = 48 MB/sec
                                                   ≈ 4.1 TB/day
                                                   ≈ 1.5 PB/year
```

A petabyte and a half a year is not a monitoring budget. Now with time-series compression (§7.2):

```
compressed: ~1.3 bytes/sample
2e6/sec × 1.3 B = 2.6 MB/sec ≈ 225 GB/day ≈ 82 TB/year
```

**A factor of roughly 18.** This is the single largest lever in the system, it comes from two specific
techniques (delta-of-delta on timestamps, XOR on values), and it is the reason a purpose-built time-series
store beats a general one by an order of magnitude rather than a few percent.

**Memory for the index**

```
25e6 series × ~200 B of label metadata and posting-list entries ≈ 5 GB
```

Five gigabytes of *index*, which must be resident, independent of the sample data. This scales with series
count, not with sample count — which is why cardinality, not ingest rate, is what makes these systems fall
over.

**Alert evaluation**

```
2,000 rules × 1 evaluation / 30 s = 67 rule-evaluations/sec
each scanning ~1,000 series over a 5-minute window
                                  ≈ 67,000 series-reads/sec
```

Modest against the ingest rate, but it is *query* load on the same storage, arriving in a synchronized
burst at each interval boundary — §7.5.

---

## 4. API

```
POST /write                                    # or scrape: GET /metrics on the target
  body: [{name, labels:{...}, samples:[[ts, value], ...]}]
  -> 204

GET /query?expr=<expr>&time=<ts>               # instant query
GET /query_range?expr=<expr>&start=&end=&step= # range query, for graphs
  expr: sum by (service) (rate(http_requests_total{status=~"5.."}[5m]))
  -> 200 {series:[{labels, values:[[ts,v],...]}]}

GET /labels , GET /label/{name}/values         # for autocomplete in the UI
```

Four decisions:

**Ingest returns `204` and never blocks meaningfully.** The client is an application whose real job is
serving users. A monitoring write path that applies backpressure to its clients converts a monitoring
problem into an outage — and it will do so precisely during an incident, when metric volume spikes. Drop
samples rather than block, and count the drops (which is itself a metric, and must be a metric that
survives the drop path).

**`step` is a required parameter on range queries.** A graph is a fixed number of pixels wide; asking for
one point per pixel rather than one per sample is what keeps a year-long query cheap. Making the caller
state it prevents the pathological case of a UI requesting 30 million points to render 800 of them.

**The query language is functional over labeled series, not SQL.** The operations that matter —
`rate()` over a counter, aggregation across a label dimension, and joining two series by shared labels —
are awkward in SQL and natural in a purpose-built expression language. This is a case where a domain
language earns its keep.

**A separate label-values endpoint.** Query builders need to enumerate label values, and doing that
through the main query path scans data. Serving it from the inverted index (§5) directly is orders of
magnitude cheaper — and it is also the endpoint that reveals a cardinality explosion first, because it is
the one that returns a million values.

---

## 5. Data model

```
series identity      = metric name + sorted set of label key-value pairs
series_id            = hash(that)          -- stable, so a restart re-derives it

inverted index (label -> series):
  "service=api"     -> [series_id, ...]     -- posting list
  "status=500"      -> [series_id, ...]
  "__name__=http_requests_total" -> [...]

chunks (per series, per ~2-hour window):
  series_id, start_ts, end_ts,
  compressed_block                          -- delta-of-delta ts + XOR values
```

Three deliberate decisions:

**Series identity is the full label set, and labels are immutable.** Adding a label to an existing metric
does not modify a series; it creates a new one and orphans the old. This is why a deploy that changes
instrumentation causes a **cardinality churn** spike — the old series stay in the index until they age out.
Understanding that series identity is the whole label set, not the metric name, is the single most
important thing to know about this data model, and it is what makes §7.1's failure mode inevitable rather
than merely possible.

**Query resolution goes through an inverted index, exactly as in Chapter 61.** A query for
`{service="api", status="500"}` intersects two posting lists to find matching series, then reads their
chunks. The label matching problem and the search problem are the same problem, and the same intersection
machinery applies — including the observation that a highly selective label should be intersected first.

**Data is chunked by time, typically 2-hour blocks.** This makes retention a matter of deleting whole
blocks rather than expiring individual rows, makes compression work on a contiguous run of samples from
one series, and means a query over a time range reads a bounded number of blocks. Time-based chunking is
what turns retention from a delete-heavy workload into a file-removal operation.

---

## 6. Architecture, derived

### Attempt 1: rows in a relational database

```sql
CREATE TABLE samples (series_id BIGINT, ts TIMESTAMP, value DOUBLE);
```

Fails on both axes simultaneously.

**Writes:** 2 million inserts/second into a B-tree indexed on `(series_id, ts)`. Each insert is a random
page write, since samples for 25 million series arrive interleaved. This is precisely the workload B-trees
handle worst (Chapter 01, §1).

**Storage:** 48 MB/sec uncompressed, 1.5 PB/year. Row overhead alone — headers, per-row indexing — exceeds
the 24 bytes of payload.

Rejected on arithmetic, not on principle. Worth one sentence to establish that the specialization is
earned.

### Attempt 2: an LSM-based key-value store

Key on `(series_id, timestamp)`, value the float. Sequential writes, good compression, horizontally
scalable.

Much better, and this is where a lot of homegrown systems land. Two remaining problems:

**Generic compression underperforms.** A general-purpose block compressor on a mixed key-value stream gets
perhaps 3–4×. The 18× figure from §3 requires knowing that timestamps are near-regular and values are
near-constant, which is domain knowledge a generic engine does not have.

**No label index.** Finding "all series where `service=api` and `status=500`" means scanning keys or
maintaining a separate index by hand. The query pattern is exploratory and multi-dimensional; a key-value
store answers point and range lookups on one ordering.

### Attempt 3: a purpose-built time-series store

Add the two things attempt 2 lacked: **domain-specific compression** (§7.2) and an **inverted index over
labels** (§5). Chunk by time and by series.

```
write path:  sample → in-memory head block (per series, append-only)
             → every ~2h, compress and flush to an immutable block
             → block contains its own index + chunks
```

This is the Gorilla/Prometheus/InfluxDB shape, and it is the answer. Note the structure: an in-memory
mutable head, periodic flushes to immutable blocks, background compaction of blocks — the same LSM shape
as Chapter 01, §1 and the same segment shape as Chapter 61, §7.2, arrived at a third time from a third set
of requirements.

### Attempt 4: rollups, because full resolution for a year is not affordable

82 TB/year at full resolution (§3) is technically storable and mostly useless: nobody queries
15-second-resolution data from eight months ago, and a query that spans a quarter at that resolution
returns half a billion points to render 800 pixels.

Downsample into tiers (§7.3): full resolution for days, 5-minute aggregates for weeks, 1-hour aggregates
for a year. Storage drops by two orders of magnitude for the long tail, and long-range queries get faster
because they read fewer points.

### Attempt 5: separate alert evaluation from ad-hoc query

Alerting is a *predictable*, *periodic*, *high-priority* workload; dashboards are *unpredictable*,
*bursty*, and *lower-priority*. Running them against the same query path means an engineer's expensive
exploratory query during an incident can delay alert evaluation during the same incident.

Give alerting its own read path — ideally its own replicas — so the two cannot starve each other. §7.5.

### Final architecture

```
  targets (10k hosts, 50 services)
        │  expose /metrics
        ▼
  ┌─ scrapers (sharded by target hash) ─┐    pull model, §7.4
  └──────────────┬──────────────────────┘
                 │  2M samples/sec
                 ▼
        ┌── ingest / distributor ──┐  hash(series_id) → shard
        └────┬───────────┬─────────┘
             ▼           ▼
        ingester 1 ... ingester N      (in-memory head blocks,
             │                          replicated ×3, WAL for crash recovery)
             │  flush every ~2h
             ▼
        object storage: immutable blocks (compressed chunks + per-block index)
             │                    │
             │                    └──► compactor ──► downsampled blocks (§7.3)
             ▼
     ┌── query path ──────────────┐    ┌── alert path (isolated) ──┐
     │  querier: fan out to       │    │  ruler: evaluate 2,000    │
     │  ingesters (recent) +      │    │  rules every 30 s,        │
     │  object storage (historic) │    │  jittered (§7.5)          │
     │  merge, evaluate expr      │    │        │                  │
     └──────────┬─────────────────┘    └────────┼──────────────────┘
                ▼                               ▼
            dashboards                  Alertmanager: dedup, group,
                                        silence, route → Chapter 32
```

Note the split at the bottom: the query path and the alert path read the same data through separate
processes, so a runaway dashboard query cannot delay an alert.

---

## 7. Deep dives

### 7.1 Cardinality explosion — the defining failure

Everything in this system scales with the number of distinct series. Series identity is the full label set
(§5). Therefore **the cost is the product of the cardinalities of every label**, and a developer adding one
label multiplies it.

Concretely. A service exposes:

```
http_requests_total{service, endpoint, method, status}
   50 services × 20 endpoints × 4 methods × 8 statuses = 32,000 series
```

Fine. Now someone adds `instance` to distinguish pods:

```
   × 200 instances = 6,400,000 series
```

Still survivable, and it is the reason instance-level metrics are the largest legitimate contributor.
Now someone adds `user_id`, reasoning that per-user error rates would be useful:

```
   × 10,000,000 users = 6.4 × 10^13 series
```

The system is dead. Not degraded — dead. The in-memory index alone would need petabytes, and it happens at
the speed of a deploy rolling out.

**Unbounded label values are the failure mode**, and the common culprits are always the same: user IDs,
request IDs, session IDs, full URLs with embedded identifiers (`/orders/8817263`), error messages as
labels, email addresses, and timestamps. Each is an *identifier*, and identifiers do not belong in labels.

**The rule, stated so it can be applied without judgment: a label's value set must be bounded and known in
advance.** If you cannot enumerate the possible values, it is not a label. `status=500` is a label;
`user_id=8817263` is not. High-cardinality identifiers belong in **logs or traces**, which are indexed
differently and priced accordingly — and pointing that out is the correct answer to "but I need per-user
error rates", because the honest response is that metrics is the wrong system for that question.

**Defenses, and you need all of them:**

- **Per-metric series limits at ingest.** Reject samples for a metric that exceeds its configured series
  budget, and emit a metric about the rejection. Rejecting data is unpleasant and it is much better than
  losing the cluster — this is a load-shedding decision, and framing it that way makes it easier to defend.
- **Per-tenant global limits**, so one team cannot consume the shared budget.
- **Cardinality attribution in the UI**: show which metrics and which labels contribute the most series.
  Without this, nobody can fix a problem they cannot see, and the on-call engineer is left guessing.
- **Alert on cardinality growth rate,** not just on absolute level. The absolute number tells you when you
  are dying; the growth rate tells you which deploy did it, while it is still rolling out.
- **Review instrumentation the way you review schema changes.** Adding a label is a schema change with a
  multiplicative cost, and treating it as a code detail is the root cause.

**Churn is the second-order version.** Even if instantaneous cardinality is bounded, series that come and
go — pod names in an autoscaling deployment, container IDs, build versions — accumulate in the index
because the index must retain them for the retention period of their data. A deployment that rolls 200
pods creates 200 × 2,000 new series every deploy, and at ten deploys a day the index grows steadily even
though active cardinality is flat. Use stable identities (a pod ordinal or a deployment name) rather than
generated ones wherever the distinction is not needed.

### 7.2 Time-series compression, and why it gets 18×

Two techniques, each exploiting a property of this specific data. Together they are the difference between
1.5 PB and 82 TB a year.

**Delta-of-delta encoding for timestamps.** Samples arrive at a near-fixed interval, so successive deltas
are nearly identical and the *second* difference is almost always zero:

```
timestamps: 1600000000, 1600000015, 1600000030, 1600000045
deltas:                 15,         15,         15
delta-of-deltas:                     0,          0
```

A zero delta-of-delta encodes in **one bit**. A small deviation (a scrape 200 ms late) encodes in a handful.
In practice the overwhelming majority of timestamps cost a single bit each, down from 64.

**XOR encoding for values.** Consecutive values of a gauge or counter are usually close, and floats that
are numerically close share most of their leading bits. XOR the current value with the previous one: an
unchanged value gives all zeros (one bit), and a small change gives a short run of meaningful bits
surrounded by zeros, which encodes as an offset, a length, and the bits themselves.

```
value 82.13 → 82.15 : XOR has few significant bits → ~10-15 bits
value unchanged      → XOR is zero               → 1 bit
```

**The combined result is ~1.3 bytes per sample**, against 16 for a naive timestamp-plus-value pair.

Two consequences worth stating:

- **The compression is only available on a contiguous run of samples from a single series**, because both
  techniques are differential against the previous sample of the *same* series. This is why the storage is
  chunked per series per time window rather than storing samples in arrival order — the physical layout
  exists to enable the compression.
- **Irregular data compresses far worse.** Event-driven metrics with jittered timestamps, or gauges that
  swing wildly, lose most of the benefit. If someone proposes pushing every event as a metric, this is one
  concrete reason it is expensive.

### 7.3 Retention tiers and rollups

Nobody queries second-resolution data from six months ago, and if they did, the answer would be
unrenderable.

| Tier | Resolution | Retention | Relative size |
|---|---|---|---|
| Raw | 15 s | 15 days | 1× |
| 5-minute rollup | 5 min | 90 days | 1/20 × 6 = 0.3× |
| 1-hour rollup | 1 h | 400 days | 1/240 × 27 = 0.11× |

Total is roughly 1.4× the raw 15-day cost, for a year of history. Without rollups, a year at raw resolution
is 27× that. **The rollups are what make long retention affordable.**

**Rollups must store multiple aggregates, not one.** Downsampling to "the average over 5 minutes" destroys
the ability to answer anything else. Store `count`, `sum`, `min`, `max` for each window — and for
percentiles, store a **mergeable sketch** (§7.4). With `count` and `sum` you can compute a correct average
over any coarser window; with only the average you cannot, because averaging averages of unequal counts is
wrong.

**Queries must transparently select the tier.** A query over 24 hours reads raw; over a quarter, reads
hourly. The user should not choose, and the resolution should be derived from the requested `step`
(§4) — asking for one point per hour over a year should never touch raw data.

**Deletion is block removal, not row deletion.** Because data is chunked by time, expiring a tier means
deleting whole immutable blocks from object storage. This is the payoff for time-based chunking noted in
§5, and it is why retention costs essentially nothing operationally.

### 7.4 Percentiles, and why averaging p99s is wrong

A common and consequential error: computing p99 per shard or per instance and then averaging them.

```
shard A: 1,000 requests, p99 = 100 ms
shard B: 1,000 requests, p99 = 100 ms
average of the p99s = 100 ms
```

The true p99 of the combined 2,000 requests could be anywhere from 100 ms to far higher — it depends
entirely on the shape of the two distributions, which the p99 values do not carry. **Percentiles are not
linear and do not average.** The same error appears when averaging a p99 over time to produce a daily
number.

Two correct approaches:

**Histogram buckets.** Each instance reports counts per latency bucket (`le=10ms`, `le=50ms`, `le=100ms`,
…). Bucket counts *are* additive, so summing them across instances gives the true combined histogram, and
the percentile is interpolated from it. Cost: each histogram is one series per bucket, so a 20-bucket
histogram is 20 series — which interacts directly with §7.1, and is why histograms are the largest
contributor to cardinality in most deployments. The accuracy is bounded by bucket width, and the buckets
must be chosen in advance.

**Mergeable sketches (t-digest, DDSketch).** A compact structure summarizing a distribution, with the
property that two sketches merge into a sketch of the union. Better tail accuracy than fixed buckets, no
need to choose boundaries in advance, and a single series per metric instead of one per bucket. Cost: the
value is an opaque blob rather than a number, so the storage and query layers must understand it.

For rollups (§7.3), sketches are strictly better: a t-digest downsampled to an hour still answers p99
correctly, whereas storing "the p99 of each 5-minute window" and combining them does not.

### 7.5 Alert evaluation at scale

Two thousand rules, each evaluated every 30 seconds, each reading a few hundred to a few thousand series
over a 5-minute lookback.

**The synchronization problem.** If every rule is scheduled on the same interval boundary, all 2,000
evaluate simultaneously at `:00` and `:30`, producing a spike of tens of thousands of series reads and
then twenty-five seconds of idle. The read path must be sized for the spike, not the average — a factor of
several in capacity. **Jitter the evaluation start times** across the interval; this is the same fix as
Chapter 92's `0 * * * *` thundering herd, and it costs one line.

**Isolation from ad-hoc queries.** Alert evaluation must not be delayed by an engineer running an
expensive query — and the moment that is most likely is during an incident, when engineers are querying
hard and alerts matter most. Separate query paths, separate resource pools, and if necessary separate
read replicas. This is a correctness requirement for the alerting system, not a performance nicety.

**`for` duration and flapping.** A rule that fires the instant a threshold is crossed will fire on a
single-scrape blip. Requiring the condition to hold for a duration (`for: 5m`) is the standard fix, and it
means the evaluator holds *state* per rule — the timestamp when the condition first became true — which
must survive an evaluator restart or every restart re-arms every pending alert.

**Deduplication and grouping happen downstream.** When a datacenter loses power, a thousand rules fire at
once. Sending a thousand notifications is worse than sending none, because the signal is lost. Group by
common labels, apply inhibition rules (a "datacenter down" alert suppresses the thousand "host down"
alerts it implies), and rate-limit. This is a distinct component — Alertmanager in the Prometheus
ecosystem — and it exists because the failure mode it solves is universal.

**Alert on symptoms, not causes** (Chapter 04, §7). "Checkout error rate above 1%" is actionable and
user-relevant. "CPU above 80%" fires constantly during healthy load and misses every failure that is not
CPU-bound. The most common way a monitoring deployment becomes useless is not that it fails, but that its
alerts are ignored — and cause-based alerting is how it gets there.

### 7.6 Push versus pull

**Pull (scrape).** The monitoring system fetches `/metrics` from each target on a schedule.

- The scrape itself is a health check — a target that cannot be scraped is visibly down, with no extra
  mechanism.
- Rate is controlled centrally; a misbehaving application cannot flood the system.
- Targets are stateless with respect to monitoring; anyone can curl `/metrics` to debug.
- Requires **service discovery**, and requires network reachability from the monitoring system to every
  target — which is a real problem across NAT, firewalls, and for short-lived jobs that exit before
  anything scrapes them.

**Push.** Targets send to a collector.

- Works for short-lived batch jobs, for targets behind NAT, and across network boundaries.
- The collector cannot distinguish "target is down" from "target stopped sending", so liveness needs a
  separate mechanism.
- A buggy client can flood the ingest path, which is why the ingest API must have per-tenant rate limits
  (§4).

**In practice both, with pull as the default** and a push gateway for the cases pull cannot reach. The
decision is dominated by the network topology and by whether short-lived jobs matter, not by aesthetics —
and saying that is better than defending one model on principle.

### 7.7 The monitoring system must not share fate

The availability requirement in §2 says this system must be *more* available than what it watches. That
has concrete consequences that are easy to state and often skipped:

- **Do not run it on the infrastructure it monitors** in a way that shares a failure domain. A monitoring
  cluster that depends on the same control plane, the same DNS, or the same storage as production will be
  down at the same time.
- **Alert delivery must not depend on the monitored system.** If notifications route through your own
  message queue, a queue outage is silent.
- **Have a dead-man's switch.** A rule that fires continuously when everything is *healthy*, routed to an
  external service that alerts when it *stops* receiving. Without it, "no alerts" is ambiguous between
  "nothing is wrong" and "the monitoring system is dead" — and those are the two most different states in
  the system.
- **Federate rather than centralize globally.** A per-region monitoring cluster that survives losing its
  region's connectivity to the global one, with a global layer aggregating for cross-region views. The
  local cluster remains authoritative for local alerting.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Cardinality explosion from a deploy | Ingesters OOM; whole system down within minutes | Per-metric and per-tenant series limits enforced at ingest; alarm on cardinality growth rate; attribution UI |
| Ingester loss | Recent in-memory data lost for its shard | Replication factor 3 across availability zones; WAL replay on restart |
| Query overload from a dashboard | Queriers saturated; alerting delayed if not isolated | Separate alert read path; per-query series and time-range limits; query timeouts |
| Alert evaluation falling behind | Alerts fire late or not at all — silent failure | Alarm on evaluation duration vs interval; this is the metric that catches it |
| Object storage unavailable | Historical queries fail; recent data and alerting unaffected | Recent window served from ingesters; degrade rather than fail |
| Scrape failures | Gaps in series; can be mistaken for the target being healthy-but-idle | `up` metric per target, alerted on separately from the target's own metrics |
| Monitoring system down entirely | Total loss of visibility, and silence looks like health | Dead-man's switch to an external service; independent failure domain |
| Clock skew on targets | Samples with future or stale timestamps rejected or misordered | Reject samples too far from now and count the rejections; NTP as a prerequisite |

**Monitoring the monitoring:** active series count and its growth rate (the leading indicator for the
worst failure); ingest rate and drop rate; per-tenant limit rejections; alert evaluation duration against
the interval; query latency and the count of queries hitting the series limit; block compaction lag; and
the dead-man's switch, which is the only one that catches total failure.

---

## 9. Common mistakes

1. **Not identifying cardinality as the binding constraint.** Designing for sample throughput when the
   system dies from series count is the central error of this chapter.
2. **Allowing unbounded label values.** User IDs, request IDs, and raw URLs as labels. The rule is that a
   label's value set must be enumerable in advance.
3. **Using a general-purpose database.** Rejected on the 18× compression figure and the absence of a label
   index — both arguments quantitative, not stylistic.
4. **Averaging percentiles across shards or over time.** Mathematically wrong, extremely common, and it
   produces numbers that are confidently incorrect rather than obviously broken.
5. **Storing only the average in rollups**, destroying every other question you could have asked of that
   window.
6. **Running alert evaluation on the same path as ad-hoc queries**, so an incident-time investigation
   delays incident-time alerting.
7. **Not jittering rule evaluation**, requiring the read path to be sized for a spike that lasts 5% of the
   interval.
8. **Alerting on causes rather than symptoms**, producing noise that trains people to ignore pages.
9. **Running the monitoring system inside the failure domain it monitors**, with no dead-man's switch, so
   that total failure is indistinguishable from total health.

---

## 10. Variants

**Log aggregation.** Superficially adjacent, structurally different: log lines are high-cardinality,
variable-length text, so the storage is an inverted index over tokens (Chapter 61) rather than compressed
numeric chunks, and the cost model is dominated by index size rather than series count. The correct answer
to "we need per-user error counts" is usually "that belongs in logs", and knowing why the systems differ
is what makes that answer credible.

**Distributed tracing.** Per-request spans linked by a trace ID. Cardinality is unbounded by construction —
every trace is unique — so the design is built around **sampling**, either head-based (decide at request
start) or tail-based (buffer the trace, keep it if it was slow or errored). Storage is a span store keyed
by trace ID, not a time series. Tail-based sampling is the interesting part and it is a stream-processing
problem of Chapter 70's shape.

**Business metrics and product analytics.** Same ingestion shape, different requirements: exactness
matters (revenue cannot be sampled), retention is years, and queries are ad-hoc aggregations over
high-cardinality dimensions — which is an OLAP columnar store (Chapter 70's sink), not a time-series
store. The distinguishing question is whether high-cardinality *grouping* is required; if yes, this
chapter's design is the wrong one.

**Real-user monitoring.** Metrics emitted by browsers and mobile clients. Push by necessity, untrusted and
therefore rate-limited and validated aggressively, wildly irregular timestamps (so §7.2's compression
underperforms), and a client population you cannot upgrade — so the schema must be stable for years.

---

## 11. Further reading

- Chapter 70, for the general stream-aggregation pipeline this specializes; Chapter 61, for the inverted
  index; Chapter 04, §5 and §7, for sketches and the RED/USE framings
- Pelkonen et al., "Gorilla: A Fast, Scalable, In-Memory Time Series Database" (VLDB 2015) — the source of
  the delta-of-delta and XOR compression in §7.2, and still the clearest exposition
- Beyer et al., *Site Reliability Engineering* (Google), chapters 6 and 10, for symptom-based alerting and
  the practice around it
- Ted Dunning and Otmar Ertl, "Computing Extremely Accurate Quantiles Using t-Digests", for §7.4
- The Prometheus documentation on storage, cardinality, and recording rules, for how these ideas are
  implemented and operated in practice
