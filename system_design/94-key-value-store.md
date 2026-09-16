# Chapter 94 — Distributed Key-Value Store

> **Prerequisites:** Chapters 01 (§1 LSM-trees, §4 partitioning, §5 replication, §6 consistency models), 04 (§6 consistent hashing)
> **Patterns:** consistent hashing, quorums, anti-entropy, gossip membership, tunable consistency

---

## 1. The problem

Store values by key, across many machines, such that the system survives node and network failure and
scales by adding hardware.

This is the chapter where the foundations assemble into a single system. Consistent hashing, replication,
quorums, LSM storage, and eventual consistency have each appeared as a component of some other design;
here they are the design. That makes it a useful capstone and a slightly unusual interview question — it
is asked less often than the product-shaped chapters, and when it is asked, the interviewer is testing
whether you understand the mechanisms rather than whether you can decompose a product.

The reference design is Amazon's Dynamo, and its defining commitment is worth stating up front because
every subsequent decision follows from it: **it is always writeable.** A write must succeed even during a
network partition, even when most replicas are unreachable. From Chapter 01, §7, choosing availability
during a partition means giving up consistency, and Dynamo gives it up thoroughly — accepting that
conflicting versions of a value will exist and pushing their resolution to the application.

**The property that makes it hard:** every mechanism in the system exists to compensate for that choice.
Once you accept divergent replicas, you need a way to detect divergence (version vectors), a way to repair
it cheaply in the background (Merkle trees), a way to keep writing when the right node is down (sloppy
quorums and hinted handoff), and a way to know who is alive without a coordinator (gossip). None of these
would be needed if you were willing to be unavailable, and understanding that the complexity is *bought*
rather than inherent is the most useful thing to take from the chapter.

---

## 2. Requirements

### Functional

1. `put(key, value)` and `get(key)`.
2. `delete(key)`.
3. Nothing else. No queries, no secondary indexes, no transactions across keys. §7.7 argues that this
   austerity is the point rather than a limitation.

### Non-functional

- **Availability** — writes succeed during node failure and network partition. This is the primary
  requirement and it determines the rest.
- **Latency** — p99 under 10 ms for both operations. Dynamo's original target was expressed as a p99.9
  SLA, which is worth mentioning: at the tail, a service composed of many such calls is defined by its
  worst percentile, not its median.
- **Scale** — linear. Adding a node adds capacity and throughput proportionally, with bounded data
  movement.
- **Consistency** — eventual, with **tunable** per-operation guarantees. Different callers of the same
  store need different things, and forcing one setting on all of them is the failure of the alternatives.
- **Durability** — an acknowledged write survives node loss, subject to the replication factor.
- **Partition tolerance** — non-negotiable; partitions happen and are not a configuration option.

### Explicitly out of scope

Range scans, joins, secondary indexes, and multi-key transactions. Name them explicitly — a candidate who
scopes these out deliberately is demonstrating that they know what the data model costs, whereas one who
never mentions them may simply not have noticed.

---

## 3. Estimation

Assume 100 TB of data, 1 million operations/second, average value 1 KB.

**Node count**

```
100 TB × replication factor 3 = 300 TB stored
per node: 4 TB usable                     → 75 nodes
```

**Throughput per node**

```
1e6 ops/sec × 3 (each write hits 3 replicas) = 3e6 replica-ops/sec
3e6 / 75 nodes                               = 40,000 ops/sec/node
```

Forty thousand operations per second per node, which an LSM-backed store handles comfortably for writes
and, with a Bloom filter per SSTable, adequately for reads.

**The replication multiplier is the number to notice.** Every client write is three physical writes.
Capacity planning, network bandwidth, and cost are all 3× the naive figure, and it is the first thing to
say when someone proposes raising the replication factor to five for durability — it is a 67% cost
increase, not a free improvement.

**Virtual nodes**

```
75 physical nodes × 200 virtual nodes each = 15,000 ring positions
```

§7.1 explains why 200 rather than 1; the number is chosen so that the standard deviation of load across
physical nodes is a few percent rather than tens of percent.

**Data movement when adding a node**

```
with consistent hashing:  100 TB / 76 ≈ 1.3 TB moved
with hash(key) mod N:     ~99 TB moved  (nearly everything remaps)
```

**A factor of 75.** This single comparison is the entire argument for consistent hashing, and producing
the arithmetic rather than asserting the property is what makes it convincing.

**Merkle tree comparison cost**

```
4 TB per node ÷ 1 MB leaf granularity = 4e6 leaves
tree depth ≈ log2(4e6) ≈ 22
divergent range located in ~22 hash comparisons rather than 4e6
```

§7.4. The point of the structure is that finding *what* differs costs logarithmically rather than
linearly, so anti-entropy between two synchronized replicas is nearly free.

---

## 4. API

```
put(key, value, context?)     -> ack           # context: the version(s) this write supersedes
get(key)                      -> [(value, context)...]   # note: a LIST
delete(key, context)          -> ack

// Per-operation tuning
put(key, value, W=2)
get(key, R=2)
```

Three decisions:

**`get` returns a list of values, not a value.** This is the API telling the truth about the consistency
model. When replicas have diverged and the versions are concurrent rather than ordered, the store cannot
know which is correct — so it returns all of them and the application decides. An API that hides this by
picking one silently is choosing last-write-wins on the caller's behalf without saying so, and §7.3
explains why that loses data.

Most callers will see a single-element list almost always. The point is that the type forces them to
handle the other case at compile time rather than discovering it in production.

**The opaque `context` must be passed back on write.** It carries the version vector (§7.3). A write that
supplies the context read from a prior `get` is declaring "I am superseding these versions", which lets
the store recognize the write as a descendant rather than a sibling. A write with no context is a blind
write and always creates a new concurrent version.

**`R` and `W` are per-operation, not per-cluster.** A session token can be written with `W=1` and read
with `R=1`; a user's account settings might use `W=2, R=2`. Forcing a single setting means either
over-paying for the cheap data or under-protecting the important data. §7.2.

---

## 5. Data model

```
The ring
  hash space [0, 2^128), each node claims many positions (virtual nodes)
  key k is stored on the N nodes found by walking clockwise from hash(k),
  skipping positions belonging to a physical node already in the set

Per node, per key range
  LSM storage:  memtable + immutable SSTables + Bloom filter per SSTable
  key -> [ (value, version_vector, timestamp), ... ]   -- siblings, if diverged

Membership state (gossiped)
  node -> { status, ring positions, heartbeat counter, incarnation }

Merkle trees
  one per key range per node, over the range's contents
```

Three deliberate decisions:

**Preference list construction skips duplicate physical nodes.** Walking clockwise from a key naturally
lands on the next N *ring positions*, but with 200 virtual nodes per physical node, several of those
positions can belong to the same machine — which would make the "three replicas" one machine and defeat
the replication entirely. The walk must skip until it has N distinct physical nodes, and in a
multi-datacenter deployment, until it has spread across failure domains. This is a small implementation
detail with a large correctness consequence, and it is easy to omit.

**A key can hold multiple sibling values.** The storage layer does not resolve conflicts; it retains them.
This is unusual and it is the direct consequence of the always-writeable requirement.

**LSM storage underneath** (Chapter 01, §1), because the workload is write-heavy by construction: every
client write becomes N replica writes, plus repair writes from anti-entropy. A per-SSTable Bloom filter
keeps read amplification manageable.

---

## 6. Architecture, derived

### Attempt 1: shard with `hash(key) mod N`

Assign keys to nodes by modulo.

Even distribution, trivial to implement, and it fails on the requirement to scale: changing `N` from 75 to
76 changes the destination of essentially every key. From §3, that is 99 TB of movement to add 1.3 TB of
capacity. The cluster is unusable during the rebalance.

### Attempt 2: consistent hashing

Place nodes and keys on a ring; a key belongs to the next node clockwise. Adding a node claims one arc and
moves only the keys in it — 1.3 TB rather than 99 TB.

Two problems remain with the naive form. **Load is uneven**: with 75 randomly placed points, arc lengths
vary substantially, so some nodes hold several times the average. And **removing a node dumps its entire
share on its single successor**, creating a cascading overload exactly when the cluster is already
degraded.

**Virtual nodes fix both.** Each physical node claims ~200 positions. Arc lengths average out, so load
variance drops to a few percent. When a node leaves, its 200 arcs are inherited by 200 different
successors, spreading the load rather than concentrating it. Virtual nodes also allow heterogeneous
hardware — a machine with twice the capacity claims twice the positions.

### Attempt 3: replicate to N nodes

Store each key on the next N distinct physical nodes clockwise — the **preference list**. Survives N−1
failures.

Now the question is how many replicas must acknowledge, which is the whole consistency design.

### Attempt 4: quorums

Require `W` acknowledgments to write and `R` responses to read.

```
R + W > N  ⟹  the read set and the write set must intersect
           ⟹  at least one responding replica has the latest write
```

With `N=3`, the configurations are:

| W | R | Property | Use |
|---|---|---|---|
| 3 | 1 | Fast reads, write unavailable if any replica is down | Read-heavy, rarely written |
| 1 | 3 | Fast writes, slow reads | Write-heavy logging |
| 2 | 2 | Balanced, tolerates one failure on each path | **The default** |
| 1 | 1 | Fastest, `R+W ≤ N`, no intersection guarantee | Caches, session data |

`N=3, W=2, R=2` is the canonical setting because it satisfies the inequality while tolerating a single
node failure on both paths.

**The intersection guarantee is weaker than it sounds**, and this is the point most candidates miss.
`R + W > N` guarantees the read set *contains* a replica with the latest write. It does not guarantee the
read *returns* it unless the client can tell which of the returned values is latest — which requires
versioning (§7.3) — and it says nothing about writes that were partially applied. It is a necessary
condition for freshness, not linearizability.

### Attempt 5: stay writeable when the preference list is unreachable

`W=2` fails if two of the three replicas are unreachable. The availability requirement says the write must
succeed anyway.

**Sloppy quorum**: if a preference-list node is down, write to the next healthy node on the ring instead,
tagging the data with a **hint** naming its intended home. The write gets its `W` acknowledgments from
whichever N healthy nodes were found, not necessarily the "right" ones.

**Hinted handoff**: the temporary holder periodically retries delivery to the intended node, and deletes
its copy once accepted.

This is what "always writeable" actually costs. A sloppy quorum's acknowledgments may come from nodes that
are not in the preference list at all, so `R + W > N` no longer guarantees intersection — a subsequent
read of the proper replicas can miss the write entirely until handoff completes. **Availability was bought
by weakening the guarantee**, and being able to state precisely which guarantee was weakened is what
separates understanding the mechanism from reciting it.

### Attempt 6: converge in the background

Replicas diverge from partitions, dropped hints, and failed writes. Two repair mechanisms:

**Read repair** — when a read returns differing versions, the coordinator writes the resolved value back
to the stale replicas. Cheap, opportunistic, and it only fixes keys that are actually read. A key written
once and read once a year stays divergent.

**Anti-entropy with Merkle trees** — replicas periodically compare tree roots; identical roots mean
identical data and the comparison ends in one hash. Differing roots are walked down to locate the
divergent ranges (§7.4).

### Final architecture

```
  client
    │  put(k,v) / get(k)
    ▼
  coordinator (any node — the ring is known to all)
    │  preference list = walk ring from hash(k), N distinct physical nodes
    │
    ├─► replica A ──┐
    ├─► replica B ──┼─► wait for W acks (or R responses)
    └─► replica C ──┘   substituting healthy nodes + hints if any are down
                            │
                            ▼
  each node:  LSM store (memtable → SSTables, Bloom filters)
              Merkle trees per key range
              hint store for handoff
              gossip: membership, heartbeats, ring state

  background:  hinted handoff retry  ·  read repair  ·  Merkle anti-entropy
```

Every node is identical — any node can coordinate any request. There is no master, no configuration
service, and no metadata tier, which is why there is no single point of failure and also why membership
must be gossiped rather than looked up.

---

## 7. Deep dives

### 7.1 Consistent hashing and virtual nodes

Covered in Chapter 04, §6; three points specific to this system.

**Virtual node count is a real trade-off, not a formality.** More positions mean smoother load and more
membership state to gossip, more Merkle trees to maintain (one per key range per node), and a longer ring
walk to find N *distinct* physical nodes. A few hundred per node is the usual range; the load variance
falls roughly as the inverse square root of the count, so the benefit saturates while the costs stay
linear.

**Skipping duplicate physical nodes matters, and is easy to get wrong** (§5). So is topology awareness: in
a multi-datacenter deployment the preference list should span racks and datacenters, or a single rack
failure takes all three replicas. Encoding topology into the ring walk is what makes replication actually
fault-tolerant rather than nominally so.

**Adding a node is not instantaneous.** The new node claims its positions, then streams data from the
predecessors of each arc while they continue serving. Reads must go to the old owner until the transfer
completes, so ownership changes at the end of the stream, not the beginning. During a large rebalance the
cluster is doing double duty, which is why capacity should be added before it is needed rather than in
response to saturation.

### 7.2 Tunable consistency, and what R+W>N does not give you

The per-operation choice of `R` and `W` is the system's most useful feature and its most commonly
misunderstood one.

**What `R + W > N` gives you:** the read set intersects the write set, so a replica holding the most recent
successful write is among those that respond.

**What it does not give you, and this list is the substance:**

- **Not linearizability.** With `N=3, W=2, R=2`, a write acknowledged by two replicas and a concurrent read
  from two replicas can return the old value if the read's quorum happens to complete first. Two clients
  reading concurrently can also observe different values. The guarantee is about the *presence* of the
  latest value in the read set, not about a global ordering of operations.
- **Nothing during a sloppy quorum.** As §6, attempt 5 established, if acknowledgments came from
  substitute nodes, the intersection property is void until hints are delivered.
- **No protection against partial writes.** A write that reaches one replica and then fails to reach `W`
  is not rolled back. That replica now holds a version nobody acknowledged, which will propagate through
  anti-entropy. A failed write may still eventually become the stored value.

That last point is genuinely counterintuitive and worth stating: **a `put` that returned an error may
still have taken effect.** The client's only correct response is to retry idempotently.

**Choosing R and W in practice:** start from what the data is. Session tokens and caches tolerate `W=1,
R=1` and should use it, because the latency saving is real and the cost of a stale read is nothing. User
data that is read after being written wants `W=2, R=2` so that read-your-writes usually holds. Data that
must not be lost wants a higher `W`, accepting reduced write availability — and that trade should be made
consciously, because raising `W` to 3 with `N=3` means any single node failure makes the key unwritable.

### 7.3 Conflict resolution: last-write-wins versus version vectors

Two clients write the same key concurrently, at replicas that cannot see each other. Both writes succeed.
Two values now exist and neither is derived from the other. Something must decide.

**Last-write-wins.** Attach a wall-clock timestamp; the higher one survives.

Simple, requires no client cooperation, and **it silently discards data**. It also depends on clock
synchronization, so a node with a fast clock wins every conflict it participates in — a subtle, persistent,
and nearly undiagnosable form of data loss. Cassandra defaults to this and it is a frequent source of
production surprise.

LWW is correct for data that is genuinely a single value with no merge semantics — a cached rendering, a
last-seen timestamp — where losing one of two concurrent writes is what you would have chosen anyway. It
is wrong for anything accumulative.

**Version vectors.** Each key carries a map of `node → counter`. A write increments the coordinating
node's counter. Comparing two vectors gives one of three answers:

```
A = {n1: 2, n2: 1}    B = {n1: 3, n2: 1}    → B descends from A; B wins, no conflict
A = {n1: 2, n2: 1}    B = {n1: 1, n2: 2}    → concurrent; both retained as siblings
```

Concurrency is *detected* rather than resolved, and both values are returned to the client (§4). The
application resolves them, because only the application knows the semantics.

**The canonical example is a shopping cart**, and it is canonical because it makes the argument
unanswerable. Two concurrent writes: one adds a book, one adds a pen. LWW keeps one item and loses the
other, and the customer never learns. Version-vector resolution returns both siblings and the application
unions them — the cart contains both items, which is what the user did.

The failure mode of the union rule is equally instructive: a removed item can reappear, because "remove"
is the absence of an element and a union restores it. Fixing that requires tombstones, at which point you
have derived a CRDT — an OR-Set — from first principles. Chapter A0 discusses the family; here the point
is that **choosing a merge function is a data modeling decision, and the store's job is to preserve enough
information for the application to make it.**

**Vector growth** is the practical objection: with many coordinating nodes, the vector grows. Bound it by
truncating the oldest entries, accepting occasional false-concurrency (two values reported as siblings
when one actually descends from the other), which is the safe direction to err.

### 7.4 Anti-entropy with Merkle trees

Read repair only fixes keys that are read. Cold keys need a background mechanism, and comparing two
replicas' full contents is 4 TB of transfer per comparison.

A **Merkle tree** over a key range hashes leaves (a block of keys) and combines them upward. Two replicas
compare roots: identical means identical data, in one hash exchange. Differing roots mean walking down to
find which subtrees disagree, exchanging `O(log n)` hashes to locate the divergent leaves — 22 comparisons
for the 4 million leaves in §3, rather than 4 million.

**The cost is maintenance.** Every write invalidates the path from its leaf to the root, so the tree must
be updated or recomputed. Recomputation is expensive; incremental update makes writes more costly. The
usual compromise is per-range trees rebuilt on a schedule, accepting that the tree is slightly stale — an
acceptable trade because the process is a background repair, not a correctness gate.

**Leaf granularity is the knob.** Fine leaves locate divergence precisely and make the tree large; coarse
leaves make the tree small and force transferring more data than necessary once divergence is found.

**Range boundaries must align with the ring**, or a ring change invalidates every tree. This is a real
operational cost of adding nodes and is worth naming: rebalancing does not just move data, it invalidates
the structure that keeps data converged, so anti-entropy is degraded for a period after every topology
change.

### 7.5 Gossip membership and failure detection

With no coordinator, every node must learn independently which nodes exist and which are alive.

**Gossip**: each node periodically picks a random peer and exchanges membership state — node list, ring
positions, heartbeat counters. Information propagates in `O(log N)` rounds, requires no central authority,
and degrades gracefully because there is nothing to lose.

**Failure detection is the hard part**, and the naive version is bad. A fixed timeout must be tuned
against the worst plausible pause, so it is either slow to detect real failures or prone to declaring
healthy nodes dead during a GC pause or a network hiccup. Both errors are expensive: a false positive
triggers unnecessary data movement, and repeated false positives cause flapping that generates more load
than the failure would have.

**The φ accrual failure detector** replaces the boolean with a continuously-updated suspicion level
derived from the observed distribution of heartbeat inter-arrival times. Rather than "is it dead?", it
answers "how surprising is this silence, given how this node normally behaves?" The threshold becomes a
tunable confidence rather than a timeout, and it adapts automatically to a link that is simply slow. This
is the right answer and it is a good thing to know by name.

**Permanent removal must be a human decision.** Gossip marks a node as suspected or down; actually removing
it from the ring triggers rebalancing of terabytes. That is an operator action, not an automatic response
to a failure detector, because the cost of being wrong is enormous and the cost of waiting is small.

### 7.6 Deletes and tombstones

Deletion in an eventually-consistent replicated store is harder than it looks, and it is the mechanism
most often omitted.

Deleting a key locally does not work: a replica that missed the delete still has the value, and
anti-entropy will helpfully restore it. **The deletion must itself be a versioned write** — a
**tombstone** — that propagates like any other value and wins against older versions by the normal
comparison.

Tombstones must be retained long enough to reach every replica, then removed to reclaim space. The
retention window (`gc_grace`) is the crux:

- **Too short**, and a replica that was down longer than the window comes back holding the original value,
  finds no tombstone to contradict it, and resurrects deleted data. This is a real and infamous failure
  mode.
- **Too long**, and tombstones accumulate. A workload that writes and deletes heavily can end up with far
  more tombstones than live data, and reads must scan past them all — a queue implemented on such a store
  degrades until reads time out, which is why "do not build a queue on Cassandra" is standard advice.

**The operational rule that follows: a node down longer than the grace period must not simply rejoin.** It
must be wiped and rebuilt from its peers, or it will resurrect every key deleted during its absence.
Naming this rule demonstrates familiarity with operating one of these systems rather than reading about
one.

### 7.7 When this is the wrong system

The most valuable thing to say about the Dynamo model is when not to use it, because it is over-applied.

**A single relational instance handles more than people assume.** Tens of thousands of writes per second
and terabytes of data, with transactions, secondary indexes, joins, and strong consistency included. The
`N=3` replication multiplier from §3 means the distributed store costs 3× the storage before it does
anything else.

**Choose the Dynamo model when** the data is genuinely key-addressed with no cross-key invariants, when
write availability during a partition is a hard product requirement, when the volume genuinely exceeds a
single node, and when the application can define a merge function for concurrent writes.

**Choose something else when** any of those is false. In particular, if the application cannot articulate
what should happen when two concurrent writes conflict, it is not ready for a store that will ask.

**And note the modern middle ground.** Since Dynamo's publication, systems have appeared that provide
strong consistency at scale by paying a coordination cost — Spanner with synchronized clocks, CockroachDB
and similar with Raft per range. They are slower per operation and unavailable during certain partitions,
and for a great many workloads that is a better trade than resolving sibling values in application code.
The Dynamo design is a landmark, not a default, and treating it as the automatic answer to "distributed
storage" is a dated instinct.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Single node down | Writes go to substitutes with hints; reads may see stale data | Sloppy quorum and hinted handoff; alarm on hint backlog, which is the direct measure of degradation |
| Node down beyond gc_grace, then rejoins | **Deleted data resurrects** | Never rejoin a long-absent node; wipe and rebuild from peers |
| Network partition | Both sides accept writes; siblings created | Version vectors detect them; application merges on read |
| Hint backlog unbounded | Substitute node fills its disk with data it does not own | Cap hint storage and shed; alarm well before the cap |
| Tombstone accumulation | Read latency grows until timeouts | Monitor tombstone-to-live ratio per range; avoid delete-heavy workloads on this store |
| Clock skew with LWW | Silent, persistent data loss favoring fast-clocked nodes | Use version vectors; if LWW is required, treat NTP as a correctness dependency |
| Hot key | One preference list saturated while the cluster is idle | Consistent hashing distributes *keys*, not *popularity* — needs a cache in front or key splitting |
| Ring change during rebalance | Merkle trees invalidated, anti-entropy degraded | Expect a repair-degraded window after every topology change; do not stack changes |

**Monitoring:** p99 and p99.9 latency for `get` and `put` separately (the tail is the SLA); hint backlog
size and age; anti-entropy repair lag per range; sibling rate on reads, which is the direct measure of how
much divergence the cluster is actually experiencing; tombstone ratio; load variance across physical nodes,
which validates that virtual nodes are doing their job; and gossip convergence time.

---

## 9. Common mistakes

1. **Using `hash(key) mod N`** and not being able to quantify why it fails — the 99 TB versus 1.3 TB
   comparison is the argument.
2. **Consistent hashing without virtual nodes**, leaving uneven load and a successor that inherits an
   entire failed node's share.
3. **Not skipping duplicate physical nodes** in the preference-list walk, so three "replicas" can be one
   machine.
4. **Claiming `R + W > N` gives strong consistency.** It gives set intersection, which is weaker, and it
   gives nothing at all under a sloppy quorum.
5. **Defaulting to last-write-wins** without acknowledging that it discards data and depends on clock
   synchronization.
6. **Returning a single value from `get`** and hiding the conflict, which is choosing LWW silently.
7. **Forgetting tombstones**, so deletes resurrect — and forgetting the operational rule about long-absent
   nodes that follows from them.
8. **Ignoring that a failed write may still have taken effect** on some replicas.
9. **Recommending this model reflexively** for workloads a single relational node would serve better, with
   transactions, at a third of the storage cost.

---

## 10. Variants

**Cassandra.** Dynamo's partitioning, replication, and gossip, with a wide-column data model and a query
language on top, and last-write-wins at cell granularity instead of version vectors. That last substitution
is the significant one: it trades the shopping-cart correctness of §7.3 for an API that never returns
siblings, which is more convenient and loses data under concurrent writes.

**DynamoDB.** Shares the name and little of the architecture. It is a managed service with strong
consistency available per read, no sibling values, and no exposed ring — partitioning and replication are
hidden. Do not conflate it with the paper in an interview; the distinction is a small credibility test.

**Riak.** The closest faithful implementation of the paper, including version vectors and sibling values
exposed to the application, plus CRDT data types that supply the merge function so the application does not
have to.

**Redis Cluster.** Different point on the spectrum: in-memory, hash slots rather than a hash ring,
asynchronous primary-replica replication rather than quorums. Faster and less durable, and it does not
attempt the always-writeable property.

**Spanner, CockroachDB, TiDB.** The opposite trade: strong consistency and distributed transactions,
achieved with Raft per range and, in Spanner's case, synchronized clocks with bounded uncertainty. Higher
per-operation latency, unavailable during certain partitions, and no sibling resolution for the application
to implement. For most applications this is now the better default, and §7.7's point is that saying so is
part of a complete answer.

---

## 11. Further reading

- Chapters 01 (§1, §4, §5, §6) and 04 (§6) — this chapter is largely their composition
- DeCandia et al., "Dynamo: Amazon's Highly Available Key-value Store" (SOSP 2007) — the source, and short
  enough to read in an evening
- Martin Kleppmann, *Designing Data-Intensive Applications*, chapter 5, for the clearest treatment of
  version vectors and concurrent writes
- Hayashibara et al., "The φ Accrual Failure Detector" (2004), for §7.5
- Corbett et al., "Spanner: Google's Globally-Distributed Database" (OSDI 2012), for what buying back
  strong consistency costs
