# Chapter 92 — Job Scheduler / Cron Service

> **Prerequisites:** Chapters 01 (partitioning, hot partitions), 03 (queues, at-least-once, idempotency, DLQs), 04 (locks and fencing, §2)
> **Patterns:** timing wheels, leases with fencing tokens, jittered dispatch, catch-up policy

---

## 1. The problem

Run a piece of work at a time — once at a specific instant, or repeatedly on a `cron` expression — across
a fleet, without missing it, without running it twice, and without falling over because ninety percent of
the world's cron expressions end in `0 * * * *`. Everyone has solved a small version of this with a line
in `crontab` on one machine. The interview version is a multi-tenant service: millions of user-defined
schedules, arbitrary targets, at-least-once execution with an exactly-once *effect*, queryable status.

**The property that makes it hard:** the trigger is *time itself*, which is a global input every node
reads slightly differently, which cannot be replayed, and which **does not stop while your system is
down.** A request-driven service that is unavailable for four hours serves no requests and owes nothing
afterward. A scheduler that is unavailable for four hours wakes up holding a debt of forty million
executions it did not perform, and there is no correct generic answer to what it should do about them.
That question — §7.3 — separates candidates who have run a scheduler from those who have drawn one.

---

## 2. Requirements

### Functional

1. Register a job: a one-shot at a future instant, or a recurring `cron` expression with a time zone.
2. Dispatch at the due time to a worker, which executes a target (HTTP call, queue publish, container).
3. Track status: queued, running, succeeded, failed, and the history of past runs.
4. Retry failed runs under a per-job policy; give up to a dead-letter queue.
5. Pause, resume, delete, manually trigger, backfill. Per-tenant isolation and quotas.

Defer, but name: job *dependencies* — a DAG of "run B after A" is Airflow's problem, not cron's, and
conflating them is how schedulers become unmaintainable — and event-driven triggers.

### Non-functional

- **Scheduling accuracy** — p99 dispatched within 1 second of the due time. This number does most of the
  work in §3; ask for it, because a 60-second tolerance is a materially easier build.
- **Durability** — a registered schedule survives anything. Losing one is losing a customer's billing run.
- **Execution semantics** — at-least-once dispatch plus a mechanism preventing concurrent duplicate
  execution. Genuine exactly-once requires the *target* to be idempotent (Chapter 03, §3); the scheduler
  cannot supply it, and saying so is a requirement, not a caveat.
- **Availability** — registration and dispatch fail independently. Registration down is an inconvenience;
  dispatch down accrues the debt in §1.
- **Scale** — 10 million job definitions, with §3's alignment skew as an explicit requirement, not a
  surprise.

### Explicitly out of scope

The workers' business logic, sandboxing untrusted job code (Chapter A2), and workflow orchestration.

---

## 3. Estimation

Assume 10 million registered job definitions across all tenants.

```
average 24 executions/day/job → 240M executions/day
240e6 / 86,400                ≈ 2,780 executions/sec average
```

Unremarkable — a single well-tuned database sustains it, and a candidate who starts sharding here has
mis-sized the problem.

**The alignment skew — the constraining number.** Cron expressions are not uniformly distributed over the
hour. People write `0 * * * *` and `0 0 * * *`; almost nobody writes `37 * * * *`.

```
hourly jobs firing at minute 0:  30% of 10M = 3,000,000 due in the same second
daily jobs at midnight:          20% of 10M = 2,000,000 due in the same second
```

Three million in one second against a 2,780/sec average is a **1,000× instantaneous spike, recurring
every hour, forever, by design.** It is not a tail event to be hand-waved; it is the normal operating
condition, and every decision below exists because of it.

**When the naive poll breaks.** The obvious implementation polls a due-jobs table each second:

```sql
SELECT job_id FROM schedule WHERE next_run_at <= now() ORDER BY next_run_at
  LIMIT 500 FOR UPDATE SKIP LOCKED;
```

Each claimed row costs a row lock, an `UPDATE` of `next_run_at`, and an index write. Batched a hundred
rows per transaction, a single primary sustains roughly **30,000 claimed rows/sec** — a defensible
ballpark; the shape of the conclusion matters, not the third digit. To hold a 1-second accuracy target,
everything due in a tick must be claimed within that tick:

```
break point = claim throughput × accuracy SLO = 30,000 × 1 s = 30,000 due at once
with 30% hourly alignment:  0.30 × N > 30,000  →  N ≈ 100,000 job definitions
```

**The naive poll breaks at about 10^5 jobs, not 10^7.** At 10 million, the last of the three million
hourly jobs dispatches `3,000,000 / 30,000 = 100 seconds` late — a hundred times the SLO. This is the most
useful number in the chapter: a design entirely adequate for a startup's internal tooling is two orders of
magnitude short of the requirement, and it names the only two levers that move — shard the claim, or
flatten the spike.

**Storage**

```
job definitions: 10M × 500 B                       ≈ 5 GB
run history: 240M/day × 300 B × 30-day retention   ≈ 2.2 TB
```

Five gigabytes of schedule fits in memory across the fleet, which is what makes §7.1's timing wheel
possible. The run history is the large table and needs a retention policy on day one.

---

## 4. API

```
POST /v1/jobs
  Idempotency-Key: <client key>
  body: { name, tenantId,
          schedule: { cron:"0 * * * *", timezone:"America/Los_Angeles" }
                  | { at:<ISO8601> } | { everySeconds:n },
          target:   { type:"http"|"queue"|"container", url|topic, headers, body },
          timeoutSeconds, maxRetries, retryPolicy,
          concurrencyPolicy: "allow" | "forbid" | "replace",
          catchUpPolicy:     "run_all" | "run_once" | "skip",
          startingDeadlineSeconds, jitterSeconds | exact:true }
  -> 201 { jobId, nextRunAt }

PATCH  /v1/jobs/{id} { paused:true } -> 204        DELETE /v1/jobs/{id} -> 204
GET    /v1/jobs/{id}/runs?cursor=&limit=  -> 200 { runs:[...], nextCursor }
POST   /v1/jobs/{id}/runs { runAt?, overrides? } -> 202   # manual trigger / backfill

  # worker-facing
POST /v1/runs/{runId}/heartbeat { fencingToken } -> 200 { leaseExpiresAt }
POST /v1/runs/{runId}/complete  { fencingToken, status, output } -> 204
```

**`catchUpPolicy` and `startingDeadlineSeconds` are in the API, not in the implementation.** The most
important shape decision here. What to do about missed executions is a property of the *job*, not of the
scheduler (§7.3), and the only place that knowledge exists is in the caller's head. A scheduler that
hard-codes one behavior is wrong for half its users; one that asks at registration time is right for all
of them, and forces the conversation at the only moment anyone is thinking about it.

**Every worker call carries a fencing token.** `heartbeat` and `complete` reject a token lower than the
one recorded on the run, which is what makes lease expiry safe (§7.2). A completion endpoint taking only
a run ID will eventually accept a result from a process that lost its lease twenty minutes ago and
overwrite a newer, correct one.

**`jitterSeconds` and `exact` are mutually exclusive, and `exact` is a quota'd privilege.** Most jobs do
not care whether they run at `:00:00` or `:00:43`; the ones that insist create §7.4's problem, and making
exactness opt-in and countable is how you keep it rare. **One-shot and recurring share one endpoint**,
because a one-shot is a schedule with no successor and every mechanism below is identical for both.

---

## 5. Data model

```
jobs
  job_id        BIGINT   partition key = hash(job_id)
  tenant_id, name, target, timeout_seconds, max_retries, retry_policy,
  cron_expr, timezone, concurrency_policy, catch_up_policy,
  starting_deadline_seconds, jitter_seconds, paused, version

schedule                                # due-time index: one row per pending occurrence
  shard_id      SMALLINT  partition key  # hash(job_id) % 64
  next_run_at   TIMESTAMP clustering key ASC
  job_id        BIGINT
  occurrence_id VARCHAR                  # deterministic: hash(job_id, scheduled_for)

runs
  run_id        BIGINT   PRIMARY KEY
  job_id, scheduled_for, attempt, state, worker_id,
  lease_expires_at TIMESTAMP,
  fencing_token BIGINT,                  # monotonic, incremented on every lease grant
  started_at, finished_at, result, error
  UNIQUE (job_id, scheduled_for, attempt)
```

**The due index is partitioned by shard first, then by time.** An index on `next_run_at` alone makes "due
now" a single hot key range — one contended B-tree page, or one hot partition in an LSM store (Chapter 01,
§4) — hot precisely when three million rows land on it. Prefixing with `shard_id` spreads that instant
across 64 partitions owned by 64 independent timer nodes. The cost is that a global "what is due
everywhere" query becomes a scatter-gather, which is fine: nothing on the hot path needs one.

**`occurrence_id` is deterministic**, derived from `(job_id, scheduled_for)` rather than generated, so a
scheduler recomputing the same schedule after a crash produces the same identity and the re-derived
occurrence collides with the recorded one instead of duplicating it — Chapter 03, §4's "deterministic
identifiers" applied where it does the most good. Relatedly, **`UNIQUE (job_id, scheduled_for, attempt)`
on `runs` is the actual exactly-once mechanism**: leases make duplicate execution rare, this constraint
makes a duplicate *record* impossible, and §7.2 turns on the distinction.

**Only the next occurrence is materialized, never a horizon of them.** Precomputing the next thousand
fire times is tempting and wrong: the IANA time zone database is amended several times a year (§7.6), so a
schedule materialized six months out can become incorrect while sitting in the table. Compute forward one
occurrence at a time, on completion.

---

## 6. Architecture, derived

### Attempt 1: one machine running cron, then a leader-elected pair

A single box is the right answer for a small internal system — say so, because reaching for a distributed
design before establishing you need one is the most common failure in this question. It breaks at one
machine: no failover, no capacity, and a second machine gives every job two executions. So: two nodes, a
lock in etcd, leader runs the loop. That is real HA and covers a surprising fraction of production
systems, and it fails twice. **Throughput** — one leader doing all claims caps at §3's 30,000/sec, so
accuracy collapses at ~10^5 jobs. **Correctness** — leadership is not mutual exclusion: a leader that
stops the world for a 40-second garbage collection past its lock TTL wakes believing it still leads while
a new leader has been dispatching for thirty seconds, and both dispatch. The lock did not prevent this
and cannot (Chapter 04, §2).

### Attempt 2: shard the schedule and poll

Partition by `hash(job_id) % 64`; one owner per shard via an etcd lease; each owner polls its own
partition with `FOR UPDATE SKIP LOCKED` and publishes due jobs to a queue for a worker pool.

Throughput is now `64 × 30,000 ≈ 1.9M claims/sec`, so the three-million spike drains in about 1.6 seconds
— still over the SLO, and paid for with a fleet sized for a load that occurs for one second in every
3,600. **Breaks at:** the alignment burst, which is a *shape* problem horizontal scaling can chase but
not fix.

### Attempt 3: flatten the spike, then stop polling

**Deterministic jitter** (§7.4) spreads `0 * * * *` over a 60-second window: 3,000,000 / 60 = 50,000/sec
fleet-wide, 780/sec per shard. The 1,000× spike becomes 17×. It is a semantic change — the job no longer
runs exactly at `:00` — which is why it belongs in the API.

**A timing wheel** (§7.1) replaces per-tick polling. Each timer node loads a bounded horizon — everything
due in the next five minutes — from its shard into an in-memory hierarchical timing wheel and ticks it.
Insert and expiry are O(1), the database is read once per horizon instead of once per second, and it
stops being the pacing element entirely.

### Attempt 4: make execution safe

Dispatch is now accurate and cheap, and still at-least-once: a worker that is dispatched to, pauses, and
resumes will run alongside its replacement. The execution side gets **leases with fencing tokens** (§7.2),
renewed by heartbeat (§7.5), backed by the unique constraint on `runs`.

### Final architecture

```
POST /v1/jobs ─► Schedule API ─► jobs  (sharded by hash(job_id))
                                   └─► schedule (shard_id, next_run_at, job_id)
                                            │
   ┌────────────────────────────────────────▼─────────────────────────┐
   │ Timer nodes — one owner per shard, etcd lease                    │
   │   loader: every 30 s pull horizon [now, now+5 min) into memory   │
   │   wheel:  hierarchical timing wheel, 1 s ticks, O(1) insert      │
   │   tick:   emit due occurrences + deterministic per-job jitter    │
   └────────────────────────────────────────┬─────────────────────────┘
        insert run row, UNIQUE(job_id, scheduled_for, attempt)
                                            ▼
                          dispatch topic (partitioned by job_id)
                                            ▼
                    Worker pool: claim lease → fencing token N
                    heartbeat every 10 s ──► runs.lease_expires_at
                                            ▼
                    target: HTTP call · queue publish · container run

   success ──► runs.state = succeeded ─► compute + write next occurrence
   failure ──► retry with backoff (Ch 03 §5); exhausted ─► DLQ + alert
   lease expiry ──► reaper re-dispatches with token N+1 (target rejects N)
```

The database is now the durable source of truth and the recovery path, not the pacing mechanism. That
separation is the design.

---

## 7. Deep dives

### 7.1 Polling a due table versus a hierarchical timing wheel

From §3, the naive poll breaks at ~10^5 job definitions given a 1-second SLO. Its costs are three: a query
per tick per shard whether or not anything is due; two index writes per execution; and lock contention
among pollers on exactly the rows everyone wants.

A **hierarchical timing wheel** (Varghese and Lauck, 1987) replaces the sorted structure with an array of
buckets and a rotating cursor. One wheel of 60 one-second slots covers a minute; a timer for `t + 17 s` is
appended to slot `(cursor + 17) % 60` in O(1), and each tick advances the cursor and fires one bucket,
also O(1) — independent of how many timers exist. Longer delays cascade through coarser wheels (minutes,
hours), a timer being demoted to the finer wheel when its coarse bucket fires. The alternatives are worse:
a sorted index is O(log n) insert plus a range scan per tick, and a min-heap is O(log n) both ways with a
single contended root. The wheel is O(1) with no contention, which is why Kafka's request purgatory and
Netty's `HashedWheelTimer` both use one.

The wheel is in memory, so it is not the system of record and cannot hold everything. The production
design is a **hybrid**: the durable store holds all 10 million schedules, and each timer node loads a
bounded horizon — the next five minutes for its shard, roughly `2,780/s × 300 s ≈ 834,000` timers
fleet-wide, a few megabytes per shard — into its wheel, refreshing every 30 seconds. The database is read
`2,880` times a day per shard instead of 86,400, and the tick loop touches no I/O.

Costs, stated plainly. **Recovery**: a crashed node's wheel is gone and its replacement reloads the
horizon, so the loader must be idempotent and `runs`' unique constraint must absorb re-derivation.
**Horizon boundaries**: a job registered to fire in 20 seconds must be injected into the current wheel
directly, not merely written to the store, or it fires 30 seconds late. **Rebalancing**: when shard
ownership moves the new owner reloads from `now`, so anything due between the old owner's last tick and
the new owner's first load is late — bounded, but real. Below ~10^5 jobs none of this is worth building.

### 7.2 Leases, fencing tokens, and why a lock is not enough

The scenario in full, because the shape matters more than the mechanism:

1. Worker A claims run `R` (job `J`, scheduled 14:00), receives a 30-second lease and **fencing token 7**.
2. Worker A stops the world — GC pause, hypervisor stall, network partition — for 45 seconds.
3. The lease expires at 14:00:30; the reaper re-dispatches; worker B claims it with **token 8** and runs
   the job to completion.
4. Worker A resumes at 14:00:45 still believing it holds the lease, and writes its result.

No lock prevents step 4. Worker A's belief is locally consistent and no message can reach it in time —
Chapter 04, §2's second bug, and no amount of lock-service sophistication removes it, Redlock included.
**The resource must participate**: every worker write carries its token, the `runs` row records the
highest token seen, and a lower token is rejected by the storage layer. Worker A's 7 loses to the
recorded 8 and its completion is refused.

That protects the scheduler's state. It does **not** protect the target: if the job charged a credit card,
worker A charged it before it went to sleep. State the boundary rather than papering over it:

> The scheduler guarantees at-least-once dispatch and at-most-one *recorded* run per occurrence.
> Exactly-once *effects* require an idempotent target — usually keyed on `(job_id, scheduled_for)`,
> which the dispatch payload supplies for exactly this purpose.

That last clause is the practical answer, and it is why `scheduled_for` — the logical due time, not the
actual dispatch time — is part of the contract. A billing job upserting on `(customer, billing_period)` is
safe against any number of duplicate dispatches, a far stronger guarantee than the scheduler can offer
alone. Keep the lease itself in a fast store and the authoritative uniqueness constraint in the durable
one: fast mechanism to make conflicts rare, authoritative constraint to make them harmless — the same
two-stage guard as Chapters 80 and 81.

### 7.3 Catch-up after an outage is a product decision

The scheduler was down from 02:00 to 06:00. At 240 million executions/day that is roughly **40 million
missed executions**, and 3 million of them belong to hourly jobs that each missed four occurrences. What
should happen at 06:00? There is no technically correct answer, and the interview is testing whether you
know that.

**An hourly billing aggregation** must run all four: each covers a distinct hour of usage, skipping one
silently drops revenue and produces a ledger that will not reconcile, and order matters. `run_all`,
serialized, worth being slow. **An hourly cache refresh** must run exactly once: the four missed runs
would each compute the same thing from current data, so three are pure waste and running them
concurrently is a self-inflicted stampede on whatever they read. `run_once`. **An hourly "here is your
report" email** must be skipped, since running all four sends a user four identical emails at 06:00 — a
user-visible incident. And **a health check** is skipped outright: a liveness probe for 03:00, evaluated
at 06:00, is not stale data, it is a lie.

So the mechanism is a per-job policy declared at registration (§4): **`run_all`**, enqueueing every missed
occurrence in order within a bounded catch-up window so a multi-week outage does not produce an unbounded
flood; **`run_once`**, collapsing them into a single run tagged with how many were skipped; **`skip`**,
resuming at the next future occurrence and recording the gap; and **`startingDeadlineSeconds`**, an
orthogonal cutoff after which an occurrence is never run regardless of policy. Kubernetes CronJob
implements exactly this, and its boundary behavior is instructive: if more than 100 schedules are missed
within the deadline window the controller gives up and logs an error rather than flooding — a deliberate
choice to fail loudly instead of silently doing something enormous.

Two operational consequences. The backlog must be **rate-limited on replay** — 40 million executions
draining at 30,000/sec is 22 minutes of pure catch-up on top of the live 2,780/sec, and releasing it
unthrottled reproduces §7.4 at four hours' magnitude. And there must be a **default**, because most users
will not set the field: `run_once` with a deadline of one period is the least-bad universal choice, and
jobs marked financial should be required to choose explicitly rather than inherit it. In an interview:
**ask.** "What kind of job is this?" is the correct response; reaching for a mechanism first is the
mistake.

### 7.4 The thundering herd at `0 * * * *`

From §3: three million jobs due in the same second, a 1,000× spike, every hour, forever. It lands twice —
on the scheduler's claim path, and on whatever the jobs *call*, which is usually a small set of internal
APIs now receiving a synchronized 1,000× burst. The second impact is worse, because those services were
sized for their own traffic and know nothing about your cron table.

**Deterministic jitter** is the fix: offset each job by a stable pseudo-random amount derived from its
identity.

```
offset = hash(job_id) % window,   window = min(period / 10, 60 s)
```

Deterministic rather than random, for two reasons: the job lands in the same slot every run, so a job
reasoning about "the last run was an hour ago" stays consistent instead of drifting between 3,540 and
3,660 seconds; and the offset is recomputable after a crash without being stored. Spreading 3 million jobs
over 60 seconds gives 50,000/sec — a 17× spike, a load-balancing problem rather than an architectural one.
The alternatives are worse: over-provisioning for the peak buys capacity used one second in 3,600, and
queueing the burst to drain it produces exactly the lateness of the naive poll, now hidden inside a queue
where nobody sees it until an SLO report.

The cost is honest and must be documented: **the job does not run at `:00`.** Some users will notice, and
a few genuinely need alignment — a market-open task, a regulatory cutoff, a job that must precede a
downstream system's fixed schedule. Hence `exact: true` under a per-tenant quota, which converts an
architectural risk into a capacity number you can see and cap. Note the parallel to Chapter 02, §3's cache
stampede and Chapter 32, §7.5's notification herd: three unrelated systems, one failure mode, one family
of fixes.

### 7.5 Long-running jobs and lease renewal

The lease TTL is a direct trade-off. Short: fast detection of a dead worker, more heartbeat traffic, more
false expiries under GC. Long: quiet, but a crashed worker's job is stuck for the whole TTL. Thirty
seconds with a heartbeat every ten is the standard shape — detection within 30 s, three chances to miss
before expiry. The volume is trivial: at 2,780 executions/sec and an average duration of 5 seconds,
concurrency is `2,780 × 5 ≈ 13,900` runs, so `13,900 / 10 ≈ 1,390` heartbeats/sec, and even a six-hour job
costs 2,160 heartbeats across its life. Heartbeat volume is never the reason to lengthen a lease — false
expiry is, and false expiry is survivable precisely because §7.2's fencing token makes a resurrected
worker harmless.

**Overlap** is the first long-job problem. A 90-minute job on an hourly schedule overlaps itself; under
`concurrencyPolicy: allow` concurrency grows by 0.5 per hour — twelve simultaneous copies after a day,
eighty-four after a week — until the pool is entirely occupied by one tenant's runaway job. The policy
must be explicit: `forbid` skips the new occurrence and records it as skipped (correct for most jobs),
`replace` cancels the running one, `allow` requires opting in and a concurrency cap.

**`replace` is harder than it looks.** The scheduler generally cannot kill a worker — it has no handle on
the process, and an HTTP target certainly cannot be recalled. What it can do is *revoke the lease*: bump
the fencing token so the old run's writes are rejected, mark it superseded, dispatch the replacement. The
old process keeps running and burning CPU, and anything it has already committed stays committed.
Cooperative cancellation — the worker checking a revocation flag on each heartbeat and exiting — is the
best available, and it is cooperative, meaning a wedged worker ignores it. Worth saying out loud, because
"we'll just cancel it" is the reflexive answer and is not implementable in general.

### 7.6 Time zones and daylight saving

Users write `0 2 * * *` and mean 2 a.m. where they live. Local time is not a monotonic function of UTC,
and twice a year it demonstrates that.

**Spring forward.** In `America/New_York` the clock jumps 02:00 → 03:00, so **the hour 02:00–02:59 does
not exist** and a job at `30 2 * * *` has no valid instant that day. Skip it (defensible, and surprising
to a user who expected a daily job to run daily) or fire at the first valid instant after the gap. Most
schedulers choose the latter; either is acceptable, neither is acceptable *silently*.

**Fall back.** The clock returns 02:00 → 01:00, so **01:30 occurs twice**, 60 minutes apart. Running twice
is wrong for a billing job and harmless for a cache refresh — the same split as §7.3, with the same
resolution: the deterministic occurrence key derives from the **UTC instant**, so the two 01:30s are
naturally distinct occurrences, and a job that must not double-fire declares it and dedupes on local
wall-clock time instead.

Three rules follow, all of which people get wrong. **Store the IANA identifier** (`America/New_York`),
never a fixed offset — `UTC-05:00` is wrong for half the year. **Compute the next occurrence in the job's
local calendar, then convert to UTC** for the due index; never advance by adding 86,400 seconds, which is
correct only where no transition intervenes, precisely the case you are trying to handle. **Materialize
only the next occurrence** (§5), because tzdata is amended as governments change their rules and a fire
time computed six months ahead can be wrong by the time it arrives.

A legitimate alternative worth naming: **support UTC only.** Kubernetes CronJob did this for years, and it
makes every problem above disappear at the cost of a feature users want — a real design choice, not a
cop-out, but it must be a decision rather than an oversight, because retrofitting time zones onto a
scheduler whose stored schedules are all UTC is a data migration with no correct automatic answer.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Timer node dies | Its shard stops firing | etcd lease expiry reassigns the shard; new owner reloads the horizon from `now`; handover-gap occurrences are late, not lost |
| Worker GC pause past the lease | Two processes believe they own the run | Fencing token; the stale writer is rejected by the storage layer (§7.2) |
| Scheduler outage | Missed executions accumulate — 40M in four hours | Per-job catch-up policy plus `startingDeadlineSeconds`; rate-limit the replay |
| Alignment burst | 1,000× hourly spike on scheduler and targets | Deterministic jitter; `exact` under quota (§7.4) |
| A job that never terminates | Worker slot occupied forever | Per-job `timeoutSeconds`; lease expiry frees the record; `concurrencyPolicy: forbid` prevents pileup |
| Poison job crashing workers | Repeated crashes stall the pool | Retry budget then DLQ with an alert (Chapter 03, §5); pause the job after N consecutive crashes |
| Clock skew between nodes | Jobs fire early or twice near a boundary | Monitored NTP; treat the store's clock as authoritative for lease expiry; never compare timestamps from two nodes |
| DST transition | Occurrence missing or duplicated | §7.6 policy, chosen per job and documented |
| Tenant registering a job every second, or an unthrottled backlog replay | One tenant starves the fleet; a catch-up flood takes down targets | Per-tenant rate and concurrency quotas; drain replays at a bounded rate |

**Monitoring:** dispatch lateness as a distribution — `actual_dispatch - scheduled_for`, p50/p99/max — is
the one metric that says whether the system is doing its job. Then due-queue depth per shard; lease expiry
rate (rising means workers are dying or the TTL is too short); run state distribution; retry and DLQ rates
per tenant; wheel occupancy against the horizon; and per-shard ownership churn, since frequent
reassignment means the etcd lease TTL is fighting your GC pauses.

---

## 9. Common mistakes

1. **Reaching for a distributed design immediately.** Below ~10^5 jobs a single leader-elected poller is
   correct, and §3's derivation is what earns the right to replace it.
2. **Assuming a distributed lock gives mutual exclusion.** It does not, and the fix is a fencing token the
   *resource* checks (Chapter 04, §2) — not a better lock.
3. **Claiming exactly-once execution.** The scheduler guarantees at-most-one recorded run; the effect is
   exactly-once only if the target is idempotent on `(job_id, scheduled_for)`.
4. **Treating catch-up as an implementation detail.** It is a product decision with different correct
   answers for a billing job and a cache refresh, and the right interview move is to ask.
5. **Ignoring the `0 * * * *` clustering.** Cron expressions are the least uniformly distributed data in
   any system, and a design validated against uniform load is validated against nothing.
6. **Storing a UTC offset instead of an IANA zone**, or advancing a schedule by adding 86,400 seconds.
   Both are correct except twice a year, which is when someone is paged.
7. **Materializing a long horizon of future fire times**, which bakes in tzdata not yet amended, makes
   every schedule edit a bulk rewrite, and is unnecessary when only the next occurrence is ever needed.
8. **No `timeoutSeconds` and no `concurrencyPolicy`**, so one slow job quietly consumes the worker pool.

---

## 10. Variants

**Delayed-message queues.** SQS delay queues, Redis sorted sets keyed by due time, RabbitMQ's delayed
exchange — the same due-time selection problem with no recurrence and no catch-up policy. A sorted set
with `ZRANGEBYSCORE` is the two-line version of §7.1 and is the right answer below a few hundred thousand
pending timers.

**Workflow orchestration (Airflow, Temporal).** Adds dependencies between tasks, so the scheduler becomes
a DAG executor with a completion-driven trigger alongside the time-driven one; Temporal's durable timers
are this chapter's mechanism with a workflow state machine on top. Do not volunteer a DAG engine when
asked for cron — do name it as the boundary.

**Per-user reminder scheduling.** "Notify me in 30 days" at hundreds of millions of pending timers: the
horizon is enormous and mostly far in the future, so the wheel holds only the near term and the store is
partitioned by due day. This is Chapter 32's `sendAt` path, and its catch-up answer is almost always
`skip` — a reminder three hours late is worse than one not delivered.

**Distributed retry scheduling.** Retries with backoff are a scheduler in disguise — each failure
registers a one-shot for `now + backoff` — so one durable timer service can replace the bespoke retry
loops scattered through most codebases.

---

## 11. Further reading

- Chapter 04, §2, for locks, fencing tokens, and why consensus-backed leases differ from Redis locks;
  Chapter 03, §4–§5, for idempotency, retries, and DLQs; Chapter 32, §7.5, for the same thundering herd in
  a different system
- George Varghese and Tony Lauck, "Hashed and Hierarchical Timing Wheels: Data Structures for the Efficient
  Implementation of a Timer Facility" (SOSP '87) — the original, still the clearest treatment of the O(1)
  argument
- Netty's `HashedWheelTimer` and Apache Kafka's request purgatory — two production timing wheels whose
  source is short enough to read in an afternoon
- [Kubernetes CronJob documentation](https://kubernetes.io/docs/concepts/workloads/controllers/cron-jobs/)
  — `startingDeadlineSeconds`, `concurrencyPolicy`, and the missed-schedule limit
- Quartz Scheduler's misfire instructions — a mature, explicitly enumerated taxonomy of §7.3's catch-up
  policies, and useful evidence that the choice belongs to the job rather than the scheduler
