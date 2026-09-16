# Chapter 11 — Distributed Unique ID Generator

> **Prerequisites:** Chapters 01 (B-trees, partitioning), 04 (§1 gives the Snowflake layout this chapter extends)
> **Patterns:** coordination-free allocation, clock as a shared resource, information leakage

---

## 1. The problem

Many machines must hand out identifiers that are never duplicated, without asking each other permission
every time.

Stated that way it sounds trivial, and the trivial reading is the trap. Uniqueness alone is solved by a
random 128-bit number and you can stop thinking. The reason this is a real interview question is that
production identifiers are asked to do three or four other jobs at the same time, and those jobs conflict.

An identifier is usually also a **sort key** (so recent rows can be range-scanned and the ID doubles as a
pagination cursor), a **primary key** in a B-tree (so its distribution determines insert performance), a
**public token** appearing in URLs (so its predictability is a security property), and a value carried in
every row, index, log line, and network payload (so its width is a real cost at scale).

**The property that makes it hard:** uniqueness, time-sortability, unguessability, and compactness cannot
all be maximized at once, and coordination-freedom constrains all four. Every scheme in this chapter is a
different answer to which of them to give up. There is no default correct answer, only a correct answer
for a stated set of requirements — which is exactly why the interviewer asks.

---

## 2. Requirements

### Functional

1. Generate an identifier that is globally unique across all generating processes.
2. Do it locally, without a network round trip per identifier.
3. Support a stated ordering property — for most systems, roughly ascending with time.

### Non-functional

- **Throughput** — at least 10,000 IDs/second per process, with headroom for bursts. A generator that
  becomes a bottleneck has failed at its only job.
- **Latency** — sub-microsecond. This runs inside a request, often several times.
- **Availability** — must not depend on a remote service being reachable on every call. This is the
  requirement that eliminates the simplest designs.
- **Uniqueness** — absolute. Not "collisions are unlikely and we handle them"; for a primary key, a
  duplicate is data corruption, and the failure surfaces far from its cause.
- **Width** — 64 bits if it must be a compact database key; 128 bits is acceptable when storage and index
  size are not constrained.
- **Ordering** — state which one you need: none, k-sorted (approximately ordered, bounded by clock skew),
  or strictly monotonic. These have wildly different costs and candidates routinely conflate them.

### Explicitly out of scope

Content-addressed identifiers (hashes of the payload — a different problem, covered in Chapter 41),
human-memorable IDs, and cryptographic nonces. Name them so the interviewer knows the boundary is
deliberate.

---

## 3. Estimation

The arithmetic here is unusually load-bearing, because a bit layout is a fixed budget and you cannot
discover you needed more later.

**Required rate.** Assume a system doing 10,000 writes/second, each needing one ID, growing 10× over five
years:

```
peak demand          ≈ 100,000 IDs/sec system-wide
across 100 processes ≈ 1,000 IDs/sec per process
```

Modest. Any scheme handles it. The reason to compute it is to discover the opposite — that per-process
demand is *low*, which is what makes block allocation (§6, attempt 3) practical: a block of 10,000 IDs
lasts ten seconds at this rate, and a block of 1,000,000 lasts a fortnight.

**Total volume over the lifetime of the system:**

```
100,000/sec × 86,400 × 365 × 10 years ≈ 3.15 × 10^13 IDs
```

That is about 2^45. A 64-bit identifier has 19 spare bits above this, which is what makes the Snowflake
layout affordable — and confirms that a 32-bit ID (4.3 × 10^9) is not merely tight but wrong by four
orders of magnitude.

**Bit budget.** 64 bits, allocated:

```
| 1 unused | 41 timestamp (ms) | 10 machine | 12 sequence |
```

Each field's size is a decision with a consequence:

```
41 bits of milliseconds = 2^41 ms = 2.2 × 10^12 ms ≈ 69.7 years from a chosen epoch
10 bits of machine id   = 1,024 concurrent generator instances
12 bits of sequence     = 4,096 IDs per millisecond per instance = 4.096M IDs/sec/instance
```

**The constraining number is the 69.7-year lifetime**, and it is constraining in a way candidates miss:
it is 69.7 years *from your chosen epoch*, so using the Unix epoch (1970) burns 56 of those years before
the system starts and leaves about 14. Choose an epoch at the project's start date. This single decision
is the most common real bug in Snowflake implementations, and stating it is worth more than the rest of
the arithmetic.

**Re-partitioning the budget.** The three fields trade directly. Fewer machines buys more sequence or a
longer lifetime:

| Layout | Machines | IDs/ms/machine | Lifetime |
|---|---|---|---|
| 41 / 10 / 12 | 1,024 | 4,096 | 69.7 y |
| 41 / 13 / 9 | 8,192 | 512 | 69.7 y |
| 41 / 8 / 14 | 256 | 16,384 | 69.7 y |
| 39 / 10 / 14 | 1,024 | 16,384 | 17.4 y |
| 42 / 12 / 9 | 4,096 | 512 | 139 y |

Being able to produce this table on demand — and to say "I'd take 13 machine bits because we run more
than a thousand pods and I'd rather cap per-pod throughput than collide" — is what the question is
testing. The layout is not a constant to memorize; it is a budget to allocate against stated numbers.

---

## 4. API

```
// Library, in-process — the normal case
id := generator.Next()          // returns int64, never blocks in the steady state

// Service, when a library is not viable (polyglot fleet, or IDs must be
// centrally auditable)
POST /ids            {count: 1000}  -> {ids: [...]}          // batch, never single
POST /id-blocks      {size: 100000} -> {start, end, leaseId}  // block allocation
```

Three points about this shape:

**It is a library, not a service, by default.** A remote call per ID violates the latency and availability
requirements simultaneously. If you find yourself designing a request/response service that is consulted
for every identifier, re-read the non-functional requirements — the answer is almost always to move the
generation into the caller and let a service hand out *capacity* rather than *values*.

**Where a service is unavoidable, it hands out blocks, not IDs.** `POST /id-blocks` is consulted once per
hundred thousand identifiers. The distinction between allocating a range and allocating a value is the
whole design.

**`Next()` must not return an error in the steady state.** A generator that can fail forces every call
site to handle a failure that has no sensible recovery. The one legitimate exception — the clock moving
backwards (§7.2) — is rare enough to justify blocking briefly rather than propagating an error upward.

---

## 5. Data model

The in-process generator's entire state:

```
type Snowflake struct {
    epoch        int64   // fixed at construction, never changes
    machineID    int64   // assigned at startup, 0..1023
    lastTimestamp int64  // last millisecond we issued in
    sequence     int64   // 0..4095 within lastTimestamp
    mu           sync.Mutex
}
```

Four words of state and a mutex. That is the entire persistent footprint, and the fact that there is no
durable storage is the point — a restarted process resumes safely because the timestamp has advanced.

For the block-allocation variant, the only shared state is one row:

```
id_blocks
  namespace     VARCHAR   PRIMARY KEY
  next_start    BIGINT              -- allocation high-water mark
  updated_at    TIMESTAMP
```

Allocation is a single atomic increment: `UPDATE id_blocks SET next_start = next_start + :size WHERE
namespace = :ns RETURNING next_start - :size`. One row, one statement, no locks held across a network
call. Simplicity here is a feature: this row is a correctness-critical singleton and every additional
mechanism around it is a new way to hand out the same block twice.

---

## 6. Architecture, derived

### Attempt 1: database auto-increment

Let the primary datastore assign the ID.

Perfectly ordered, perfectly unique, zero code. It fails on two of the stated requirements at once:

- **It requires a round trip before the ID is known.** You cannot construct the object, write it to a
  queue, and log its ID until the database has answered. This forces an ordering on your writes that you
  did not want.
- **It does not shard.** Two shards each issuing `1, 2, 3` collide immediately. The workarounds —
  per-shard offsets and a stride (`shard 0: 1, 5, 9...`, `shard 1: 2, 6, 10...`) — freeze the shard count
  into the identifier space, so adding a shard means re-deriving the scheme.

At 100,000 IDs/second across a sharded fleet, this is not usable. It remains the right answer for a
single-node system, and saying so is better than reflexively rejecting it.

### Attempt 2: a central ID service, one ID per call

Move the counter behind a service; every generation is an RPC.

This fixes sharding and preserves ordering. It fails the latency and availability requirements:

```
100,000 IDs/sec × 1 RPC each        = 100,000 RPC/sec against one logical counter
added latency per ID                ≈ 0.5 ms intra-datacenter round trip
```

Half a millisecond is added to every write, several times over for requests that create multiple objects.
Worse, the ID service is now on the critical path of every write in the company: its availability is an
upper bound on everything else's. You have converted a local function call into a distributed dependency
to solve a problem that does not require distribution.

### Attempt 3: block allocation (the ticket server)

Keep the central counter, but hand out **ranges**. A process requests a block of 100,000, serves from it
locally, and returns only when exhausted.

```
100,000 IDs/sec ÷ 100,000 per block = 1 RPC/sec system-wide
```

Five orders of magnitude fewer calls. The service can be down for as long as the outstanding blocks last
— at the rates above, hours — so its availability stops being load-bearing. IDs remain dense and
sortable *within* a process but interleave across processes, which is usually acceptable.

Two costs, both real and both acceptable:

- **Blocks are leaked on crash.** A process that dies holding 60,000 unused IDs never returns them. At
  2^63 available values, leaking is free; the arithmetic says you could leak a full block every second for
  three hundred thousand years.
- **Ordering across processes is loose.** Process A holding block `[1M, 1.1M)` and process B holding
  `[1.1M, 1.2M)` will issue IDs whose order does not reflect creation time at all — B's first ID is
  numerically after A's last, even if it was created hours earlier. If the ID is being used as a
  time-ordered cursor, this scheme is wrong, and that is the reason to keep going.

### Attempt 4: Snowflake — timestamp-prefixed, coordination-free

Encode the time directly, and use the machine ID to make concurrent generators disjoint.

```
id = (now_ms - epoch) << 22 | machine_id << 12 | sequence
```

```
Next():
    now = current_millis()
    if now < lastTimestamp:            // clock went backwards — see §7.2
        handle_clock_regression()
    if now == lastTimestamp:
        sequence = (sequence + 1) & 4095
        if sequence == 0:              // 4096 exhausted this millisecond
            now = wait_until_next_millis(lastTimestamp)
    else:
        sequence = 0
    lastTimestamp = now
    return ((now - epoch) << 22) | (machineID << 12) | sequence
```

No coordination on the hot path, no allocation service in the steady state, and IDs sort by creation time
to within inter-machine clock skew. This satisfies every stated requirement, and it is the answer to give
unless something specific argues otherwise.

What it costs: a dependency on the wall clock as a correctness input (§7.2), a machine-ID assignment
problem (§7.3), and — because the top bits are a timestamp — complete transparency about when a record
was created and, by differencing consecutive IDs, how fast records are being created (§7.5).

### Final architecture

```
                     ┌─── application process ───────────────┐
   startup           │                                        │
  ┌──────────┐       │   Snowflake{epoch, machineID,          │
  │  etcd /  │──────►│              lastTimestamp, sequence}  │
  │ ZooKeeper│ lease │              │                          │
  │ / pod    │ machine│             ▼                          │
  │ ordinal  │  id    │        Next() ── local, lock-held,     │
  └──────────┘       │                  no I/O, ~50 ns        │
        ▲            └────────────────────┬───────────────────┘
        │ heartbeat / lease renewal        │
        │                                  ▼
   ┌────┴─────┐                    ids emitted into
   │   NTP    │──── clock ────►    rows, logs, queues
   └──────────┘   (monitored:
                   offset alarm)
```

The remote dependencies are consulted **at startup** (machine ID) and **continuously but off the critical
path** (clock discipline). Neither is in `Next()`. That property is what the whole design is for.

---

## 7. Deep dives

### 7.1 The four axes, and why no scheme wins all of them

| Scheme | Unique | Sortable | Unguessable | Compact | Coordination |
|---|---|---|---|---|---|
| DB auto-increment | Yes | Strictly | **No** | 64 bit | Every ID |
| UUIDv4 | Yes (prob.) | **No** | Yes | 128 bit | None |
| UUIDv7 | Yes (prob.) | k-sorted | Partly | 128 bit | None |
| Snowflake | Yes | k-sorted | **No** | 64 bit | Startup only |
| Block allocation | Yes | **Loosely** | No | 64 bit | 1 per block |
| Snowflake + keyed permutation | Yes | **No** (externally) | Yes | 64 bit | Startup only |

Read the table as a set of impossibility results rather than a menu:

**Sortable implies leaky.** If the identifier's order tells you creation order, then two identifiers tell
an observer the creation interval, and a stream of them tells them your throughput. You cannot have a
public identifier that is both a time cursor and opaque. The last row escapes only by giving up external
sortability while retaining it internally (§7.5).

**Compact implies coordinated, at least once.** 64 bits is not enough entropy to make random collision
negligible at scale, so something must partition the space — a machine ID, a block, a counter. The
partitioning is the coordination, and the only question is how often you pay for it. UUIDv4 buys
zero-coordination by spending 128 bits.

**Strictly monotonic implies a bottleneck.** A single global order requires a single point that
serializes, which is attempt 2. Every scheme that avoids the bottleneck offers k-sortedness at best. When
an interviewer says "ordered", ask which one they mean; a great many designs that claim to need strict
monotonicity need only "roughly, for pagination".

### 7.2 The clock, which is a shared mutable resource you do not control

Snowflake's correctness rests on the assumption that the local clock never goes backwards. It does.

**Causes, all routine:** NTP applying a correction after drift; a VM being live-migrated or resumed from
a snapshot; a leap second handled by stepping rather than slewing; a hypervisor stalling the guest;
someone running `date -s`.

**Why it is a correctness bug, not a nuisance:** if the clock steps back 50 ms and the generator naively
continues, it re-enters a millisecond it has already issued in, with the sequence counter reset to zero.
It will re-emit identifiers it has already handed out. These are *duplicate primary keys*, and they will
surface as a constraint violation on an unrelated write minutes later, or — worse, in a store without a
unique constraint — as one row silently overwriting another.

**Three responses, in decreasing order of correctness:**

1. **Refuse to issue until the clock catches up.** Block in `Next()` until `now >= lastTimestamp`. Correct
   and simple. The cost is that a 50 ms regression stalls all ID generation for 50 ms, and a large
   regression (a resumed snapshot, minutes or hours) stalls indefinitely — so bound the wait, and if the
   regression exceeds the bound, **fail loudly and take the instance out of service.** An instance with a
   badly wrong clock should not be issuing identifiers at all.
2. **Borrow from the sequence bits.** Keep issuing at `lastTimestamp` and continue incrementing the
   sequence across the regression. Correct as long as the regression is short enough that the 4,096
   sequence values are not exhausted before the clock recovers — which makes correctness depend on a race
   you are not measuring. Acceptable only with a hard cap and the fallback in (1).
3. **Ignore it.** This is what a surprising amount of code in the wild does, and it is the bug.

**Prevention beats handling.** Run `chrony` or `ntpd` configured to *slew* rather than step for small
corrections, monitor clock offset as a first-class metric with an alarm well below your tolerance, and
prefer a monotonic clock source for the "has time advanced" check while using the wall clock only for the
value. The interview answer that stands out is not the handling logic — it is naming clock discipline as
an operational prerequisite of the design.

### 7.3 Machine ID assignment, where the worst failure lives

Duplicate machine IDs are the catastrophic failure of this scheme: two instances with the same ID will
issue identical identifiers whenever their clocks and sequences coincide, which at high throughput is
constantly. It produces duplicate primary keys with no error at the point of generation.

Four assignment mechanisms:

**Static configuration.** A number in a config file per host. Works, and does not survive autoscaling,
container rescheduling, or a copy-pasted deployment manifest — which is exactly how duplicates happen.

**Coordination service lease (ZooKeeper / etcd).** On startup, the process claims the lowest free ID as an
**ephemeral** node and holds it for its lifetime; the node vanishes if the process dies or its session
expires, returning the ID to the pool. Correct, and it introduces a startup dependency on the coordination
service. The subtlety: a process that loses its session but keeps running (a long GC pause, a network
partition) may have had its ID reassigned. Handle it by treating session loss as fatal — stop issuing and
exit — rather than by hoping.

**StatefulSet ordinal.** In Kubernetes, a StatefulSet gives each pod a stable ordinal index, which maps
directly onto the machine-ID field. Free, no extra dependency, and correct as long as you never run two
StatefulSets of the same service against one ID space — which is precisely what a blue/green deployment
or a second region does. Scope it explicitly.

**Derive from the IP or MAC address.** Tempting and unsafe: private IP ranges are reused across
availability zones and clusters, and the low bits of two addresses in different subnets collide readily.

**Whichever you choose, detect duplicates rather than trusting the mechanism.** Have each instance
register `(machine_id, hostname, start_time)` in a shared store on startup and alarm on any ID claimed
twice concurrently. This is a dozen lines and it converts a silent data-corruption bug into a page.

### 7.4 UUIDv4, UUIDv7, and why random primary keys are expensive

**UUIDv4** is 122 random bits. Collisions are negligible — the birthday bound puts a 50% chance at roughly
2^61 identifiers — and it needs no coordination at all. Two costs.

The first is width: 128 bits, and typically stored as a 36-character string by teams who do not think
about it, which is 36 bytes instead of 16 and appears in every secondary index. At 10^11 rows the
difference is measured in terabytes of index.

The second is subtler and matters more: **random keys destroy B-tree insert locality.** A clustered index
on a random key inserts into a uniformly random leaf page each time. The working set for inserts becomes
the entire index rather than its right edge, so the buffer pool cannot hold it, every insert becomes a
page read followed by a page write, and page splits scatter throughout the tree producing fragmentation
and lower fill factors. A sequential key appends to the rightmost page, which is always hot. The
difference on a large table is not marginal; it is the difference between an insert-bound workload
fitting in memory and not. See Chapter 01, §1 for why B-trees behave this way.

**UUIDv7** fixes exactly this: 48 bits of Unix milliseconds followed by random bits, standardized in
RFC 9562. It is time-sortable, retains coordination-freedom, and restores insert locality. It is the
correct modern default whenever 128 bits is affordable and you do not need Snowflake's compactness — and
because it is a standard rather than a per-company convention, it is what a greenfield service should
use.

The residual argument for Snowflake over UUIDv7 is width (64 vs 128 bits) and the ability to carve out
application-specific bits — a shard hint, a type tag — from the layout. Both are real; neither is
automatic.

### 7.5 Information leakage, and the keyed permutation that fixes it

A monotonic identifier in a URL tells an observer three things: approximately when the object was created,
that other objects exist at nearby values, and — by sampling `id` at two times — precisely how many
objects you create per hour. Competitors have historically used exactly this to measure order volume, and
enumeration of sequential IDs is a standard step in any assessment of a public API.

The instinct is to switch to random IDs, which forfeits sortability and insert locality. There is a better
answer.

**Apply a keyed bijection to the counter before encoding it.** A bijection maps the integer range onto
itself one-to-one, so uniqueness is preserved exactly — no collision handling, no lookup table — while the
output order has no relationship to the input order. A small **Feistel network** over the 64-bit range,
using a keyed hash as the round function and four or more rounds, is the standard construction: it is a
few dozen lines, it is invertible (so you can recover the internal ID from the public one without a
database lookup), and it requires no additional storage.

```
internal:  1, 2, 3, 4                    -- ordered, compact, good index locality
public:    9f2a4c11, 3e7b0055, ...       -- unguessable, non-enumerable, no volume leak
```

Keep the internal identifier as the primary key and expose the permuted form. You get sortable storage and
opaque public tokens simultaneously, which the four-axis table said was impossible — the escape is that
the two properties now belong to two different representations of the same value.

Most candidates never reach this, and it is a genuinely strong thing to offer.

### 7.6 Sequence exhaustion and the throughput ceiling

Twelve sequence bits cap a single instance at 4,096 IDs per millisecond. When exhausted, the generator
must spin until the next millisecond, which is a hard throughput ceiling of ~4.1M IDs/sec/instance.

Two observations. First, this ceiling is far above realistic demand — the estimate in §3 put a busy
process at 1,000 IDs/sec, four thousand times below the limit — so a design that hits it should first
check whether it is really issuing four million identifiers a second or whether something is looping.
Second, if it *is* real, the fix is to reallocate bits (§3's table) rather than to spin; giving up two
machine bits for two sequence bits quadruples per-instance throughput at the cost of halving the fleet
size twice over.

The spin itself must be a busy-wait on the clock, not a sleep — a millisecond sleep in a hot path with a
mutex held is a far worse outcome than the exhaustion it is handling.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Clock steps backwards | Duplicate IDs, surfacing later as constraint violations or silent overwrites | Block until caught up; hard-fail beyond a bound; slew rather than step; alarm on NTP offset |
| Duplicate machine ID | Systematic duplicate IDs across two instances | Ephemeral lease with fatal session loss; startup registration with duplicate alarm |
| Coordination service down at startup | New instances cannot start; running instances unaffected | Cache the last-held ID locally and reclaim it on restart, with the duplicate alarm as the safety net |
| Sequence exhaustion | Generation stalls to the next millisecond | Alarm on it; reallocate bits if sustained |
| Epoch set to Unix epoch | Silent 56-year loss of lifetime; IDs overflow the field in ~14 years | Assert at construction that the epoch is within a few years of the project start |
| Block-allocation counter row lost | Reissues the entire ID space from zero | Treat the row as critical data: replicated, backed up, and monitored for monotonicity |
| Clock far ahead (not behind) | IDs from the future; no duplicates, but ordering is wrong and the field is consumed early | Same NTP monitoring; assert the generated timestamp is within a sane window |

**Monitoring:** NTP offset per host with an alarm at a fraction of your tolerance; clock-regression events
counted (this should be zero, and any non-zero value is worth investigating); sequence-exhaustion events;
IDs issued per second per machine ID (a machine ID appearing from two hostnames is the duplicate alarm);
and the high-water mark of the timestamp field against its 69.7-year ceiling, checked annually rather
than continuously.

---

## 9. Common mistakes

1. **Using the Unix epoch as the Snowflake epoch.** Silently discards 56 of 69.7 years. The most common
   real defect in implementations of this scheme, and it is invisible until the field overflows.
2. **Treating clock regression as a nuisance rather than a correctness bug.** It produces duplicate
   primary keys. "We'd just log it and continue" is a wrong answer.
3. **Deriving the machine ID from an IP or MAC address.** Private ranges are reused across zones and
   clusters; collisions are not hypothetical.
4. **Proposing a central service with one RPC per ID** and not noticing it violates the latency and
   availability requirements that were stated two minutes earlier.
5. **Using UUIDv4 as a clustered primary key** without mentioning insert locality. It is a defensible
   choice with a real cost, and the cost is the interesting part.
6. **Claiming strict global monotonicity from a coordination-free scheme.** Snowflake is k-sorted, bounded
   by clock skew. Say k-sorted.
7. **Exposing sequential IDs publicly** and not mentioning enumeration or volume leakage.
8. **Memorizing 41/10/12 as if it were a law.** It is a budget. The interviewer wants to see you allocate
   it against the numbers you just derived.
9. **Forgetting that block allocation loses cross-process time ordering.** It is otherwise the best answer
   for many systems, and this is the one requirement it quietly fails.

---

## 10. Variants

**Short public codes (Chapter 10).** A 7-character base62 code is a 42-bit space, far too small for
Snowflake's timestamp. That chapter's block-allocation-plus-permutation answer is the right shape, and
this chapter's §7.5 is the mechanism it depends on.

**Sharded primary keys.** Steal bits from the ID for a shard hint, so the router can locate a row from its
ID without a lookup. Cheap and effective; the cost is that resharding now changes the meaning of existing
identifiers, so the hint must be advisory (a routing accelerator) and never authoritative.

**Idempotency keys (Chapter 03, §4).** A different problem wearing similar clothes: the client generates
it, uniqueness need only hold within a TTL window, and unguessability does not matter. UUIDv4 is correct
here and Snowflake would be over-engineering.

**Ordered IDs across a multi-region deployment.** Clock skew between regions is larger than within one, so
k-sortedness degrades. Either accept coarser ordering, or partition the machine-ID space by region and
treat cross-region order as undefined — the honest option, and the one that does not pretend a global
clock exists.

**ULID.** 48-bit timestamp plus 80 random bits, Crockford-base32 encoded, lexicographically sortable as a
string. Essentially UUIDv7's design with a text-friendly encoding; choose it when identifiers are handled
as strings throughout.

---

## 11. Further reading

- Chapter 04, §1, for the compact statement of the layout this chapter expands
- Chapter 10, §7.1, for the same allocation problem under a much smaller keyspace
- RFC 9562, "Universally Unique IDentifiers (UUIDs)" — the normative definition of UUIDv7
- Twitter's original Snowflake announcement and source release, for the bit layout and its rationale
- Instagram Engineering, "Sharding & IDs at Instagram" — a production variant that embeds a shard ID
- Rob Pike and Ken Thompson's discussion of Feistel networks in the context of format-preserving
  encryption; NIST SP 800-38G covers the standardized constructions
