# Chapter 93 — Pub/Sub Log (Kafka)

> **Prerequisites:** Chapters 01 (LSM, partitioning, replication), 03 (queues vs logs, delivery semantics, idempotency), 04 §6 (consistent hashing, for contrast)
> **Patterns:** append-only log, consumer-owned offsets, leader/follower replication, sequential I/O

---

## 1. The problem

Many producers publish events; many independent consumers read them, each at its own pace, each able to
start over from an arbitrary point in the past. Events survive for days whether or not anyone has read
them. You are designing the broker — Kafka, Pulsar, Kinesis — not using one.

Chapter 03 §2 draws the distinction between a queue and a log and tells you when to reach for each. This
chapter builds the log: what it looks like on disk, who tracks how far each reader has got, how a
partition survives the loss of the machine holding it, and why a system that writes every message to disk
is faster than one that does not.

**The property that makes it hard:** the workload demands two things that normally require opposite
designs. It is a **streaming pipe** — a gigabyte a second, latency in single-digit milliseconds — and it
is simultaneously a **database that retains a week of everything** and can serve an arbitrary historical
position on demand. The reconciliation is a single, ruthless restriction: **give up random access
entirely.** Every write is an append to the end of a file, every read is a sequential scan forward from an
offset, and nothing is ever updated in place. Once you accept that, disk stops being the bottleneck people
expect and becomes the cheapest part of the system. Every design decision below follows from the
restriction, and every limitation — ordering only within a partition, no per-message acknowledgment, no
priorities — is the bill for it.

---

## 2. Requirements

### Functional

1. Publish a record to a topic; the broker assigns it a position and acknowledges.
2. Subscribe as a **consumer group**: the group's members divide the topic's partitions among themselves,
   and each record goes to exactly one member of each group.
3. Multiple groups read the same topic independently, without affecting one another.
4. Read from an arbitrary offset or timestamp — replay is a first-class operation, not a recovery tool.
5. Retain by time or size; optionally compact to the latest value per key.

### Non-functional

- **Throughput** — 1 GB/second of ingest, sustained, and several times that in fanout to consumers.
- **Produce latency** — p99 under 10 ms for an acknowledged write with replication.
- **Durability** — an acknowledged record survives the loss of any one broker, and the tolerance is
  configurable per topic. Not all data deserves the same durability, and paying for the strongest
  everywhere is the most common waste in a Kafka cluster.
- **Retention** — 7 days by default, which is what makes reprocessing after a bug possible (Chapter 03 §2).
- **Ordering** — total order **within a partition**. Not across a topic. This is a requirement, not a
  limitation to apologize for: global ordering would require a single writer for the whole topic and cap
  throughput at one machine.
- **Consumer independence** — a slow or stopped consumer must not affect producers or other consumers.

### Explicitly out of scope

Per-message acknowledgment, per-message delay, priority queues, and selective redelivery — all of which
are the queue semantics of Chapter 03 §2 and are structurally incompatible with consumers owning their
offsets (§7.2). Also out of scope: message transformation, schema enforcement, and cross-cluster
replication (§10).

---

## 3. Estimation

Assume 1,000,000 records/second at 1 KB each, replication factor 3, seven-day retention.

**Write bandwidth**

```
1e6 × 1 KB               = 1 GB/s of logical ingest
× replication factor 3   = 3 GB/s written across the cluster
per broker, sequential NVMe write ≈ 500 MB/s sustained
  → 3 GB/s ÷ 500 MB/s    ≈ 6 brokers for write bandwidth alone
```

**Storage — and this is the number that sizes the cluster**

```
1 GB/s × 86,400 × 7 days = 604 TB of primary data
× 3 replicas             = 1.8 PB
÷ 20 TB usable per broker ≈ 90 brokers
```

**Ninety brokers, not six.** The cluster is fifteen times larger than throughput requires, and it is
**retention × replication** that does it. Two consequences worth stating out loud, because they are the
levers an operator actually has: halving retention halves the cluster, and dropping replication factor
from 3 to 2 removes a third of it at the cost of tolerating one fewer failure. Neither of those is a
tuning knob; both are capacity decisions with a durability price, and knowing which one to reach for is
the difference between a cluster that fits its budget and one that does not.

**Read bandwidth and the page cache**

```
3 independent consumer groups × 1 GB/s = 3 GB/s of reads
```

If every consumer is caught up, those reads are served from the **page cache** — the same pages the broker
just wrote, still resident — so the disk sees zero read I/O. That is the design's central efficiency, and
it depends entirely on consumers staying near the tail. A single consumer that falls a day behind reads
from cold disk, and worse, its scan pulls a day of old data through the page cache and **evicts the hot
tail that everyone else was reading**. One lagging consumer converts a zero-read-I/O cluster into a
disk-bound one for every other consumer on those brokers. This is the most common surprise in operating
one of these systems, and it does not appear in any diagram.

**Partition count**

```
per-partition sustained throughput ≈ 10 MB/s (a single writer, plus consumer parallelism limits)
1 GB/s ÷ 10 MB/s ≈ 100 partitions minimum; provision 200–300 for consumer parallelism headroom
```

Partition count is also the cap on a consumer group's parallelism — a group cannot usefully have more
members than the topic has partitions — which is why §7.4 argues for over-provisioning it up front.

---

## 4. API

```
produce(topic, key|partition, records[], acks=0|1|all) -> {partition, baseOffset}
fetch(topic, partition, offset, minBytes, maxBytes, maxWaitMs) -> recordBatch[]
listOffsets(topic, partition, earliest|latest|timestamp) -> offset
commitOffset(group, topic, partition, offset)
joinGroup / syncGroup / heartbeat(group, memberId)
```

**Fetch is a pull, and that is a deliberate inversion.** A broker that pushed would have to model each
consumer's capacity, and a slow consumer would either be overwhelmed or force the broker to buffer per
consumer — reintroducing exactly the per-consumer state the design is trying to avoid. Pulling makes
backpressure automatic (Chapter 03 §6): a slow consumer simply asks less often, and the broker neither
knows nor cares. `minBytes` with `maxWaitMs` recovers the efficiency a naive pull would lose, by letting
the broker hold a fetch open until enough data accumulates — long polling (Chapter 02 §8), which turns a
low-traffic partition's busy-poll into one blocked request.

**The batch is the unit, not the record.** Producers accumulate records into a compressed batch, and that
batch is written, replicated, stored, and fetched as one object, decompressed only by the consumer. This
is what makes a million records per second tractable: per-record costs — a syscall, a CRC, an index entry,
a network frame — are amortized across thousands of records. A producer configured with `linger.ms=0`
sends batches of one and can lose an order of magnitude of throughput while every configuration file
claims it is tuned for speed.

**`acks` is the durability dial and belongs in the API, not in a config file.** `acks=0` is fire and
forget; `acks=1` waits for the leader's local write; `acks=all` waits for every in-sync replica. Exposing
it per produce request is what lets one application send click telemetry and payment events through the
same cluster with honestly different guarantees.

---

## 5. Data model

A partition is a directory of **segment** files, and the newest segment is the only one open for writing:

```
/orders-7/
  00000000000000000000.log        closed segment: records 0 .. 4,193,102
  00000000000000000000.index      sparse offset  -> byte position
  00000000000000000000.timeindex  sparse timestamp -> offset
  00000000004193103.log           active segment: appends land here
```

**Segments exist so that deletion is a file unlink.** Retention on an append-only structure would
otherwise mean rewriting from the front, which is the one thing this design refuses to do. Roll a segment
at 1 GB or a few hours, and expiring a week of data is a scheduled `unlink` — no compaction, no rewrite,
no read amplification.

**The index is sparse, and that is a real decision.** One entry per few kilobytes of log rather than per
record. Dense would mean `604 TB ÷ 1 KB = 6 × 10^11` entries; sparse at one per 4 KB is 2 MB of index per
1 GB segment, which is memory-mapped and stays resident. A lookup binary-searches the sparse index to a
nearby position and scans forward a few kilobytes — a bounded linear scan is cheaper than an index big
enough to avoid it, which is the same trade an LSM-tree makes (Chapter 01 §1).

Two properties of an offset are load-bearing. It is **assigned by the partition leader at append time**,
which is why a partition has exactly one writer — the offset is a sequence number and a sequence needs a
sequencer. And it is **dense and monotonic**, so the position of record `n` can be found from the index
without a lookup table, and a consumer's entire state is one integer per partition.

Consumer offsets are stored in an ordinary compacted topic (`__consumer_offsets`), keyed by
`(group, topic, partition)`. The broker hosts consumer state in the same primitive it offers to everyone
else, which is a good sign a primitive is the right one.

---

## 6. Architecture, derived

### Attempt 1: a broker that tracks acknowledgments

The classic message queue. The broker holds messages, delivers them, tracks per-message acknowledgment
state, and deletes on ack. Rich semantics: redelivery, priorities, per-message delay.

Three things break at our scale. **Per-message state is per-message memory and random access** — a million
in-flight messages per second means a million mutable records the broker must index, update, and expire,
which is a database workload sitting in front of a network pipe. **A second consumer needs a second copy**,
because delivery destroys, so N independent consumers means N× storage and N× write cost. And **replay is
impossible**: the message is gone, so a bug discovered on Tuesday cannot be fixed by reprocessing Monday.

### Attempt 2: an append-only log with consumer-owned offsets

Invert it. The broker appends records to a file and never removes them on read; each consumer remembers
its own offset. The broker's per-consumer state collapses from "one record per in-flight message" to
**one integer per partition per group.**

Everything follows: replay is a seek; a new consumer group is a new integer, costing nothing; a slow
consumer is invisible to everyone else; and the broker becomes so simple that it is essentially a file
server with a network protocol.

It still runs on one machine, so it caps at that machine's write bandwidth and disk, which §3 says is
1/15th of the requirement.

### Attempt 3: partition the log

Split the topic into P independent logs, each an ordered sequence with its own offsets, spread across
brokers. Throughput and storage now scale linearly with P.

The cost is stated once and permanently: **ordering is now per-partition only.** A producer chooses a
partition by hashing a key, so records sharing a key share a partition and are ordered relative to each
other; records with different keys have no defined order. Everything in §7.4 is the consequence.

A partition is lost with its broker, so this is not yet durable.

### Attempt 4: leader/follower replication with an in-sync replica set

Each partition has R replicas. One is the **leader** and takes all reads and writes; the others fetch from
it exactly as a consumer would, which is elegant — replication reuses the fetch path rather than adding a
protocol. Replicas that are caught up within a bound form the **ISR** (in-sync replica set); a replica
that falls behind is ejected from the ISR and rejoins when it catches up.

`acks=all` means "acknowledged by every member of the ISR," and this is the subtle part. The ISR
**shrinks under failure**, so a slow replica does not block writes — it is removed, and the write proceeds
with the rest. That is why ISR beats a fixed quorum: a 5-node quorum tolerating 2 failures costs 5×
storage and blocks on the third-slowest node every time, while RF=3 with ISR costs 3× and never waits on a
straggler.

The catch is that an ISR can shrink to one, at which point `acks=all` means "acknowledged by one node" and
guarantees nothing. **`min.insync.replicas` is the guard**: set it to 2 and a partition whose ISR has
fallen to one **rejects produce requests**. That is an explicit, per-topic choice of consistency over
availability — the write path for that partition is down until a replica catches up — and it is the right
choice for data whose loss is worse than its delay. Making that choice per topic, rather than per cluster,
is the point.

### Attempt 5: consumer groups and rebalancing

Partitions are distributed among a group's members by a **group coordinator** (a broker elected per
group). Members heartbeat; when membership or partition count changes, the group rebalances. This is the
part that surprises people in production, and it is §7.5.

### Final architecture

```
producers ──batch, compress, partition by hash(key)──┐
                                                     ▼
   ┌────────── broker 1 ──────────┬──── broker 2 ────┬──── broker 3 ────┐
   │ orders-0  LEADER  [ISR:1,2,3]│ orders-0 follower│ orders-0 follower│
   │ orders-1  follower           │ orders-1 LEADER  │ orders-1 follower│
   │   segments on disk +          │                  │                  │
   │   page cache + sendfile       │                  │                  │
   └───────────────┬──────────────┴──────────────────┴──────────────────┘
                   │ fetch(partition, offset)          ▲
                   ▼                                   │ partition metadata,
   group A: 2 consumers  ─┐                            │ leader election
   group B: 5 consumers  ─┴─ offsets → __consumer_offsets (compacted topic)
                                                       │
                                          controller quorum (KRaft):
                                          membership, leaders, ISR
```

---

## 7. Deep dives

### 7.1 Why disk is not the bottleneck

The instinct that "it writes to disk, so it will be slow" is wrong here, and the reasons are three and
quantifiable.

**Sequential writes.** On a spinning disk, a random 4 KB I/O costs an average seek (~5 ms) plus half a
rotation (~4 ms), so about 9 ms:

```
random:      ~110 IOPS × 4 KB ≈ 0.5 MB/s
sequential:  150–200 MB/s
ratio:       300–400×
```

Three orders of magnitude, which is why the original design could claim a spinning disk outran a network.
Be honest about modern hardware: NVMe does ~500,000 random 4 KB IOPS (about 2 GB/s) against 3–7 GB/s
sequential, so the gap collapses to **2–3×**. The argument survives, weaker, and the real modern win is
not the media but everything sequential access lets you avoid: no seek scheduling, no read-modify-write of
partially filled pages, no index maintenance, and perfect readahead.

**The page cache does the caching.** The broker never manages a cache of its own. It writes to the OS page
cache and lets the kernel flush; a consumer reading the tail hits pages that are still resident from the
write. Three consequences that all matter: the broker's JVM heap stays small (no multi-gigabyte cache, so
no garbage collection pauses proportional to cached data); a broker restart does not lose the cache,
because the page cache belongs to the kernel; and the cache is exactly as large as free memory, with no
tuning. **A 128 GB broker gets a ~100 GB read cache for free**, which at 1 GB/s per broker covers roughly
a hundred seconds of tail — comfortably more than a healthy consumer's lag.

**Zero-copy on the read path.** A conventional send moves bytes disk → page cache → application buffer →
socket buffer → NIC: four copies and multiple context switches. `sendfile` sends page cache → NIC by DMA,
eliminating two copies and both user-space transitions. The CPU saving is real but modest — memcpy runs at
roughly 10 GB/s per core, so 3 GB/s of fanout with two extra copies costs about 60% of a core. The bigger
wins are not allocating per-request buffers at all and not polluting the CPU cache with data the broker
never inspects.

**The cost, which is rarely mentioned:** enabling TLS breaks zero-copy outright. Encryption must happen in
user space, so every byte is copied and encrypted, and throughput on a fanout-heavy cluster drops
materially. That is a real trade — wire encryption versus a measurable fraction of your cluster — and it
belongs in the design discussion rather than in a surprise benchmark later.

### 7.2 Consumers own their offsets

The broker does not track what has been delivered. Each group records, per partition, one integer: the
offset of the next record it wants.

**What that buys.** Broker state per group is O(partitions), not O(messages) — the difference between
kilobytes and gigabytes, and the reason brokers are cheap enough to be plain file servers. Replay is a
seek, so reprocessing a week after a bug is an operational command rather than a project. Adding a
consumer group is free: the data is already there, and the new group's reads are served from the page
cache if it starts near the tail. And a slow consumer imposes no cost on anyone, since nothing is buffered
on its behalf.

**What it costs**, and this is the part to volunteer:

- **No per-message acknowledgment**, so no selective redelivery. A record that fails cannot be retried in
  isolation; the consumer must either retry in place, skip it and record that fact, or route it to a
  dead-letter topic (Chapter 03 §5). A **poison message blocks its partition** for as long as the consumer
  keeps retrying, and everything behind it waits.
- **No priority and no per-message delay.** Both require reordering, and the log's order is fixed at write
  time. A delay is implemented with a separate topic and a consumer that sleeps, which is a workaround.
- **Duplicates are guaranteed**, because committing the offset and processing the record are two separate
  actions. Commit before processing and a crash loses records (at-most-once); commit after and a crash
  reprocesses them (at-least-once). There is no third option without a transaction spanning both, which is
  §7.7. **At-least-once plus an idempotent consumer** is the answer (Chapter 03 §3–§4).

The trade is stated plainly: the log gives up per-message control to gain replay, multiple readers, and a
broker that scales. If the workload genuinely needs per-message acknowledgment and priorities, it wants a
queue, and Chapter 03 §2 is where that decision is made.

### 7.3 What breaks when a leader fails

A broker holding leadership for hundreds of partitions dies. What happens, in order:

1. **Detection.** The controller notices the broker's session has lapsed. This takes seconds — a
   heartbeat interval plus a timeout — and is deliberately not shorter, because an aggressive detector
   turns a garbage-collection pause into a leadership change.
2. **Election.** For each affected partition, the controller promotes a replica **from the ISR**. Because
   the ISR is by definition caught up to the high watermark, no acknowledged record is lost.
3. **Propagation.** New metadata reaches producers and consumers, which discover their stale leader
   through an error and refetch metadata.

Total: hundreds of milliseconds to a few seconds. Producers with retries enabled buffer and resend, so the
user-visible effect is a latency spike, not errors — provided the idempotent producer is on, since blind
retries would otherwise duplicate.

**The high watermark is what makes this safe.** Consumers may only read up to the highest offset
replicated to all ISR members. Records past it exist on the leader and are invisible, precisely so that a
consumer can never read a record that a subsequent election would erase. The cost is that end-to-end
latency includes replication: a record is not readable until it is replicated, which is the honest price
of not exposing writes that might be rolled back.

**Unclean leader election** is the configuration to argue about. If the ISR is empty — every in-sync
replica is down — you may promote an out-of-sync replica. It has less data, so **acknowledged records
disappear**, and consumers that already read past its end see the log truncate underneath them. Enabled,
the partition stays available and silently loses data; disabled (the correct default), the partition is
unavailable until an ISR member returns. This is CAP as a per-topic checkbox, and the right answer is
different for a payments topic than for a metrics topic — which is the whole point of it being per-topic.

**Truncation on rejoin.** A returning replica may hold records the new leader never had, so it truncates
to the divergence point. Locating that point by offset alone was historically wrong across multiple
elections; **leader epochs** — a monotonically increasing leadership generation stamped into the log —
make the comparison exact. This is the same fencing-token idea as Chapter 04 §2: an epoch number that lets
the state holder reject a stale writer's claim.

### 7.4 Ordering, and the producer's choice of key

Ordering is per-partition. The producer's partition choice therefore *is* the ordering decision, and it is
made once, early, and is expensive to change.

**Choose the key as the entity whose sequence matters.** For a chat system, `conversation_id`. For an
order pipeline, `order_id`. For a change stream, the primary key. The right question is "what two events
must never be seen out of order?", and the answer names the key.

Three consequences.

**Key skew becomes partition skew.** Partitioning is `murmur2(key) mod numPartitions`, and if one key is a
large share of traffic, one partition is a large share of load and one consumer instance does most of the
work while the rest idle. This is the power-law problem of Chapter 20 wearing different clothes, and the
fixes are the same shape: split the hot key with a suffix and give up its internal ordering, or route it
to a dedicated topic.

**Adding partitions breaks key affinity.** `mod numPartitions` is exactly the `hash mod N` of Chapter 91
§3, with the same defect: growing from 100 to 120 partitions sends a key to a new partition while its
earlier records sit in the old one, so a consumer can process a new event before an older one for the same
entity. Kafka does not use consistent hashing here because a partition is not a cache entry — it is a
durable ordered log that cannot be moved without violating its own order. **The practical answer is to
over-provision partitions at topic creation**, since 300 partitions on a topic that needs 100 costs
metadata and file handles, while resizing later costs correctness.

**In-flight requests can reorder.** With retries enabled and more than one request in flight, a retried
batch can land after a later one. The idempotent producer (§7.7) attaches per-partition sequence numbers,
letting the broker reject out-of-order batches and preserve order with up to five in flight. Without it,
strict ordering requires `max.in.flight=1`, which costs most of the pipelining.

### 7.5 Consumer group rebalancing, and the pause nobody expects

A group's partitions are assigned by the coordinator: members send `JoinGroup`, one member is elected
leader and computes the assignment, the coordinator distributes it via `SyncGroup`. It triggers whenever a
member joins, leaves, times out, or the partition count changes.

**The eager protocol is stop-the-world.** Every member revokes *all* its partitions and waits for the new
assignment before resuming — including the members whose assignment does not change. The whole group stops
consuming for the duration.

```
group of 100 consumers, rolling restart, ~2 s per rebalance
each instance leaving and rejoining = 2 rebalances × 100 instances = 200
                                    ≈ 400 s of group-wide downtime for a routine deploy
```

Six minutes of stalled consumption from a deploy that touched nothing, and consumer lag that takes far
longer to drain. Three fixes, and a production cluster wants all three.

**Static membership.** Give each consumer a stable `group.instance.id`. A member that restarts within its
session timeout reclaims its previous assignment with no rebalance at all, which removes the deploy case
entirely — the single highest-value setting here.

**Cooperative incremental rebalancing.** Two phases: members keep everything, the leader computes the new
assignment and revokes only the partitions that actually move, then a second round assigns them. Members
whose assignment is unchanged never stop. This turns a stop-the-world pause into a partial one
proportional to the churn.

**Watch `max.poll.interval.ms`.** A consumer that takes longer than this between polls is presumed dead
and evicted, triggering a rebalance, which slows everyone, which makes more consumers exceed the interval.
That is a self-reinforcing loop, and it is the most common way a healthy group collapses under a workload
change — usually a batch that got slower, not a broker that got worse. The fixes are to reduce
`max.poll.records`, move slow work off the poll thread, or raise the interval deliberately.

### 7.6 Retention and compaction

**Time or size retention** deletes whole segments once every record in them is older than the threshold.
O(1) per segment, no rewriting, no read amplification — the reason segments exist.

**Log compaction** is the other mode and a different idea. Instead of deleting old records, a background
process retains **the most recent record per key** and discards earlier ones. The log stops being a
history and becomes a **snapshot with a history in front of it**: everything before the compaction point
is the latest state of every key that ever existed, and everything after is the recent change stream.

That duality is what makes several things work. A consumer can bootstrap complete state by reading the
compacted topic from the beginning and then continue live with no separate snapshot mechanism — which is
exactly how `__consumer_offsets` works, and how a stream processor restores its state store after a crash.
Deletion is a **tombstone**: a record with a null value, retained for `delete.retention.ms` so that every
consumer has a chance to observe it before it too is compacted away. A consumer offline longer than that
window will never learn the key was deleted, which is the same tombstone-lifetime hazard as Chapter 94's
anti-entropy.

The cost is that compaction is a background rewrite: read segments, keep the surviving records, write new
ones. That is write amplification and I/O contention with the live path, and it is the same mechanism, and
the same trade, as LSM compaction in Chapter 01 §1.

### 7.7 Exactly-once, as a mechanism

Chapter 03 §3 argues exactly-once delivery does not exist across arbitrary boundaries. What exists is a
pair of mechanisms that eliminate two specific duplicate sources.

**The idempotent producer** eliminates duplicates from *producer retries*. Each producer gets a producer
ID, and each batch carries a per-partition monotonic sequence number. The broker remembers the last few
sequence numbers per producer per partition and silently drops a batch it has already appended. This
converts "the ack was lost so I resent it" from a duplicate into a no-op. It is nearly free, it also
preserves ordering with five requests in flight (§7.4), and it should simply be on.

**Transactions** eliminate duplicates from *consume-transform-produce* loops. A transaction coordinator
tracks the transaction in an internal log; the producer writes records to several partitions, writes its
consumed offsets **into the same transaction**, and commits. The coordinator writes commit or abort
markers into every affected partition. Consumers configured `read_committed` filter aborted records using
an index of aborted transactions the broker maintains alongside the log.

The guarantee is precise: **within the Kafka cluster**, a consume-transform-produce step happens exactly
once, because the output and the input offsets commit or roll back together. The moment the transform
calls an external API, that call is outside the transaction and the guarantee is gone (Chapter 03 §3).

Two costs. Latency: markers and coordinator round trips add milliseconds, and throughput falls a few
percent with reasonable batch sizes and much more with tiny transactions, because the per-transaction
overhead is fixed. And the failure mode people hit: `read_committed` consumers can only read up to the
**last stable offset**, the point before the earliest open transaction. **A producer that opens a
transaction and stalls blocks every `read_committed` consumer on those partitions** — not slows, blocks —
until it commits, aborts, or the transaction timeout fires. Consumption stops with no error, no lag on the
producer side, and no obvious cause.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Broker loss | Its leaderships move; brief unavailability per partition | ISR election; producer retries + idempotence turn it into a latency spike |
| ISR shrinks below `min.insync.replicas` | Produce fails for that partition | Deliberate: durability over availability. Alert on ISR size, not just on broker count |
| Every replica down, unclean election enabled | Acknowledged records vanish; consumers see truncation | Leave unclean election off except where availability genuinely outranks the data |
| Consumer group thrashing | Group-wide stalls, lag climbing with no broker fault | Static membership, cooperative rebalancing, sane `max.poll.interval.ms` (§7.5) |
| One lagging consumer | Cold reads evict the hot tail; unrelated consumers slow down | Alert on per-group lag; isolate bulk readers onto their own brokers or replicas |
| Hot partition from a skewed key | One consumer saturated while others idle | Re-key, split with a suffix, or move the key to its own topic (§7.4) |
| Disk full | The broker stops accepting writes and often cannot recover cleanly | Alert on days-of-retention remaining, not percent used — the useful unit is time |
| Open transaction stalls | `read_committed` consumption blocks with no error | Alert on last-stable-offset lag; keep transaction timeouts short (§7.7) |

**Monitoring:** consumer lag per group per partition, which is the single most informative metric and the
leading indicator for nearly every incident; under-replicated and offline partition counts; ISR shrink and
expand rate, which detects a struggling broker before it fails; produce p99 by `acks` setting; and
page-cache hit ratio, since the moment reads start touching disk the performance model has changed.

---

## 9. Common mistakes

1. **Calling it a queue and expecting queue semantics.** Per-message ack, priority, and per-message delay
   are structurally absent because consumers own offsets. Chapter 03 §2 is where that choice is made.
2. **Claiming global ordering.** Ordering is per-partition, and a design that needs total order over a
   topic has just capped itself at one writer.
3. **Choosing a partition key without saying what it orders**, then being surprised when two events for
   the same entity are processed out of order.
4. **Under-provisioning partitions.** Adding them later breaks key affinity permanently and caps consumer
   parallelism in the meantime.
5. **Ignoring rebalance behavior**, then losing minutes of consumption to a routine deploy and blaming
   the broker.
6. **Assuming exactly-once covers external side effects.** Kafka transactions are Kafka-internal;
   the external call needs an idempotency key (Chapter 03 §4).
7. **Committing offsets before processing**, which is at-most-once dressed as an optimization and loses
   records on every crash.
8. **Sizing the cluster from throughput.** Retention times replication is what sizes it — ninety brokers
   against six, in §3 — and it is the arithmetic that gets skipped.
9. **Treating a lagging consumer as that consumer's problem.** Its cold reads evict the page cache
   everyone else depends on.

---

## 10. Variants

**Pulsar** separates serving from storage: brokers are stateless and segments live in BookKeeper. Scaling
storage no longer means moving partitions, and adding a broker is instant. It also supports queue-style
shared subscriptions with per-message acknowledgment, so it can be both models — at the cost of a second
distributed system to operate.

**Kinesis and other managed logs.** The same model with shards instead of partitions, retention up to a
year, and capacity expressed in provisioned shards. The design questions are identical; the operational
ones disappear and the cost ones appear.

**Redis Streams.** The log data structure inside a cache, with consumer groups and per-message
acknowledgment. Excellent for modest volumes where a broker cluster is not justified; bounded by memory,
and durability is Redis's, which is to say best-effort unless configured otherwise.

**Tiered storage.** Offload closed segments to object storage and keep only the recent tail on local disk.
This directly attacks §3's constraining number — retention no longer multiplies local disk — at the cost of
much slower historical reads. It is the most consequential recent change to the model in this chapter.

**Event sourcing on the log.** Treating the log as the system of record and materializing state by
replaying it. Compaction (§7.6) is what makes it viable. The limits are real: no ad-hoc query, no
cross-entity transaction, and a schema you can never truly retire because old records are still there.

---

## 11. Further reading

- Chapter 03 §2–§3 for the queue-versus-log decision and delivery semantics, Chapter 01 §1 for the LSM
  compaction analogy
- Jay Kreps, "The Log: What every software engineer should know about real-time data's unifying
  abstraction" — the clearest statement of why the log is the primitive
- Kreps, Narkhede, Rao, "Kafka: a Distributed Messaging System for Log Processing" (NetDB 2011)
- KIP-98 (idempotent producer and transactions) and KIP-429 (incremental cooperative rebalancing), both of
  which document the problem before the solution and are unusually readable
- The Apache Kafka documentation's "Design" section, especially the parts on persistence and efficiency
