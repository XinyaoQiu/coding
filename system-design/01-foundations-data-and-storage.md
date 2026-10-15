# Chapter 01 — Data and Storage

Every system in Part II is, underneath, a decision about where bytes live and who is allowed to be wrong
about them for how long. This chapter builds that vocabulary. It is the longest foundation chapter
because storage decisions are the ones that are expensive to reverse.

---

## 1. Storage engines: B-trees and LSM-trees

Almost every database you will name in an interview is built on one of two on-disk structures. Knowing
which, and why, lets you answer "why Cassandra here and Postgres there" without hand-waving.

### B-tree

A balanced tree of fixed-size pages (typically 4–16 KB). A lookup walks from the root to a leaf,
costing `O(log_B n)` page reads — in practice three or four disk reads for a large table, and fewer
because upper levels stay cached.

Writes update pages **in place**. To make this crash-safe, the engine first appends the intended change
to a write-ahead log, then applies it. A single logical write therefore touches the log and, eventually,
one or more randomly located pages.

- **Good at:** point reads, range scans, predictable read latency, transactions.
- **Costly at:** write throughput. Random page writes and the write-ahead log mean a write is expensive
  relative to an append.
- **Used by:** PostgreSQL, MySQL/InnoDB, most relational systems, and the metadata layer of nearly
  everything else.

### LSM-tree (Log-Structured Merge tree)

Writes go to an in-memory sorted structure (the memtable) plus a commit log. When the memtable fills, it
is flushed to disk as an immutable sorted file (an SSTable). Background **compaction** merges SSTables,
discarding overwritten and deleted keys.

- **Good at:** write throughput. Every write is a memory operation plus a sequential log append; the
  expensive merging is amortized and happens off the critical path.
- **Costly at:** reads, which may have to consult several SSTables (mitigated with Bloom filters per
  file), and latency predictability, because compaction competes for I/O.
- **Used by:** Cassandra, RocksDB, LevelDB, HBase, ScyllaDB, and the storage layer of many newer systems.

### The one-sentence version

**A write-heavy system wants an LSM-tree; a read-and-transaction-heavy system wants a B-tree.** When a
chapter in Part II picks Cassandra for a message store or a timeline, this is why: the workload is
append-dominated, the access pattern is a known key, and no one needs a join.

### Read, write, and space amplification

The three costs any storage engine trades against each other:

- **Read amplification** — physical reads per logical read. LSM: high (multiple SSTables). B-tree: low.
- **Write amplification** — bytes written to disk per byte of data. LSM: high over time, because
  compaction rewrites data repeatedly. B-tree: high per write, because of page granularity and the log.
- **Space amplification** — disk used per byte of live data. LSM before compaction: high (stale copies).

You cannot minimize all three. Naming this trade-off explicitly is a strong signal in a storage deep dive.

---

## 2. Choosing a data store

Do not begin with the store. Begin with the access pattern, then pick the store that serves it.

Ask, in this order:

1. What is the dominant **read** query? Write it out as a sentence.
2. What is the dominant **write**? At what rate?
3. Does anything need to be **transactional across rows**?
4. What is the total **volume**, and does it fit on one machine?
5. What **consistency** does each path need?

Only then:

| Access pattern | Store | Why |
|---|---|---|
| Varied queries, joins, transactions, secondary indexes, moderate scale | **Relational** (PostgreSQL, MySQL) | The default. A single Postgres node handles far more than candidates assume — tens of thousands of QPS and terabytes. Reach for something else when you can say what specifically breaks. |
| Known key, very high write rate, no joins | **Wide-column** (Cassandra, DynamoDB, HBase) | LSM storage, horizontal by design. You must know the query before you design the table — the partition key *is* the schema. |
| Flexible or evolving document shape, key access | **Document** (MongoDB) | Convenient; the flexibility is a liability at scale because nothing enforces the shape. |
| Full-text, relevance ranking, faceting | **Search engine** (Elasticsearch, OpenSearch) | An index, not a source of truth. Populate it from the authoritative store; be able to rebuild it. |
| Large immutable objects | **Object storage** (S3, GCS) | Cheap, durable, effectively unbounded. Clients should read and write it directly, never through your servers. |
| Time-stamped events, analytical aggregation | **OLAP / columnar** (ClickHouse, Druid, Pinot, BigQuery) | Columnar layout, aggressive compression, scans over billions of rows. Terrible for point updates. |
| Hot small values, counters, sorted structures, ephemeral state | **Redis** | In-memory; treat durability as best-effort unless you have configured otherwise and said so. |
| Traversal over relationships of unbounded depth | **Graph** (Neo4j) | Genuinely useful for multi-hop traversal. For one-hop ("who do I follow"), an ordinary indexed table is better — say this, because reaching for a graph database on a follow-graph question is a common over-reach. |

### The most common mistake

Choosing a distributed store for a workload a single node handles comfortably. If the honest answer is
"one Postgres instance with a read replica," say that, and add: "I'd revisit this at roughly X writes
per second, which is where this becomes a problem." That sentence demonstrates both judgment and the
ability to size a system, which is worth more than naming a fashionable database.

---

## 3. Indexing

An index is a redundant, ordered copy of some columns that makes a query cheap and every write more
expensive. That trade — **reads faster, writes slower, storage larger** — is the whole subject.

- **Primary / clustered index:** determines physical row order. One per table. Range scans on it are
  nearly free.
- **Secondary index:** a separate structure mapping a column value to a row locator. Each one adds write
  cost.
- **Composite index:** ordered on `(a, b, c)`. It can serve queries filtering on `a`, on `a, b`, and on
  `a, b, c` — the **leftmost prefix rule**. It cannot serve a query filtering on `b` alone. This is worth
  knowing precisely; it comes up whenever an interviewer asks "which index would you add?"
- **Covering index:** contains every column the query needs, so the base row is never read.

In distributed stores, secondary indexes are more expensive than they look. A **global** secondary index
must be partitioned differently from the base table, so writes become cross-partition. A **local**
secondary index is partition-local and cheap to write but requires a scatter-gather to query. DynamoDB
exposes both, and naming the distinction is a good depth signal.

---

## 4. Partitioning (sharding)

Splitting data across nodes so that neither storage nor throughput is bounded by one machine.

### Choosing the partition key

This is the single most consequential schema decision, and the hardest to change later.

**Rule: choose from the dominant read pattern, then verify the write pattern doesn't create a hot
partition.** A key that makes reads efficient but concentrates writes on one node has moved the
bottleneck, not removed it.

For a chat system, `conversation_id` is right: reading a conversation is one partition scan, and writes
spread across conversations. Keying on `message_id` would spread writes beautifully and make reading a
conversation a scatter-gather across every node — technically sharded, practically useless.

### Strategies

**Range partitioning.** Contiguous key ranges per node. Supports range scans. Prone to hot spots when the
key correlates with time or popularity — partitioning by timestamp means every write lands on the newest
partition.

**Hash partitioning.** `hash(key) mod N`. Even distribution, no range scans. The `mod N` is the problem:
changing `N` remaps nearly every key.

**Consistent hashing.** Nodes and keys are placed on a ring; a key belongs to the next node clockwise.
Adding or removing a node moves only about `1/N` of keys instead of nearly all of them. **Virtual nodes**
— each physical node occupying many ring positions — fix the uneven distribution that a small number of
nodes would otherwise produce, and make rebalancing smoother when a node leaves. This is the standard
answer for caches and for any system where membership changes.

**Directory / lookup-based.** An explicit map from key range to node. Maximum flexibility, and a lookup
service that is now a critical dependency.

### Hot partitions

Real key distributions are power laws. One celebrity, one viral ad, one popular product will exceed the
median by orders of magnitude. Mitigations, in increasing order of effort:

1. **Cache the hot key** in front of the store. Often sufficient; a hot key is by definition cacheable.
2. **Salt the key.** Write to `key#0` … `key#9`, chosen at random; read all ten and merge. Trades read
   cost for write distribution. Standard for hot-key counters.
3. **Split the entity.** Give the hot entity its own partition or its own service.
4. **Change the access pattern.** In a feed system, this is exactly the move from push to pull for
   celebrity accounts — the hot key stops being written to at all.

### Resharding

The operation everyone forgets to plan. When asked "how do you add capacity," do not say "we'd reshard."
Say how:

1. **Dual-write** to old and new topology; the new one is not read yet.
2. **Backfill** historical data into the new topology in the background.
3. **Verify** — compare reads from both, on a sample or in full, until divergence is zero.
4. **Cut reads** over, keeping dual-writes so rollback stays available.
5. **Cut writes**, then decommission.

The reason this matters in an interview is that it is the honest cost of a bad partition key, and
knowing the procedure is what makes "choose the partition key carefully" more than a slogan.

---

## 5. Replication

Copies of the same data on multiple nodes, for availability, read throughput, and durability.

### Single-leader

All writes go to a leader, which streams a replication log to followers. Reads may go to either.

- **Synchronous replication:** the leader waits for a follower to acknowledge. No data loss on leader
  failure; higher write latency; unavailable if the follower is down.
- **Asynchronous:** the leader acknowledges immediately. Fast, and a leader crash loses recent writes.
- **Semi-synchronous:** wait for one follower, not all. The usual production compromise.

**Replication lag** is the source of a family of anomalies you should be able to name:

- *Reading your own writes* — a user posts a comment, is served by a lagging replica, and their comment
  is gone. Fix: route a user's reads to the leader for a short window after they write, or track a
  per-session log position and only use replicas that have caught up to it.
- *Monotonic reads* — successive reads served by replicas with different lag make time appear to run
  backwards. Fix: pin a session to one replica.
- *Consistent prefix reads* — causally related writes observed out of order. Fix: ensure causally related
  writes share a partition.

**Failover** is where data is lost and split-brain happens. Detecting leader failure (timeout — how long?
too short and you fail over on a GC pause), promoting a follower (which one? the most caught-up), and
ensuring the old leader does not return and accept writes (fencing) are all genuinely hard. Saying
"we'd fail over" without acknowledging this is shallow; saying "failover is where the interesting bugs
are, specifically split-brain and lost writes on an async replica" is not.

### Multi-leader

Writes accepted at several leaders, typically one per region. Buys local write latency and regional
independence; costs you write conflicts, which now must be resolved. Only worth it for multi-datacenter
deployments or offline-capable clients.

### Leaderless (quorum)

Clients write to several replicas directly and read from several. With `N` replicas, `W` write
acknowledgments and `R` read responses, **`R + W > N`** guarantees the read set intersects the write set,
so a read sees the latest write. `N=3, W=2, R=2` is the canonical configuration.

This is Dynamo's model, inherited by Cassandra and Riak. Anti-entropy processes (Merkle-tree comparison,
read repair) converge divergent replicas in the background. Conflicts are resolved by last-write-wins
(simple, loses data) or version vectors (correct, pushes the resolution to the application).

---

## 6. Consistency models

Precision here is one of the fastest ways to sound senior, because most candidates use "consistent" to
mean nothing in particular.

From strongest to weakest:

**Linearizability.** The system behaves as if there were a single copy of the data and every operation
took effect instantaneously at some point between its invocation and its response. Once a write returns,
every subsequent read anywhere returns that value or a later one. Expensive: it requires coordination,
which means a round trip, which means latency.

**Sequential consistency.** All nodes see operations in the same order, but that order need not match
real time.

**Causal consistency.** Operations that are causally related are seen in order by everyone; concurrent
operations may be seen in different orders. Strong enough for most social products — a reply never
appears before the comment it replies to — and far cheaper than linearizability.

**Read-your-writes.** A client sees its own writes. Not a global property; a per-session one. This is
what users actually notice, and it is often the real requirement behind a vague "it must be consistent."

**Monotonic reads.** A client never sees time move backwards.

**Eventual consistency.** If writes stop, replicas converge. Says nothing about when. Adequate for
timelines, view counts, and recommendation results; inadequate for anything a user is charged for.

**In an interview, specify consistency per endpoint, not per system.** A ticket-booking site is eventually
consistent when browsing seat availability and strictly serializable at the moment of reservation. That
sentence, said in step 1, is worth more than an entire correct paragraph about CAP.

---

## 7. CAP and PACELC

**CAP.** During a **network partition**, a system must choose between remaining **available** (serving
requests that may return stale data) and remaining **consistent** (refusing requests it cannot serve
correctly). It says nothing about behavior when there is no partition, and "choosing CA" is not an
option — partitions are not optional.

CAP is over-cited and under-understood. The useful form of it in an interview is the concrete one:
*"if the replica in region B cannot reach region A, does a user in B get a stale read or an error?"*
Answering that about your own design is worth more than reciting the theorem.

**PACELC** is the better framing. *If there is a Partition, trade Availability against Consistency;
Else, trade Latency against Consistency.* The second half is the half that governs your system 99.99%
of the time: strong consistency costs a coordination round trip even when everything is healthy.

---

## 8. Transactions and isolation

**ACID:** Atomicity (all or nothing), Consistency (invariants preserved — the least interesting letter,
and really the application's job), Isolation (concurrent transactions don't interfere), Durability
(committed means survives a crash).

Isolation levels, and the anomaly each one still permits:

| Level | Permits |
|---|---|
| Read uncommitted | Dirty reads |
| Read committed | Non-repeatable reads (a row changes between two reads in one transaction) |
| Repeatable read / snapshot isolation | Phantoms and write skew |
| Serializable | Nothing — equivalent to some serial order |

**Write skew** deserves a mention because it is the anomaly that bites real systems and is invisible at
snapshot isolation: two transactions each read a set of rows, each checks an invariant that currently
holds, and each writes a different row. Individually valid; together they violate the invariant. The
classic example is two doctors each going off-call after checking that at least one other doctor is on
call. Booking systems (Chapter 80) have exactly this shape.

### Concurrency control

**Pessimistic** — acquire a lock, do the work, release. Correct, serializes contention, risks deadlock.
Right when conflicts are common.

**Optimistic (OCC)** — read a version, do the work, and on commit verify the version is unchanged; if it
changed, abort and retry. No locks held during the work. Right when conflicts are rare — and dead wrong
when they are not, because the retry storm is worse than the lock would have been.

The Part II chapters on contention (50, 80, 81) all reduce to a single move: **use a cheap mechanism to
make conflicts rare, then use optimistic concurrency for correctness.** The cheap mechanism (a queue, a
short-lived hold, a lock in Redis) is an optimization; the version check or unique constraint in the
database is the guarantee. Systems that use only the first are the ones that oversell.

### Distributed transactions

**Two-phase commit** — a coordinator asks all participants to prepare, then tells them to commit. It is
correct and it blocks: if the coordinator dies after prepare, participants hold locks indefinitely. Avoid
across services.

**Sagas** — a sequence of local transactions, each with a compensating action that semantically undoes it.
No global locks, no atomicity — intermediate states are visible — but it composes across services and
survives partial failure. This is the standard answer for multi-service workflows (Chapter 81).

**The pragmatic answer** for most systems is neither: design so that the thing requiring atomicity lives
in **one** partition of **one** database, and everything else is eventually consistent with idempotent
retries. When an interviewer asks about distributed transactions, "I'd try to make it unnecessary, by
co-locating the invariant" is a better opening than a description of 2PC.

---

## 9. The outbox pattern

A recurring problem: a service must update its database *and* publish an event, atomically. Doing both
directly gives you a window where one succeeds and the other does not.

**Solution:** within the same database transaction, write the business row *and* a row to an `outbox`
table. A separate process reads the outbox and publishes to the message broker, marking rows as sent.
The database transaction makes the pair atomic; the publisher makes delivery at-least-once; consumers
are idempotent (Chapter 03).

This pattern appears in Chapters 32, 70, and 81, and knowing its name is a cheap, strong signal.

---

## 10. Checklist for a storage decision

When you choose a store in step 5 of an interview, be able to answer all six:

1. What is the primary key / partition key, and which query motivated it?
2. What breaks first as this grows — storage, write throughput, or read throughput?
3. What consistency does each access path require?
4. What is the hot-key scenario, and what happens?
5. How does this get resharded?
6. What is the backup and recovery story, and what is the recovery point objective?

If you can answer these six for every store in your design, the storage portion of the interview is over
and you won it.

---

## Further reading

- Martin Kleppmann, *Designing Data-Intensive Applications* — chapters 3, 5, 6, 7, and 9 are the source
  material for most of this chapter and are worth reading in full
- [system-design-primer — Databases](https://github.com/donnemartin/system-design-primer#database)
- Amazon's Dynamo paper (2007) — for the quorum, consistent hashing, and anti-entropy model
- Google's Spanner paper (2012) — for what it costs to buy back linearizability at global scale
