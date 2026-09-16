# Chapter 83 — Trading System

> **Prerequisites:** Chapters 01 (§8 transactions, durability), 03 (logs, replay), 82 (single-owner serialization), 93 (the log this depends on)
> **Patterns:** deterministic single-threaded state machine, state replication by input log, microsecond latency budgets, pre-trade risk as an inline gate

---

## 1. The problem

Buyers and sellers submit orders for a security. The system maintains a book of resting orders and matches
incoming ones against it according to a fixed priority rule, publishing the resulting trades and the
book's state to everyone watching.

This chapter exists in the book for one idea, and the idea generalizes far beyond finance: **the matching
engine is a single-threaded deterministic loop, and it is replicated by shipping its inputs rather than
its state.** Almost every instinct a distributed-systems engineer brings to this problem — shard it,
parallelize it, replicate the state, use a consensus protocol on the data — makes it slower and less
correct. Understanding *why* the obvious approach is wrong here is worth more than the domain knowledge.

The second idea is that the latency budget changes which techniques exist. At 200 milliseconds you choose
between databases. At 200 *microseconds* you are choosing between memory layouts, and garbage collection,
system calls, and cache misses are all on the budget. Most of what the rest of this book takes for granted
— a network hop to a datastore, a JSON parse, a lock — costs more than the entire budget.

**The property that makes it hard:** the matching rule requires a total order over all orders for a
security, correctness is absolute and legally auditable, and the latency budget is small enough that the
usual mechanisms for achieving ordered, durable, replicated state are all individually too slow.

---

## 2. Requirements

### Functional

1. Accept limit and market orders; accept cancels.
2. Match incoming orders against the resting book by price-time priority, generating trades.
3. Publish trades and book updates to market data subscribers.

Defer, but name: clearing and settlement (a separate system operating on a T+1 timescale), complex order
types (stop, iceberg, pegged), auctions at open and close, and multi-leg or cross-security orders.

### Non-functional

- **Matching latency** — p99 under 100 microseconds from the engine's ingress to its egress, and the tail
  matters more than the median. Participants measure and compare this number, and jitter is a competitive
  disadvantage for them.
- **Determinism** — given the same ordered input, the engine must produce byte-identical output. This is
  not a performance property; it is what makes §7.2's replication and §7.6's audit possible.
- **Total order per security** — absolute. Price-time priority is meaningless without an unambiguous
  definition of "time".
- **Durability** — an acknowledged order must survive the loss of a machine. No exceptions and no
  sampling.
- **Throughput** — 10^5–10^6 messages/sec per security during a burst; §3.
- **Fairness** — the ordering must be defensible to a regulator and to participants who will reverse-
  engineer it. "Roughly in order" is not a thing.
- **Availability** — a halt is preferable to an incorrect match. This inverts the usual preference and is
  worth stating: in most of this book, degraded service beats no service; here, a wrong trade is worse
  than a stopped market.

### Explicitly out of scope

Clearing, settlement, custody, and regulatory reporting. Also market-making strategy and anything a
*participant* builds — this chapter designs the venue, not the trader.

---

## 3. Estimation

**Message rate**

```
liquid security, normal:     ~5,000 messages/sec (orders + cancels)
open, close, news event:     ~500,000 messages/sec, bursting for seconds
cancel:trade ratio:          often 20:1 or higher — most messages never trade
```

**The cancel ratio is the number people miss.** The overwhelming majority of messages modify or remove
resting orders rather than execute. The book data structure must therefore make **cancel** as cheap as
match — which rules out designs where cancel is a search (§5).

**Latency budget, itemized**

```
total budget, ingress to egress:               ~100 µs
  network in (kernel bypass)                     ~5 µs
  decode, validate                               ~2 µs
  pre-trade risk check                           ~5 µs
  matching                                       ~1 µs
  journal to durable log (replicated)           ~20 µs
  encode, publish market data                    ~5 µs
  network out                                    ~5 µs
                                               ────────
                                                 ~43 µs, leaving headroom for jitter
```

Two things fall out. **The journal dominates**, and §7.3 is about making a replicated durable write cost
tens of microseconds rather than milliseconds. And **matching itself is nearly free** — the actual
business logic is a handful of comparisons and pointer updates, which is why spending engineering effort
parallelizing it would be absurd.

For contrast: a single database round trip is ~500 µs, five times the entire budget. A garbage collection
pause is 1–100 ms, one thousand times the budget. This is why the design looks unlike everything else in
this book.

**Book size**

```
resting orders per liquid security:   10^4 – 10^5
distinct price levels:                10^2 – 10^3
```

Small. The entire book fits in a few megabytes and comfortably in L2/L3 cache, which is a design input
rather than a coincidence — §5 arranges the data structure to keep it that way.

**Securities**

```
5,000 securities × ~5,000 msg/sec average = 25 × 10^6 messages/sec aggregate
```

Which is why sharding by security (§7.4) is not optional, even though sharding *within* a security is
forbidden.

---

## 4. API

```
// Binary protocol over a kernel-bypass transport. Fixed-width, no parsing.
NewOrder    { clientOrderId, security, side, price, qty, tif, participantId }
Cancel      { clientOrderId, security }
Replace     { clientOrderId, newPrice, newQty }

// Outbound, per participant
Ack         { clientOrderId, exchangeOrderId, sequence, timestamp }
Reject      { clientOrderId, reason }
Fill        { exchangeOrderId, price, qty, tradeId, sequence }

// Outbound, broadcast
MarketData  { security, sequence, [book deltas | trade prints] }
```

Four decisions:

**A fixed-width binary protocol, not JSON.** Parsing JSON costs microseconds and allocates; a fixed-layout
binary message is a struct cast. At a 100 µs budget, serialization format is an architectural decision
rather than a preference. (Real venues use FIX or a proprietary binary encoding; the specific choice
matters less than that it is fixed-width and allocation-free.)

**Every outbound message carries a `sequence`.** It is the engine's monotonic counter for that security,
and it is what lets a participant detect a gap, request a replay, and know unambiguously whether their
cancel arrived before or after someone else's order. The sequence *is* the total order (§7.1), exposed.

**No query API on the hot path.** There is no "get my open orders" call into the engine. Participants
maintain their own state from the message stream, and a separate drop-copy service reconstructs it from
the journal for reconciliation. Adding a read path into the matching loop would put an unpredictable
workload inside a deterministic one.

**Market data is a separate stream with its own sequence**, published to all subscribers identically. That
identity is a fairness requirement: if two subscribers receive different views, one has an advantage.

---

## 5. Data model

The order book, which is the entire state:

```
book (per security)
  bids: price level → FIFO queue of orders     -- descending price
  asks: price level → FIFO queue of orders     -- ascending price

price level
  price, total_qty, doubly-linked list of orders

order
  exchange_order_id, participant, qty, remaining,
  prev, next          -- position in its level's FIFO
  level               -- back-pointer to its price level

order_index: exchange_order_id → order*        -- hash map, for O(1) cancel
```

Four deliberate decisions:

**Price levels are an array indexed by price, not a tree.** Prices are discrete (a tick size) and live
within a narrow band around the last trade. An array of levels covering that band gives O(1) access to any
level, versus O(log n) for a tree, and — more importantly — it is contiguous in memory, so walking levels
is a cache-friendly linear scan. The band is re-based when the price moves outside it, which is rare and
can be slow. This is a classic space-and-simplicity trade made in favor of cache behavior, and it is only
correct because the price band is bounded.

**Each level holds a FIFO queue, implemented as a doubly-linked list.** Price-time priority means the
oldest order at a price matches first — that is a queue. Matching consumes from the head; new orders
append at the tail. Both O(1).

**`order_index` maps order ID directly to the order node, and the node holds a back-pointer to its
level.** This is what makes cancel O(1): look up the node, unlink it from its list, decrement the level's
total. Given the 20:1 cancel-to-trade ratio from §3, a design where cancel requires searching a level is
strictly worse than one where matching is slightly slower. **Optimize for the operation that dominates,
which is not the one the system is named after.**

**Everything is preallocated.** Order nodes come from a fixed-size pool, not the allocator. Allocation on
the hot path means eventual garbage collection or malloc contention, either of which blows the budget by
orders of magnitude. This constrains the implementation language as much as it constrains the data
structure.

---

## 6. Architecture, derived

### Attempt 1: orders in a database, matching in a transaction

Each order is a row; matching is a transaction that reads the opposite side, computes fills, and writes.

Correct, and it misses the latency budget by four orders of magnitude. A single round trip is 500 µs; the
transaction involves several, plus lock acquisition on a hot table where every order for a security
contends.

Worth stating only to establish the scale of the gap. This is not a design that needs tuning; it is the
wrong category of design.

### Attempt 2: in-memory book, multi-threaded, with locks

Keep the book in memory. Use a thread pool; lock the book per operation.

The memory move is right and buys three orders of magnitude. The threading is wrong, for three separate
reasons that compound:

**All operations on one security must serialize anyway**, because matching reads and writes the shared
book. The threads spend their time waiting for the lock. The parallelism is nominal.

**Lock acquisition costs more than the work.** Matching is ~1 µs (§3). An uncontended atomic operation is
tens of nanoseconds, but a contended lock involves a futex, a context switch, and a scheduler round trip —
microseconds, sometimes tens. **The coordination is more expensive than the thing being coordinated.**

**It destroys determinism.** Thread scheduling is nondeterministic, so two runs over the same inputs can
produce different interleavings and different trades. That forfeits §7.2's replication scheme and §7.6's
audit, both of which depend on being able to replay inputs and get the same answer.

### Attempt 3: single-threaded deterministic loop

One thread owns the book for a security. It reads from an input queue, processes each message to
completion, and writes outputs.

```
loop:
    msg = input_ring.poll()          -- busy-wait, no syscall
    seq = ++sequence
    journal(seq, msg)                -- durable, replicated (§7.3)
    outputs = book.apply(msg)        -- pure function of (book, msg)
    publish(outputs)
```

No locks, because there is no concurrency. No nondeterminism, because there is no scheduler involved in
the outcome. The thread is pinned to a core and busy-polls its input rather than blocking, so there is no
context switch and no wakeup latency.

Throughput is bounded by single-core speed, which sounds like a limitation and is not: at ~1 µs of
matching work, one core sustains on the order of a million messages per second, above the burst rate for
any single security from §3.

**The counterintuitive result to state plainly: removing the concurrency made it faster.** The concurrency
was never performing independent work; it was contending for a resource that is inherently serial, and
paying coordination costs for the privilege. This is the same conclusion Chapter 82 reached from a
different direction, and it generalizes: **when a resource must be serialized, own it in one thread rather
than coordinating access to it from many.**

### Attempt 4: replicate the inputs, not the state

The engine's state is in the memory of one process. If that machine dies, the book is gone. The instinct is
to replicate the state — stream book updates to a standby.

That is the wrong choice here, for two reasons. The book is large relative to the messages that change it,
so state replication moves far more bytes than input replication. And it re-introduces the question of
whether the replica's state is *exactly* right, which is difficult to verify.

**Because the engine is deterministic, replaying the same ordered inputs reproduces the same state
exactly.** So replicate the input log:

```
                  ┌──► primary engine   ── outputs ──► participants
sequenced input ──┤
      log         ├──► standby engine   (same inputs, same state, outputs suppressed)
                  └──► standby engine
```

Each replica independently computes the identical book. Failover is promoting a standby that is already
warm and already correct — no state transfer, no catch-up beyond the last few messages. Recovery from
total loss is replaying the log from a periodic snapshot.

**This is state machine replication**, and determinism is its precondition. It is also why attempt 2's
nondeterminism was disqualifying rather than merely untidy: without determinism, replicas diverge and the
entire scheme collapses.

### Attempt 5: shard by security, and only by security

One core per security does not scale to 5,000 securities and 25 million messages/second aggregate. But
different securities share no state — a trade in one cannot affect the book of another — so they are
**independent state machines** and can run on separate cores and separate machines.

The line is sharp and worth naming: **shard across securities, never within one.** Sharding within a
security would require distributed coordination on every match, which is the thing the design exists to
avoid. §7.4.

### Final architecture

```
  participants
      │  binary protocol, kernel bypass
      ▼
  ┌── Gateway ──────────────────────────────┐
  │  decode, authenticate, PRE-TRADE RISK   │  §7.5 — inline, must be
  │  (reject before sequencing)             │       microseconds
  └───────────────┬─────────────────────────┘
                  │
                  ▼
  ┌── Sequencer ────────────────────────────┐
  │  assigns the total order; writes to the │  §7.1 — the single point
  │  replicated journal (§7.3)              │       that defines "first"
  └───────────────┬─────────────────────────┘
                  │  sequenced, durable input log
       ┌──────────┼──────────┬──────────────┐
       ▼          ▼          ▼              ▼
   Engine A   Engine B    Standby A    Journal archive
  (sec 1-500)(sec 501+)  (replay of A)  (snapshots + log)
       │                                    │
       │  fills, book deltas                └──► drop copy, audit, replay
       ▼
  ┌── Market data publisher ──┐
  │  tiered fanout, identical │
  │  stream to all subscribers│
  └───────────────────────────┘
```

The sequencer is the only component that must be globally ordered, and it does the minimum possible work
to establish that order.

---

## 7. Deep dives

### 7.1 The sequencer, and why ordering is a separate concern from matching

Price-time priority requires an unambiguous "time". Wall clocks across a fleet disagree by more than the
inter-arrival time of messages during a burst, so the clock cannot define the order.

**A dedicated sequencer assigns a monotonic sequence number to every inbound message.** That number, not
any timestamp, is the order of record — the same conclusion Chapter 82 reached for bids and Chapter 30 for
chat messages, arrived at a third time.

Separating sequencing from matching buys three things:

**Ordering is established once, for all securities**, so cross-security consistency is well-defined even
though the engines are independent. This matters for audit and for participants running strategies across
instruments.

**The sequencer's work is trivial and uniform**, so its latency is predictable. It does not parse business
logic, evaluate risk, or touch a book — it stamps and journals. Predictability matters as much as speed
here, because jitter in the ordering path is jitter every participant sees.

**Failover has a clean definition.** A standby sequencer takes over at a known sequence number, so there is
never ambiguity about whether a message was ordered.

**Fairness at the ingress is the residual problem, and it is genuinely unsolvable in general.** Two
participants pressing at the same instant are ordered by whose packet arrives first, which depends on
cable length and switch queuing. Venues address this with equalized cable lengths inside the colocation
facility and, in some markets, deliberate randomized delays — but no amount of design makes a 200 µs link
equal to a 2 µs one. State it as a known limit rather than implying the sequencer solves it.

### 7.2 Determinism as a load-bearing property

Determinism is usually a nice-to-have. Here, four separate mechanisms depend on it, and losing it breaks
all four at once:

- **Replication** (§6, attempt 4) — replicas compute identical state from identical inputs.
- **Recovery** — replaying the journal from a snapshot reconstructs the exact book, not an approximation.
- **Audit** — a regulator asking "why did this trade happen at this price?" is answered by replaying the
  inputs and observing the same result. An approximate answer is not an answer.
- **Testing** — a production incident is reproducible offline, byte for byte, against a candidate fix.

Achieving it requires discipline that is unusual outside this domain:

**No wall-clock reads inside the engine.** Time enters as a field on a sequenced input message, assigned by
the sequencer. An engine that calls `now()` produces different output on replay.

**No randomness, no hash iteration order.** Any map iterated during matching must have a defined order, or
must not be iterated.

**No concurrency inside the engine**, which attempt 3 already established.

**No allocation-dependent behavior.** Preallocated pools (§5) also serve determinism: behavior must not
depend on whether an allocation succeeded or how long it took.

**Floating point requires care.** Prices are integers in minor units — never floats — for exactness, the
same discipline as Chapter 81. Any genuinely floating-point computation must be pinned to identical
instruction behavior across replicas, which in practice means avoiding it.

The general lesson transfers: **determinism converts a distributed state problem into a distributed log
problem**, and distributed logs are a solved problem (Chapter 93) in a way distributed mutable state is
not. Systems that can adopt this constraint should, and many that could do not realize it is available.

### 7.3 Durability inside a 20-microsecond budget

An acknowledged order must survive machine loss. The obvious implementations are far too slow: `fsync` to
an SSD is ~100 µs to milliseconds, and a database write is worse.

Three techniques, used together:

**Replicate to memory on other machines, not to disk on this one.** Write the journal entry to two or three
peers over a low-latency link and acknowledge when a quorum has it in memory. A round trip on a tuned
10/25 GbE link with kernel bypass is ~5–10 µs, so a quorum write is achievable inside the budget. Durability
is provided by *independence of failure domains* rather than by non-volatile media — the same reasoning
that lets Chapter 93's Kafka acknowledge on in-sync replicas rather than on fsync.

**Write sequentially, batch opportunistically.** The journal is append-only. Under load, several messages
arriving in the same few microseconds are written as one batch, so per-message cost falls as load rises —
the system gets *more* efficient exactly when it is busiest, which is the opposite of most systems and is
a property worth pointing out.

**Persist asynchronously behind the in-memory quorum.** Each replica flushes to disk on its own schedule.
The acknowledged-durability boundary is the memory quorum; the disk write protects against correlated
failure of the whole cluster.

**The remaining exposure** is simultaneous loss of every replica within the flush interval — a datacenter
power event. That is accepted, documented, and mitigated by placing replicas in separate power and network
domains. Naming the residual risk precisely, rather than claiming durability is absolute, is the honest
position.

### 7.4 Sharding by security, and the cross-security limit

Securities are independent state machines, so they shard cleanly across cores and machines. Assignment is
static or slowly-changing, because moving a security means quiescing its engine, snapshotting, and
restarting elsewhere — cheap enough to do between sessions, not during one.

**Balance by message rate, not by count.** Symbol volume follows a power law; a shard holding a handful of
the most active names will exceed one holding a thousand quiet ones. Rebalance between sessions using
measured rates.

**Cross-security atomicity is not offered, and this is a deliberate product decision rather than a
limitation to apologize for.** A multi-leg order — buy one instrument, sell another, atomically — would
require a distributed transaction spanning two engines, which would put coordination inside the loop that
exists to have none. The available answers are all worse than the constraint: run both legs on the same
engine (limits sharding), accept legging risk and handle partial execution in a layer above (what most
venues do), or build a separate combination-order book that treats the spread as its own instrument (what
derivatives venues do). Choose explicitly and say why.

### 7.5 Pre-trade risk, an inline gate with a microsecond budget

Before an order reaches the book, it must be checked: does the participant have sufficient buying power,
is the order within position and size limits, is the price within a sanity band, is the participant
permitted to trade this instrument.

These checks are **mandatory and inline**. Checking after matching is meaningless — the trade has already
happened and cannot be unwound. This is a case where a correctness requirement forces work onto the
critical path regardless of its cost, and the design must absorb it rather than defer it.

Fitting them into ~5 µs:

**Keep all risk state in memory, per participant, colocated with the gateway.** No lookup to a risk
service; a lookup is a round trip and the budget does not have one.

**Make the checks arithmetic, not queries.** Buying power is a number that is decremented on order and
restored on cancel or fill. Position limits are comparisons. Anything requiring aggregation across
participants or securities is precomputed and refreshed out of band.

**Accept staleness where it is safe, and be explicit about where it is not.** A position limit computed
from state that is a few hundred microseconds old is acceptable; buying power must be exact, because a
participant who can spend the same capital twice is an unbounded loss. Distinguishing which checks
tolerate staleness is the substance of the deep dive — the answer is "the ones where the error is bounded
and self-correcting" versus "the ones where it compounds".

**The fat-finger check earns its place.** A price sanity band that rejects orders wildly away from the last
trade prevents the single most common catastrophic error, costs one comparison, and has repeatedly been the
difference between an embarrassing order and a market-wide incident. It is the cheapest risk control in
the system.

The design tension is real and should be named: every risk check adds latency, participants compete on
latency and will complain, and the checks are non-negotiable. The resolution is to make them fast, not to
make them fewer.

### 7.6 Market data fanout and audit replay

**Fanout.** Trades and book deltas go to every subscriber. The volume is high and the fairness requirement
is unusual: all subscribers must receive the **identical stream**, because a difference in content or
ordering is an advantage. Multicast within the colocation facility, tiered gateways beyond it (the same
tiered structure as Chapters 31 and 82). Conflated feeds — coalescing updates for slow subscribers — are
offered as a *separate, clearly labeled product*, never as a silent degradation of the primary feed,
because a subscriber who does not know they are receiving a coalesced view is being disadvantaged without
consent.

**Gap recovery.** Multicast drops packets. Every message carries a sequence number, and a separate replay
service serves ranges from the journal. This is why sequence numbers are exposed in the API (§4) rather
than being an internal detail.

**Audit replay** is the journal's second job. Regulators ask why a specific trade occurred; the answer is
to replay the sequenced inputs from a snapshot and demonstrate that the engine produces exactly that
trade. This requires the journal to be immutable, retained for years, and to include *rejected* messages
— the reason an order did not trade is as auditable as the reason one did.

Note that the journal serves replication, recovery, market data gap-fill, audit, and offline testing.
**One log, five consumers.** That economy is a direct consequence of the determinism decision, and it is
the strongest argument for it.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Primary engine crashes | That shard's securities stop | Warm standby already at the same sequence; promote in milliseconds — no state transfer needed |
| Sequencer fails | All ordering stops, market-wide | Hot standby with a known handover sequence; this is the one true single point and it is engineered accordingly |
| Replica divergence | Standby's book differs from primary — silent and catastrophic if promoted | Continuous checksum comparison of book state at sequence checkpoints; halt on mismatch rather than continue |
| Journal quorum unavailable | Cannot durably acknowledge | **Halt.** Do not accept orders you cannot journal; a halt is preferable to an unrecoverable trade |
| GC pause or page fault in the engine | Latency spike affecting every participant | Preallocated pools, no allocation on the hot path, huge pages, memory locked resident, core pinning |
| Message burst beyond capacity | Queue growth, latency climbs | Bounded input queues with explicit rejection at the gateway; never an unbounded queue (Chapter 03, §6) |
| Erroneous order accepted | Trades that must be busted | Fat-finger bands at the gateway; a documented trade-bust procedure, because prevention is never complete |
| Clock skew | No effect on ordering | The sequence, not the clock, is authoritative — this is a designed non-failure and worth noting as such |

**Monitoring:** latency percentiles to p99.99 measured at hardware timestamps rather than in application
code; **jitter**, not just the median, since participants experience the tail; queue depth at gateway and
sequencer; replica sequence lag and state checksum agreement; journal quorum write latency; message rate
by security against shard capacity; and reject rates by reason, where a change in the mix is usually the
first sign that a participant's system has malfunctioned.

---

## 9. Common mistakes

1. **Parallelizing the matching engine.** The work is inherently serial, coordination costs more than the
   matching, and it destroys the determinism four other mechanisms depend on.
2. **Replicating state instead of inputs.** More bytes, harder to verify, and it discards the property
   that makes replication trivial.
3. **Not recognizing determinism as load-bearing**, and permitting a `now()` call or a map iteration
   inside the engine.
4. **Using wall clocks for priority.** Clocks disagree by more than the inter-arrival time; the sequence
   is the order.
5. **Optimizing match at the expense of cancel**, when cancels outnumber trades 20:1. The `order_index`
   plus back-pointer exists for this reason.
6. **Allocating on the hot path**, importing a garbage collector or allocator whose pauses exceed the
   entire budget by three orders of magnitude.
7. **Deferring risk checks until after matching**, where they are meaningless.
8. **Using floating point for prices.** Integers in minor units, always (Chapter 81).
9. **Offering cross-security atomicity** without noticing it requires distributed coordination inside the
   loop designed to have none.
10. **Treating a halt as failure.** In this domain a halt is the correct response to uncertainty, and a
    design that prefers availability over correctness has the requirements backwards.

---

## 10. Variants

**Cryptocurrency exchange.** Same engine, and three differences: it runs continuously with no session
boundaries (so the between-sessions rebalancing and snapshotting of §7.4 must happen online), custody is
internal rather than delegated to a clearing house (so the risk state in §7.5 includes actual balances),
and withdrawal is an irreversible external action requiring its own controls. The matching core is
unchanged.

**Sports betting and prediction markets.** Order book matching with the same structure, at far lower rates
and far looser latency budgets — which means most of this chapter's techniques are unnecessary and the
design collapses toward Chapter 82's. The interesting additions are event resolution and settlement
against an oracle.

**Ad exchange real-time bidding.** Called an auction and structurally unlike one: a sealed single-round
auction completed in ~100 ms, hundreds of thousands of times per second, with no resting book and no
persistent state. Chapter 70's shape.

**Internal matching / dark pools.** Same engine, no public market data feed, and matching typically at a
price derived from the public market rather than discovered internally. Removing the fanout removes most
of the throughput requirement, leaving the determinism and audit requirements untouched.

**Rate limiting and admission control at extreme scale.** Not a trading system, but the same technique:
when a counter must be strictly correct and is contended, owning it in a single-threaded process fed by a
queue beats distributed coordination. Worth noticing that §6's conclusion is portable to problems with no
finance in them at all.

---

## 11. Further reading

- Chapter 82, for the same single-owner serialization at a millisecond budget; Chapter 93, for the
  replicated log this depends on; Chapter 81, for integer money and the ledger discipline
- Martin Thompson et al. on the LMAX Disruptor — the canonical public description of a single-threaded
  deterministic matching engine and the ring-buffer transport that feeds it, and the clearest available
  writeup of why removing concurrency made it faster
- Fred Schneider, "Implementing Fault-Tolerant Services Using the State Machine Approach" (1990) — the
  formal statement of §7.2's replication scheme and its determinism precondition
- The published matching-engine specifications of major venues, for how price-time priority and order
  types are defined precisely enough to be unambiguous
