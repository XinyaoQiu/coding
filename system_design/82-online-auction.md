# Chapter 82 — Online Auction

> **Prerequisites:** Chapters 01 (§8 transactions, isolation, write skew), 02 (§8 push protocols), 04 (§2 locks), 80 (the contention machinery this reuses), 92 (scheduled close)
> **Patterns:** contention on a single hot row, server-authoritative ordering, live fanout to watchers, scheduled simultaneity

---

## 1. The problem

An item is listed with a closing time. Users place bids; each must exceed the current high bid. When the
clock runs out, the highest bidder wins.

Chapter 80 handled contention over **many** scarce resources — fifty thousand seats, each wanted by a few
people. This chapter handles contention over **one** resource wanted by many, and the difference matters
more than it first appears. Seat contention shards naturally: seat 14A and seat 22C are independent rows,
so the load spreads across the inventory. An auction has a single high-bid value that every bid must read
and conditionally update. **There is no sharding a single number.** All the concurrency lands on one row.

The second difference is time. A ticket sale has a start; an auction has an *end*, and the end is when
everything happens. Bidding activity across a marketplace is roughly flat until the final seconds of each
auction, at which point a given item's traffic rises by orders of magnitude and then stops. The system is
mostly idle, punctuated by thousands of independent tiny spikes.

**The property that makes it hard:** a serialized write path on a single row, under a load spike that is
both extreme and precisely predictable, where the ordering of two bids arriving milliseconds apart
determines who wins — and the loser will believe they were cheated.

---

## 2. Requirements

### Functional

1. Place a bid on an item, accepted only if it exceeds the current high bid by the minimum increment.
2. Show the current price and bid history to anyone watching, live.
3. Close the auction at its end time and determine the winner.

Defer, but name: payment and settlement (Chapter 81), listing creation and search (Chapter 51), disputes
and feedback, and reserve prices — though reserve is cheap to include and worth mentioning.

### Non-functional

- **Bid acceptance latency** — p99 under 300 ms. Slower than this and users in the closing seconds cannot
  respond to being outbid, which is a fairness problem, not just a latency one.
- **Bid ordering** — must be a total order per item, determined by the server, and must be defensible
  after the fact. This is a legal and trust requirement as much as a technical one.
- **No lost bids and no double-accept.** A bid that returns success must be durable and must be visible in
  the history. Unlike Chapter 52's swipes, there is no acceptable loss rate here.
- **Live update latency** — watchers should see a price change within ~1 second. Not instant, but slow
  enough that a user bids against a stale price is a complaint.
- **Close-time correctness** — the winner is determined by the server's clock, and a bid arriving one
  millisecond before the deadline must be honored. §7.3.
- **Consistency** — the current high bid is **strongly consistent** on the write path; the displayed price
  for watchers may be eventually consistent by up to a second. Two different guarantees for the same
  number, and being explicit about the split is the design.

### Explicitly out of scope

Fraud detection, shill bidding, and identity verification. Name them — shill bidding in particular is the
thing an experienced interviewer will probe, and acknowledging it as out of scope is better than
pretending the system prevents it.

---

## 3. Estimation

Assume a marketplace with 10 million active auctions and 100 million bids per day.

**Average bid rate**

```
100e6 / 86,400 ≈ 1,160 bids/sec
```

Trivial. If this were the whole story there would be no chapter.

**The distribution is what matters**

Bidding is overwhelmingly concentrated at the close. Assume 60% of an auction's bids arrive in its final
minute, and closings are spread through the day but cluster in evening hours.

```
auctions closing per second (evening peak):
  10e6 auctions × 7-day duration → ~1.65e6 close/day
  evening concentration (3×)     → ~57 closes/sec

per popular auction, final 10 seconds:
  a contested item may take 50-100 bids in its last 10 seconds
                                 → 5-10 bids/sec on ONE row
```

**Five to ten writes per second against a single row, all of which must serialize.** That is the number.
It is small in absolute terms and enormous relative to what one row can do while holding a lock and
returning within 300 ms — and it is why §7.1 is the chapter's core.

**Watcher fanout — the second constraining number**

```
a popular auction: ~100,000 concurrent watchers
price updates in the final minute: ~60

60 updates × 100,000 watchers = 6,000,000 messages in 60 seconds
                              = 100,000 messages/sec for ONE item
```

One hundred thousand messages per second, for one item, from a single price change stream. This dwarfs
the bid rate by four orders of magnitude and is the reason §7.4 exists. **The read fanout, not the write
contention, is where the capacity goes** — a useful inversion of the instinct that contention is always
the expensive part.

**Storage**

```
100e6 bids/day × 365 × ~80 B ≈ 2.9 TB/year
```

Unremarkable, and it must be immutable and auditable, which is the only interesting thing about it.

---

## 4. API

```
POST /items/{itemId}/bids
  body: {maxAmount, clientBidId}          # maxAmount, not amount — see §7.5
  -> 201 {bidId, currentPrice, highBidderIsYou: bool, serverTime}
  -> 409 {reason: "outbid"|"below_increment"|"auction_closed", currentPrice}

GET  /items/{itemId}                      # snapshot: price, high bidder, end time
GET  /items/{itemId}/stream               # SSE: price and end-time updates
GET  /items/{itemId}/bids?cursor=         # history
```

Four decisions:

**The client submits `maxAmount`, not the amount to bid.** This is proxy bidding (§7.5), and putting it in
the API from the start rather than bolting it on later is the right call — it changes what a "bid" *is*,
and retrofitting it means changing the meaning of stored rows.

**`409` returns `currentPrice`.** A rejected bid must tell the client what it lost to, so the UI can offer
an immediate re-bid without a second round trip. In the closing seconds that round trip is the difference
between winning and not.

**`serverTime` is returned on every bid.** Clients need to display a countdown, and a countdown driven by
the client's own clock will be wrong by seconds. Returning server time lets the client compute an offset
once and render a countdown that agrees with the server's notion of the deadline — which is the only
notion that matters (§7.3).

**SSE, not WebSocket, for the watch stream.** The flow is entirely server-to-client: watchers receive
price updates and send nothing on that channel; bids go over ordinary HTTP POST. SSE gives automatic
reconnection and `Last-Event-ID` resumption, works through proxies, and — critically — does not require
the fleet to become stateful in the way Chapter 30's chat servers do. Reaching for WebSocket here is the
common reflex and it buys nothing. (Chapter 02, §8.)

**`clientBidId` for idempotency.** A retried bid must not become a second bid. At 300 ms latency in the
closing seconds, clients retry.

---

## 5. Data model

```
items
  item_id        BIGINT PRIMARY KEY
  seller_id      BIGINT
  reserve_price  BIGINT              -- in minor units; never a float (Ch. 81)
  end_time       TIMESTAMP           -- mutable: extension writes here (§7.3)
  status         SMALLINT            -- open / closing / closed / settled
  version        INT                 -- optimistic concurrency token

auction_state                        -- the hot row, deliberately separated
  item_id        BIGINT PRIMARY KEY
  current_price  BIGINT
  high_bidder    BIGINT
  high_max       BIGINT              -- the winner's hidden maximum (§7.5)
  bid_count      INT
  version        INT

bids                                 -- append-only, immutable, the audit record
  bid_id         BIGINT PRIMARY KEY  -- Snowflake: server-assigned, time-ordered
  item_id        BIGINT
  bidder_id      BIGINT
  max_amount     BIGINT
  accepted       BOOLEAN
  server_ts      TIMESTAMP           -- server clock, authoritative
  sequence       INT                 -- per-item monotonic, the tiebreak of record
```

Four deliberate decisions:

**`auction_state` is split from `items`.** The hot, contended, frequently-updated fields live in their own
narrow row. Updating the price does not touch the item's title, description, or images, so the row is
small, the update is cheap, and — importantly — the contended row is not also being read by every page
render that wants the description.

**`high_max` is stored and never exposed.** Proxy bidding requires knowing the current leader's maximum in
order to decide whether a new bid beats it, and revealing it would let the next bidder bid exactly one
increment above. It must be readable by the bid evaluation logic and by nothing else, which is worth
saying out loud because it is an access-control requirement embedded in a schema decision.

**`bids` is append-only, including rejected bids.** Storing rejections costs little and is the difference
between being able and unable to answer "why did I lose?" three weeks later. In a system where money and
trust are involved, the audit trail is a feature, not a byproduct.

**`sequence` is a per-item monotonic counter, distinct from the timestamp.** Two bids can share a
millisecond; the sequence cannot tie. It is assigned inside the same atomic update that changes the price,
so it is the authoritative ordering (§7.2).

---

## 6. Architecture, derived

### Attempt 1: read the price, compare, write

```
price = SELECT current_price FROM auction_state WHERE item_id = ?
if bid > price + increment:
    UPDATE auction_state SET current_price = bid, high_bidder = me
```

The classic lost-update race. Two bidders read a price of $100, both bid $110, both pass the check, both
write. The second write silently overwrites the first: one bidder was told they were winning and is not,
and no record of the conflict exists.

This is **write skew** (Chapter 01, §8) and it is not prevented by snapshot isolation, which is worth
saying because "wrap it in a transaction" is the reflexive fix and at the default isolation level of most
databases it does not work.

### Attempt 2: pessimistic row lock

```sql
BEGIN;
SELECT ... FROM auction_state WHERE item_id = ? FOR UPDATE;
-- evaluate
UPDATE ...;
COMMIT;
```

Correct. Every bid serializes on the row lock, which is exactly what the semantics require.

The cost is that the lock is held across the transaction, and if anything slow happens inside it — a
network call to a fraud service, a notification publish, an index update — the lock duration becomes the
throughput limit:

```
lock held 20 ms → 50 bids/sec maximum on one item
lock held 100 ms → 10 bids/sec — at the closing-seconds rate from §3
```

At 5–10 bids/second in the final seconds this is survivable *only if the critical section is genuinely
tiny*. It also means a slow bid blocks every other bidder on that item, and queued bidders time out and
retry, which makes it worse.

Workable, and the constraint it imposes — **nothing slow inside the lock** — is the real lesson. Move
fraud checks, notifications, and fanout outside it.

### Attempt 3: optimistic concurrency

```sql
UPDATE auction_state
   SET current_price = ?, high_bidder = ?, version = version + 1, sequence = sequence + 1
 WHERE item_id = ? AND version = ?          -- the read version
```

Zero rows updated means someone else won the race; re-read and retry.

No lock is held. Under low contention this is strictly better than attempt 2. Under the closing-second
spike it is worse: with ten concurrent bidders, most retry, and retries collide again. This is the
standard OCC failure — it is optimal when conflicts are rare and pathological when they are the norm — and
an auction's final seconds are exactly the regime where they are the norm.

The lesson is the same as Chapter 80's: **OCC is not an alternative to managing contention, it is what you
use after you have made contention rare.**

### Attempt 4: serialize per item, deliberately

Route every bid for an item to a single **per-item handler** — one goroutine, one thread, one actor, one
partition consumer — and process bids for that item in a loop, one at a time.

```
bid → hash(item_id) → owning process → in-memory queue → sequential evaluation
```

Within that process, there is no concurrency, so there is no race and no lock. The price and high bidder
live in memory. Evaluation is a comparison. Throughput on one item becomes thousands per second rather
than tens, because the serialization is a queue rather than a distributed lock.

This is the same move as Chapter 83's matching engine, and for the same reason: **when a resource must be
serialized anyway, owning it in a single-threaded process is faster and simpler than coordinating access
to it from many.** The concurrency you removed was never doing useful work.

The obvious objection: the in-memory state can be lost. Answer: every accepted bid is written to the
durable `bids` log *before* acknowledging, and the in-memory state is a materialization of that log. A
crashed handler's replacement replays the item's bid log — a few hundred rows — and resumes. State is
recoverable because the log, not the memory, is the source of truth.

### Attempt 5: get the fanout off the write path

The write path now handles bids well and must not also handle 100,000 messages per second per item.
Publish price changes to a stream and let a separate tiered distribution layer deliver them (§7.4).

### Final architecture

```
  BID PATH                                    WATCH PATH (100k/sec/item)

  Client                                      Client
    │ POST /bids                                │ GET /stream (SSE)
    ▼                                           ▼
  API ──► route by hash(item_id)            Edge/fanout tier (many nodes)
           │                                    ▲
           ▼                                    │ each subscribes ONCE
  ┌── Bid handler (owns item) ──┐               │
  │  in-memory: price, high     │          ┌────┴─────┐
  │  bidder, high_max, seq      │          │  Pub/sub │  one message per
  │           │                 │          │  topic   │  price change
  │  1. evaluate (µs)           │          └────▲─────┘
  │  2. append to bids log ─────┼───► Postgres  │
  │  3. update auction_state ───┼───► (durable) │
  │  4. publish price change ───┼───────────────┘
  │  5. ack to client           │
  └──────────┬──────────────────┘
             │ extension check (§7.3)
             ▼
      end_time update ──► scheduled close (Ch. 92) ──► settlement (Ch. 81)
```

The write path is a queue in front of a single-threaded evaluator; the read path is a broadcast tree. They
share only the pub/sub topic, and neither can slow the other down.

---

## 7. Deep dives

### 7.1 Serializing one row, and why the queue beats the lock

Attempts 2 through 4 are three ways to serialize access to one value, and the differences are worth being
precise about because the instinct is to reach for the lock.

| | Pessimistic lock | OCC | Per-item handler |
|---|---|---|---|
| Correct | Yes | Yes | Yes |
| Cost per bid under no contention | A transaction | A conditional update | A queue enqueue |
| Behavior under 10-way contention | Nine bidders block | Nine bidders retry, colliding again | Nine bidders queue, all served in order |
| Throughput on one item | ~1 / lock duration | Degrades with contention | Thousands/sec |
| Where the ordering comes from | Lock acquisition order | Whoever commits first | Queue arrival order |
| Failure mode | Slow operation inside the lock stalls everyone | Retry storm | Handler crash → replay from log |

The per-item handler wins because it **acknowledges that the work is inherently serial and stops paying to
pretend otherwise.** A lock is a mechanism for letting many concurrent workers take turns; if the turns
are the entire work, the concurrency machinery is pure overhead. Converting it into a queue removes the
coordination, removes the retry, and gives a total order for free.

Two costs to state honestly:

**It requires stable routing.** Every bid for item X must reach the process owning X, which means a
consistent-hash routing layer and a plan for what happens during a rebalance. The standard answer is a
short lease per item (Chapter 04, §2) with a fencing token, so that a handler that has lost ownership
cannot write.

**It introduces a single point of failure per item** — but a *cheap* one, because the recovery is a log
replay of a few hundred rows and it affects one item rather than the marketplace.

**Durability ordering matters.** Append to the bid log *before* acknowledging. Acknowledging from memory
and writing asynchronously means a crash can lose an accepted bid — and a bid the user was told they won
with, that then does not exist, is the worst possible outcome. The in-memory state is an optimization; the
log is the record.

### 7.2 Bid ordering, and why client timestamps are inadmissible

Two bids arrive within a millisecond. One wins. The loser will ask why, and the answer must be defensible.

**Client timestamps cannot be used.** Client clocks are wrong by seconds, are trivially forgeable, and
would let anyone win any auction by claiming an earlier time. This is obvious once stated and is worth
stating, because a surprising number of designs accept a client-supplied `bid_time`.

**Server receipt time is necessary but not sufficient.** Millisecond timestamps tie, and under the closing
spike ties are common. Worse, "receipt time" is ambiguous across a fleet — the load balancer's clock, the
API server's clock, and the handler's clock differ.

**The `sequence` counter is the ordering of record.** It is assigned by the single owning handler
(§6, attempt 4) at the moment of evaluation, it is monotonic per item by construction, and it cannot tie.
The timestamp is stored for display and for the extension logic; the sequence decides who was first.

This is a general pattern worth naming: **when order matters and time cannot provide it, manufacture order
at the point of serialization.** Chapter 30 does the same for messages within a conversation, and for the
same reason.

**Network latency asymmetry remains, and cannot be fixed.** A bidder with a 20 ms connection genuinely
beats one with a 200 ms connection when both press at the same instant. No server-side mechanism repairs
this. It is a known property of online auctions and it is one of the reasons close-time extension (§7.3)
exists — extending the auction converts a race decided by network latency into one decided by whether
someone is willing to bid more.

### 7.3 The close, sniping, and automatic extension

**The close is a scheduled event at massive simultaneity.** Fifty-seven auctions close per second at peak
(§3), each requiring: reject further bids, determine the winner, write the outcome, notify both parties,
and hand off to settlement. This is Chapter 92's problem, with the additional constraint that closing late
is a correctness violation rather than an inconvenience.

Two implementation notes. Do not poll a table for `end_time <= now()` across ten million rows; use a
timer structure keyed on the near-term closes, refreshed periodically from the database (Chapter 92,
§6). And make the close **idempotent on `item_id`** — a duplicate close must be a no-op, because the
timer will occasionally fire twice.

**Sniping is the interesting part.** A bidder who waits until the final second to bid denies everyone else
the chance to respond. This is rational individual behavior and it degrades the auction: prices come in
lower, and losing bidders feel — correctly — that they never got to compete.

**Automatic extension (the "going, going, gone" rule) is the standard fix**: any bid within the final N
seconds pushes `end_time` out by N seconds. The auction ends only after N quiet seconds.

This must be a **system requirement, not a policy layer**, and the distinction is the point of the deep
dive. Extension mutates `end_time`, which means:

- The close timer must be **cancellable and reschedulable**, so the timer structure needs an update
  operation rather than only insert-and-fire.
- The extension must be decided **inside the same serialized handler** that accepts the bid (§6, attempt
  4). If extension is evaluated separately, a bid accepted at T-1 second can race the close firing at T.
- The new `end_time` must be **broadcast to watchers**, or their countdowns show an auction ending that
  has not ended.
- There must be a **hard cap** on total extension, or a contested item never closes.

A design that treats extension as a feature to add later will discover that it changes the close
mechanism, the watch protocol, and the concurrency boundary — which is why it belongs in the architecture
from the beginning.

### 7.4 Live price fanout, which is where the capacity goes

From §3: 100,000 messages per second for one popular item, four orders of magnitude above the bid rate.

**The naive design** has each watcher's connection subscribe to the item's updates directly from the
pub/sub system. One hundred thousand subscribers on one topic means the broker delivers the same small
message a hundred thousand times, and brokers are not built for that fan-out ratio.

**Tiered distribution.** Each edge node holds many watcher connections and subscribes to the topic
**once**. A price change is delivered to ~100 edge nodes, each of which writes it to its ~1,000 local
connections. The broker's fan-out is 100, not 100,000; the per-connection write is a local memory
operation and a socket write.

```
price change → topic → 100 edge nodes → 1,000 local SSE connections each
                (100 deliveries)         (100,000 socket writes, parallel)
```

This is the same tiered structure as Chapter 31, and the reason is identical: **broadcast fan-out must be
amortized in a tree, never performed by the origin.**

**Coalescing.** In the final seconds, price changes may arrive faster than a watcher needs to see them. A
watcher does not need every intermediate price — they need the current one. Have the edge node hold the
latest value per item and flush on a ~200 ms tick, dropping superseded updates. This bounds outbound
message rate independently of bid rate, and it is correct because the state is a *value*, not an event
stream — the last one supersedes the rest.

That distinction is worth generalizing: **coalescing is safe when the payload is current state and unsafe
when it is a sequence of events.** Price is state. Bid history is events, and must not be coalesced —
which is why the history is fetched over HTTP rather than pushed.

**Watchers vastly outnumber bidders**, typically by 100:1 or more. Almost all of this capacity serves
people who will never bid. That is worth knowing, because it means the watch path can be degraded during
overload — increase the coalescing interval, drop to polling — without affecting the auction's
correctness at all. Having a designed degradation for the expensive path that does not compromise the
cheap critical one is a good thing to volunteer.

### 7.5 Proxy bidding, where one action becomes many

Most real auction systems do not take a bid amount; they take a **maximum**, and bid on the user's behalf
up to it, one increment at a time.

```
current: $100, high_bidder = A (A's hidden max = $150)
B bids max $130
  → B's max ($130) < A's max ($150), so A stays ahead
  → price rises to min(A_max, B_max + increment) = $131
  → B is immediately outbid without ever leading
```

This is a better user experience — a bidder states their true value once and does not have to sit at the
screen — and it changes the system in three ways worth noting:

**One user action produces a cascade of price changes.** A single bid can move the price several
increments, and each movement is a broadcast to watchers. The fanout amplification of §7.4 is driven by
proxy bids, not by raw bid count.

**The evaluation is no longer a comparison.** It reads the current leader's hidden maximum, compares two
maxima, and computes a new price — which is precisely why `high_max` must live in the same row and the
same serialized handler as the price. Splitting them across services makes the evaluation a distributed
transaction over a hot row, which is the thing the whole design avoids.

**Confidentiality is a correctness property.** If a bidder can learn the current leader's maximum, they
bid exactly one increment above and win at the minimum. `high_max` must never appear in any API response,
any log at an accessible level, or any cached object. This is an unusual case where an access-control
mistake is not a privacy leak but a direct economic exploit, and it is worth flagging as such.

### 7.6 Reserve prices and the settlement handoff

**Reserve** is straightforward and worth one paragraph because it interacts with the close: if the final
price is below the reserve, there is no winner. The reserve must be compared at close, not at each bid,
and it must be hidden from bidders (typically the UI shows "reserve not met" without the number). Storing
it on `items` rather than `auction_state` keeps it out of the hot row.

**Settlement** is Chapter 81's problem, and the handoff is the interesting part. The close writes the
outcome and emits an event; payment happens asynchronously and can fail. When it does, the auction is
already over — you cannot un-close it. The standard resolutions are a second-chance offer to the
next-highest bidder, or relisting. Both require that the **full bid history is retained**, which is the
concrete payoff for the append-only `bids` table in §5.

Use the **outbox pattern** (Chapter 01, §9) for the close-to-settlement handoff: write the auction outcome
and the settlement-initiation event in one transaction. A close that succeeds without triggering payment
leaves an item sold and unpaid, with nothing to reconcile against.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Bid handler crashes mid-auction | That item accepts no bids until reassigned | Short lease with fencing token; replacement replays the item's bid log (a few hundred rows, sub-second) |
| Handler loses ownership but keeps running | Two handlers accept bids for one item, both writing | Fencing token rejected by the storage layer — the lock alone is insufficient (Chapter 04, §2) |
| Close timer fires late | Bids accepted after the deadline | Reject at the handler using server time as authoritative, independent of when the timer fires; the timer triggers settlement, it does not gate acceptance |
| Close timer fires twice | Duplicate settlement | Idempotent close keyed on `item_id` and a status transition guard |
| Fanout tier saturated | Watchers see stale prices; bidding unaffected | Increase coalescing interval; degrade to polling; the write path is isolated so correctness holds |
| Pub/sub outage | No live updates at all | Clients fall back to polling the snapshot endpoint; SSE reconnect with `Last-Event-ID` recovers on restore |
| Extension storm | A contested item never closes | Hard cap on cumulative extension |
| Clock skew between API and handler | Bids near the deadline judged inconsistently | One clock is authoritative — the handler's — and it is the only one that decides acceptance |

**Monitoring:** bid acceptance p99 **split by time-to-close** (the aggregate hides the only interesting
regime, which is the final ten seconds); per-item bid queue depth; handler reassignment rate; close
lateness distribution, which must be tight and is the metric that proves the timer works; fanout message
rate and coalescing drop rate; and the count of bids rejected as `auction_closed` within 100 ms of the
deadline, which is the direct measure of how many users experienced the race.

---

## 9. Common mistakes

1. **Not noticing that contention concentrates on one row.** Chapter 80's inventory shards; an auction's
   price does not, and the design consequences are entirely different.
2. **Read-then-write without concurrency control**, producing lost updates that are invisible in testing
   and produce two users who both believe they are winning.
3. **Reaching for OCC under high contention**, where it degrades into a retry storm — it is the tool for
   *after* contention has been made rare.
4. **Holding a lock across slow work** — a fraud check, a notification, a fanout publish — turning lock
   duration into the throughput ceiling.
5. **Accepting client timestamps for ordering.** Forgeable, wrong, and unnecessary once the serialization
   point can assign a sequence.
6. **Treating close-time extension as a policy detail.** It changes the close mechanism, the concurrency
   boundary, and the watch protocol.
7. **Fanning out price updates from the origin** to 100,000 subscribers rather than through a tier.
8. **Not coalescing price updates**, delivering every intermediate value when only the latest matters.
9. **Exposing the leader's hidden maximum** anywhere — an access-control slip that is a direct economic
   exploit rather than a privacy issue.
10. **Designing only the bid path** and ignoring that the watch path is four orders of magnitude larger.

---

## 10. Variants

**Flash sale / limited-inventory drop.** Chapter 80's shape rather than this one: many identical units, so
the contention is on a counter that *can* be sharded (decrement one of N sub-counters, sum for display).
The single-hot-row problem disappears, which is why a flash sale is easier than an auction despite feeling
harder.

**Reverse auction / procurement.** Sellers bid downward. Structurally identical with the comparison
inverted; the interesting difference is that bids are typically sealed until close, which removes the
entire live-fanout problem — the most expensive part of this chapter — and replaces it with a
confidentiality requirement.

**Sealed-bid and Vickrey auctions.** No live price, no fanout, no sniping, no extension. Bids are
encrypted or access-controlled until close, at which point they are all evaluated at once. Almost every
deep dive in this chapter evaporates; what remains is confidentiality and a correct close. A good
illustration that the auction *format* determines the system, not the word "auction".

**Ad exchange real-time bidding.** Superficially similar and completely different: the auction lasts ~100
milliseconds, there are no human bidders, no watchers, and no extension — but there are hundreds of
thousands of auctions per second. The contention problem vanishes (each auction has one round) and is
replaced by a hard real-time latency budget. Chapter 70's shape, not this one's.

**Ticket resale with dynamic pricing.** A hybrid: inventory like Chapter 80, price movement like this
chapter, and the fanout problem of watchers tracking a price that changes without anyone bidding.

---

## 11. Further reading

- Chapter 80, for the contention machinery and the two-layer guard; Chapter 83, for the single-threaded
  deterministic evaluator taken further; Chapter 92, for scheduled close at simultaneity
- Chapter 31, for the tiered broadcast fanout, and Chapter 02, §8, for why SSE rather than WebSocket
- Chapter 81, for the settlement handoff and the outbox pattern in its natural home
- Roth and Ockenfels, "Last-Minute Bidding and the Rules for Ending Second-Price Auctions" (AER, 2002) —
  the empirical study of sniping that motivates §7.3's extension rule, and the clearest evidence that the
  closing rule changes bidder behavior rather than merely accommodating it
