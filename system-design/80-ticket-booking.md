# Chapter 80 — Ticket Booking (Ticketmaster)

> **Prerequisites:** Chapters 01 (§8 transactions, write skew, OCC vs pessimistic, sagas), 02 (caching, CDN), 03 (idempotency), 04 (§2 distributed locks and fencing)
> **Patterns:** admission control, two-layer guard, optimistic concurrency, time-limited holds, saga compensation

---

## 1. The problem

A venue has a fixed number of seats. Users browse events, open a seat map, pick specific seats, hold them
while entering payment details, and buy. When a popular artist goes on sale at 10:00 a.m., every fan in the
country is on the page at 09:59:59. The functional description is boring and the workload is not. Ticketing
is one of the few consumer products where demand exceeds supply by more than an order of magnitude, the
supply is **non-fungible** (seat 14A is not 14B), and being wrong is not a UX blemish but a refund, a
lawsuit, and a person standing outside a stadium holding a valid-looking ticket for an occupied seat.

State the problem in these words, early, because it is the framing that earns credit:

> **Correctly handling high contention on limited inventory under massive concurrency, without overselling,
> while keeping latency acceptable.**

Every clause is load-bearing. *Correctly* rules out designs that are merely fast. *High contention on
limited inventory* separates this from every read-heavy system in Part II. *Massive concurrency* is why the
obviously correct design — one transaction, one lock, one seat — does not survive. *Latency acceptable*
forbids serializing everything through one writer and calling it solved.

**The property that makes it hard:** the read load and the write load differ by four orders of magnitude
and point at the same tiny set of rows. A million people want fifty thousand seats. The system must serve a
million concurrent readers of a rapidly changing seat map while performing eighty correct, mutually
exclusive writes per second against rows everyone is fighting over. These are not two features of one
system. They are two systems, and the first structural decision is to stop pretending otherwise.

---

## 2. Requirements

### Functional

1. Browse and search events; view event details.
2. View a seat map showing which seats are available.
3. Select seats and receive a **time-limited hold**.
4. Complete a purchase against a hold, producing an order and a ticket.
5. Release the hold — explicitly, or by expiry.

Defer, but name: resale and transfer, dynamic pricing, "best available" recommendation, refunds, presale
codes, delivery. Ask one clarifying question: **are seats individually chosen, or is this general admission
where only a count matters?** The two have different correctness machinery — a per-seat unique constraint
versus a decrementing counter (§7.7). Assume reserved seating; it is harder and more commonly intended.

### Non-functional

- **No overselling. Ever.** A hard invariant, not a target with a percentile attached. Say it out loud; it
  is why a database constraint appears later, and a candidate who never states the invariant cannot be
  credited with enforcing it.
- **Latency** — hold p99 under 500 ms; purchase p99 under 2 s, including a payment round trip you do not
  control; browse p99 under 200 ms, mostly from CDN.
- **Browse availability** — must survive the spike; a stale seat map is fine, a blank page is a public
  event. **Booking consistency** — strong, linearizable per seat. State the asymmetry explicitly: **the seat
  map is allowed to be stale and the booking is not.** The whole design rests on it.
- **Hold duration** — 10 minutes: long enough to type a card number, short enough that inventory does not
  idle. A tunable product parameter, not a constant. **Fairness** — arrival order should roughly determine
  service order, which is what motivates the waiting room (§7.3).
- **Idempotency** — every mutating call safely retryable. Users double-tap; clients retry on timeout.

### Explicitly out of scope

Payment internals (Chapter 81), fraud detection beyond admission control, barcode rotation and delivery,
promoter-facing event creation.

---

## 3. Estimation

Take a single stadium show as the unit; the aggregate is not what breaks. The on-sale time is published, so
everyone refreshes at once.

```
one arena show   = 50,000 seats;  demand ≈ 1,000,000 people  →  demand : supply = 20 : 1
arrivals in ~1 s ≈ 1,000,000 clients × (page + map + polling) ≈ 1,000,000+ requests/sec
writes that must succeed: 50,000 / 600 s (a ten-minute sellout) ≈ 83 bookings/sec
```

Eighty-three writes per second is something a single Postgres instance does without noticing. **The
constraining number is the ratio: `1,000,000/s : 83/s ≈ 12,000 : 1`.** One undivided system would size a
strongly consistent transactional store for a million requests per second in order to perform eighty-three
useful writes. The correct response is a cheap, cacheable, eventually consistent read system absorbing
99.99% of traffic, and a small, strongly consistent write system that never sees more than a few thousand
requests per second because something upstream refuses to let more in.

**The contention arithmetic — the second number worth deriving.** If a million arrivals each pick a seat
roughly uniformly from 50,000:

```
expected contenders per seat = 1,000,000 / 50,000 = 20
P(an attempt conflicts) = 1 - (1 - 1/50,000)^999,999 ≈ 1 - e^-20 ≈ 0.999999998
```

**Conflicts are not rare. Conflicts are certain.** Nineteen of twenty attempts on a seat must fail — hold
that for §7.2, where optimistic concurrency control, whose entire justification is that conflicts are rare,
has to be argued for anyway. **Payload:** `50,000 seats × ~40 B ≈ 2 MB` raw, or ~100 KB compressed with
status as a bitmap; times a million clients, 100 GB delivered in the first seconds. A CDN problem, not an
application-server problem — the second reason browse is a different system. Durable storage (~3.6 TB over
five years) is never the interesting constraint; contention is.

---

## 4. API

```
GET  /events/{eventId}
  ->   200 {name, venue, onSaleAt, priceTiers[]}              # CDN, TTL 60 s
GET  /events/{eventId}/seatmap
  ->   200 {version, seats:[{seatId, section, row, tier}]}    # immutable geometry, TTL 1 day
GET  /events/{eventId}/availability?since=<version>
  ->   200 {version, changed:[{seatId, status}]}              # delta, TTL 2 s

POST /queue/{eventId}/enter
  ->   200 {waitToken, position, estimatedWaitSeconds}
  ->   200 {admissionToken, expiresAt}                        # once admitted

POST /events/{eventId}/holds
  headers: X-Admission-Token, Idempotency-Key
  body:  {seatIds: ["A-14-3","A-14-4"]}
  ->     201 {holdId, seatIds, expiresAt}      ->  409 {conflictingSeatIds}
DELETE /holds/{holdId}                                        -> 204

POST /orders
  headers: Idempotency-Key
  body:  {holdId, paymentMethodToken}
  ->     201 {orderId, status:"confirmed", tickets[]}
  ->     200 {orderId, ...}      # replay: the STORED result of the first attempt
  ->     410 if the hold expired
```

**The seat map is split into immutable geometry and mutable availability.** Which seats exist and where they
sit never changes, so it is a static asset a CDN serves for free; availability changes constantly, so it is
a separate, tiny endpoint returning a **delta since a version**. One combined document would be entirely
uncacheable, because a single seat sold invalidates 2 MB. Highest-leverage API decision in the chapter, and
it costs nothing.

**`POST /holds` returns 409 with the conflicting seat IDs**, not a bare failure. Losing a race is the common
case — nineteen times in twenty — so the error path is a primary path and must carry enough for the client
to re-render and offer alternatives immediately.

**Both mutating calls carry an `Idempotency-Key`,** and a retried purchase returns the **stored result**,
not a fresh success (Chapter 81 §7.1); a retry that creates a second order has charged a card twice for one
seat. **Admission tokens gate booking and are absent from browse** — without one, anyone who bookmarks the
hold endpoint bypasses admission control, and so does every bot.

---

## 5. Data model

```
events(event_id PK, venue_id, on_sale_at, status)

seats
  event_id UUID PK part 1                     -- partition key
  seat_id  TEXT PK part 2                     -- "A-14-3"
  section, row, number, price_tier
  status          ENUM(available, held, sold)
  held_by         UUID NULL                   -- hold_id
  hold_expires_at TIMESTAMP NULL
  version         BIGINT NOT NULL DEFAULT 0   -- the OCC guard

holds(hold_id PK, event_id, user_id, seat_ids[], expires_at,
      status ENUM(active, converted, released, expired))

orders(order_id PK, idempotency_key UNIQUE, user_id, event_id, hold_id,
       status ENUM(pending, paid, failed, cancelled), total_cents, created_at)

order_seats                                   -- THE constraint
  event_id UUID, seat_id TEXT, order_id UUID
  PRIMARY KEY (event_id, seat_id)             -- one row per seat, ever
```

**`order_seats` is where overselling is made impossible** — not in the application, not in Redis, not in a
careful sequence of checks, but in a uniqueness constraint the database enforces on every insert regardless
of what any server believes. Two concurrent purchases of A-14-3 both attempt the insert; exactly one
succeeds, and how they got there does not matter. This row is the guarantee; everything else in the chapter
only reduces how often it has to fire.

**`seats.version` is the same guarantee in a different spelling,** used for the hold transition rather than
the sale: every seat write is `UPDATE seats SET ..., version = version + 1 WHERE event_id=? AND seat_id=?
AND version=?`, and zero affected rows means someone moved first. Version-column OCC and a unique key are two
encodings of one idea — *let the storage engine adjudicate the race*. Holds change an existing row, so a
version fits; sales create a fact that must never duplicate, so a unique key fits.

**Seat state is denormalized onto `seats` rather than joined from holds and orders,** because the seat map
read must be a single-partition scan; the cost is that `seats.status` can disagree with the authoritative
tables after a crash, which §7.4 handles. **Partition by `event_id`**, since all contention and all queries
live within one event. This is Chapter 01 §8's "co-locate the invariant": everything requiring atomicity is
in one partition of one database, so no distributed transaction is needed on the inventory side. One
on-sale event *is* a hot partition by construction — addressed by capping what reaches it (§7.3).

---

## 6. Architecture, derived

### Attempt 1: one transaction per booking

```
BEGIN;
  SELECT status FROM seats WHERE event_id=? AND seat_id IN (...) FOR UPDATE;
  UPDATE seats SET status='sold' WHERE ...;
  INSERT INTO order_seats ...;
COMMIT;
```

Serializable, no overselling, twelve lines. It is right, and it breaks in two quantifiable ways.

**It breaks at the connection pool, around 5,000 concurrent requests.** Postgres handles a few hundred to a
couple thousand active connections before lock-manager and context-switch overhead dominate; a pooler raises
the ceiling but each in-flight transaction still holds a server slot. A million requests against a pool of
2,000 leaves 998,000 queued, timing out, and retried — which doubles arriving load. The failure is a retry
storm, not graceful degradation.

**It breaks at the row lock, much earlier.** `FOR UPDATE` serializes contenders behind the holder; twenty
contenders on a 5 ms transaction means the last waits 100 ms, which is tolerable — but the naive
implementation holds the seat locked while the user types a card number. **Holding a database transaction
open across a human interaction is the canonical mistake in this problem**; it turns a 5 ms lock into a
3-minute lock, and twenty contenders then queue for an hour. Hence the central structural idea: **the hold
must be a committed state change, not an open transaction.** Reserve, commit, return — the transaction ends
and the reservation persists.

### Attempt 2: a Redis lock as the hold

The hold is short-lived, mutually exclusive, and self-expiring, so a distributed lock with a TTL looks like
exactly the right primitive:

```
SET seat:{event}:{seat} {holdId} NX PX 600000     -- 10 minutes
```

`NX` makes acquisition atomic; `PX` makes expiry automatic, so an abandoned checkout self-heals with no
sweeper and no timer. Redis absorbs this trivially — hundreds of thousands of ops/sec, sub-millisecond — and
losers get their 409 in a millisecond instead of queuing on a row lock. This is most of the answer, and it is
where many candidates stop. On its own it is **wrong**, and this chapter will be blunt about it, because
interviewers fail people on exactly this point. Per Chapter 04 §2:

- **Failover loses the lock.** Redis replication is asynchronous. A primary accepts `SET NX` for A-14-3, has
  not shipped it to the replica, and dies. The replica is promoted without the key, and a second client
  acquires the same seat cleanly from a healthy-looking cluster.
- **A paused process outlives its TTL.** An instance stalls eleven minutes on GC or a hypervisor migration,
  wakes believing it holds the seat, and writes the sale. The lock told it the truth when it asked; the
  truth changed while it was not looking.
- **Redis is a cache and gets treated like one** — flushed, evicted under memory pressure, resized. Each
  silently removes the only thing preventing a double sale.

The general statement, which transfers to Chapters 50 and 81: **a lock alone cannot make a system correct;
the resource must participate.**

### Attempt 3: the two-layer guard — the answer

Keep the Redis lock. Put the constraint underneath it.

```
1. Redis:  SET seat:{event}:{seat} {holdId} NX PX 600000
           -> fails for ~19 of 20 contenders in <1 ms, no database round trip

2. Postgres, only for the winner:
           UPDATE seats SET status='held', held_by=?, hold_expires_at=now()+'10 min',
                            version = version + 1
            WHERE event_id=? AND seat_id=? AND version=? AND status='available';
           -> 0 rows  =>  lost anyway; release the Redis key, return 409

3. At purchase, one transaction:
           INSERT INTO order_seats (event_id, seat_id, order_id) VALUES (...);
           -- PK violation => already sold. Abort before capture (§7.5).
           UPDATE seats SET status='sold' WHERE ... AND version=?;
```

**The lock is an optimization. The constraint is the guarantee.** Say that sentence, in roughly those words.
It is the load-bearing claim of the chapter and the thing being tested.

| Layer | Buys you | Does not buy you |
|---|---|---|
| Redis `SET NX PX` | Rejecting ~95% of contenders in <1 ms with no DB round trip; automatic hold expiry | Correctness. It loses keys and it lies to paused processes. |
| DB version / unique key | Absolute correctness under every failure mode, including ones you did not enumerate | Throughput. It drowns under a million requests. |

They compose because they fail in opposite directions: the fast layer fails by being occasionally too
permissive, and the slow layer catches exactly that.

### Attempt 4: admission control — the virtual waiting room

The guard is correct, but 950,000 rejected contenders per second still arrive, still consume TLS handshakes,
application threads, and a Redis round trip each. Redis survives; the gateway, auth, mesh, and logging around
it do not, and a million disappointed clients retrying turns one spike into sustained overload.

**Put a queue in front of the booking path and admit a bounded number of users.** Clients hit
`POST /queue/{event}/enter`, get a position, and wait; an admission service releases N users per second with
signed, short-lived tokens, and only token holders may call `POST /holds`. Size N from what you protect — if
the booking path sustains 2,000 rps and each admitted user makes ~5 calls over a 3-minute session:

```
2,000 rps / 5 calls   ≈ 400 sessions admitted/sec
concurrent sessions   ≈ 400 × 180 s ≈ 72,000 in-flight users
drain time            ≈ 1,000,000 / 400 ≈ 2,500 s ≈ 42 minutes
```

Forty-two minutes is a long wait and it is the honest answer: 50,000 seats, a million people, so 95% were
never getting one. **The queue does not create the disappointment; it makes it survivable** — the users at
the front get a fast, working site instead of everyone getting a site that is down. That is the difference
between a degraded site and a dead one, and it is why this component exists in every real ticketing system.
It also does what §7.2 depends on: with 72,000 active users instead of a million, contenders per seat fall
from 20 to about 1.4.

### Attempt 5: separate the browse path completely

The queue protects writes and must not protect reads — a user who cannot see the event page cannot decide to
join the queue. Browse is a different stack: static event and geometry documents at the CDN edge, and an
availability delta with a 1–5 second TTL from a replica. A seat map five seconds stale shows seats just
taken; the user clicks one and gets a 409. **That 409 is the design working, not failing** — the
authoritative check happens at hold time anyway.

### Final architecture

```
                        ┌──────────── CDN ────────────┐
  1,000,000 browsers ───┤ /events/{id}   TTL 60 s      │
     (browse path)      │ /seatmap       TTL 1 day     │ immutable geometry
                        │ /availability  TTL 2 s       │ delta by version
                        └──────────────┬───────────────┘
                                       ▼ misses      ▲ async invalidation
                        Read replicas / availability cache
  ─────────────────────────────────────┼──────────────────────────────────────
     (booking path)                    │
  Client ─► Waiting room ─► admission token ─► Gateway (token required)
             │ Redis ZSET: position               │
             │ releases N/sec                     ▼
             └───────────────────────────►   Hold service
                                                   │
                        ┌──────────────────────────┴───────────────────┐
                        ▼                                              ▼
         Redis SET seat:{e}:{s} NX PX 600s        Postgres (partition per event_id)
         absorbs ~95% of contention                 seats.version  — OCC on hold
                        │                           order_seats PK — the guarantee
                        ▼                                              ▲
                  Order service ──► Payment (Ch. 81) ───────────────────┘
                        │            saga: authorize → capture
                        ▼
                  Hold sweeper (expired holds → available)
```

Two stacks, two consistency models, one shared truth. The seam between them is the availability cache, and
it is deliberately allowed to be wrong.

---

## 7. Deep dives

### 7.1 The two-layer guard, stated properly

The naive framing is "use a distributed lock so two people cannot book the same seat." The precise framing:

> **The Redis lock is a contention filter. The database constraint is the correctness boundary. Removing the
> lock makes the system slow; removing the constraint makes the system wrong.**

*Redis loses a key.* Two clients believe they hold A-14-3. The first `UPDATE ... WHERE version=?` succeeds;
the second affects zero rows and returns 409. If both somehow got past the hold, `INSERT INTO order_seats`
violates the primary key for one, and that purchase fails before money moves. **No overselling** — the user
sees a late, ugly error, which is the correct price. *Database only:* correct, and it collapses at the
connection pool, because every one of 950,000 losers costs a transaction. *Redis only:* fast, and it
oversells during exactly the failure modes that occur during a high-profile on-sale, when infrastructure is
most stressed and most likely to be resized — which is what a candidate offering only the lock is signing up
for, and the interviewer knows it.

A third option deserves naming and rejecting: **fencing tokens** (Chapter 04 §2), where the lock service
issues a monotonically increasing token and the resource rejects any lower one. Strictly correct, and
unnecessary here, because the version column already *is* a per-row fencing token — `version = ?` rejects
any writer working from stale knowledge, which is exactly what a fence does. Recognizing they are the same
mechanism is worth saying; adding a token service on top is not.

**The cost** is a small class of phantom-unavailable seats: locked in Redis by a client that crashed before
the database write, appearing taken for up to the TTL — inventory idle for at most ten minutes, strictly
better than a double sale. Shrink it by releasing the key when the update returns zero rows.

### 7.2 Optimistic versus pessimistic, and why the queue changes the answer

Chapter 01 §8 gives the rule: pessimistic locking when conflicts are common, OCC when they are rare, and OCC
under heavy conflict is *worse* than locking, because every loser does its work before discovering it was
wasted and then retries. Apply that to the raw numbers and OCC loses badly — twenty contenders per seat is a
95% abort rate and a retry storm. Apply it after the waiting room and it flips:

```
without admission control: 1,000,000 active / 50,000 seats = 20.0 contenders/seat
with admission control:       72,000 active / 50,000 seats ≈  1.44 contenders/seat
P(conflict) ≈ 1 - e^-0.44 ≈ 0.36 at the start of the sale, falling as users spread out
```

**This is the argument, and it must be made explicitly:** the queue and the hold do not merely protect the
database from load, they *change the concurrency regime*. They convert a problem where conflicts are the
norm — where pessimism would be correct and also unusably slow — into one where conflicts are a minority
case, which is precisely the regime OCC was built for. Admission control is not a capacity bolt-on; it is
the precondition that makes the concurrency-control choice valid.

The hold does the other half. `SELECT ... FOR UPDATE` is untenable not because locks are slow but because
the natural critical section spans a human; the hold **replaces a long lock with a short transaction plus a
committed timestamp**, so the database is locked for milliseconds while the seat is reserved for ten.

One more distinction. "No two orders contain the same seat" is a **write skew** shape (Chapter 01 §8): two
transactions each read that a seat looks free, each check an invariant that holds, and each write. At
snapshot isolation — the default in Postgres's `REPEATABLE READ` and many managed databases — both commit
and the invariant breaks. What saves you is expressing the constraint as **uniqueness of a row both
transactions must write**, turning an invisible anomaly into a plain key collision every isolation level
detects. Converting write skew into a key collision is the most reliable fix for this class of bug.

### 7.3 The virtual waiting room

Cap downstream concurrency, roughly preserve arrival order, be visible so users wait instead of hammering
refresh, and be hard to bypass. **Mechanism:** on arrival the client gets a signed wait token and the service
does `ZADD queue:{event} {arrival_ts} {token}`, with position from `ZRANK`. An admission worker runs a leaky
bucket — every 100 ms it pops the lowest-ranked `N/10` entries, mints a short-lived signed admission token
(event, user, 3-minute expiry), and pushes it to the client over SSE or long polling (Chapter 02 §8); the
gateway rejects booking requests without one. **Size N from measured downstream health** — booking p99 and
database CPU — rather than a constant, so a slow payment processor automatically throttles admissions instead
of building a backlog. That is a token bucket with a dynamically adjusted refill rate (Chapter 04 §3).

**Fairness is underspecified, and noticing that scores points.** Strict FIFO by arrival rewards whoever has
the lowest network latency and whoever wrote a script; several real systems instead run a **randomized draw
among everyone who registered during a window**, which feels less just and is much better for bot resistance,
because it removes the value of arriving 40 ms earlier. Bypass resistance is also why the token exists at
all: rate limits alone fail against thousands of residential IPs, but a signed, single-event, short-lived
token bound to an authenticated account means scaling an attack requires scaling accounts — a fight you can
win with account age, payment history, and device attestation. **Cost:** a new critical dependency in front
of revenue, so give it a degraded mode (admit everyone under a hard global rate limit) rather than an
outage.

### 7.4 Hold expiry: a TTL is not reconciliation

The Redis key expires by itself. That is what makes Redis attractive here, and it is a trap, because
**expiring the key does not change the database row.** After the TTL fires, `seats.status` still reads `held`
and `hold_expires_at` is in the past. Nothing has reconciled them.

**Lazy validation** is the correctness mechanism: any read or write that consults a seat treats
`status='held' AND hold_expires_at < now()` as available — `WHERE status = 'available' OR (status = 'held'
AND hold_expires_at < now())`. An expired hold is then functionally released the instant it expires, with no job, no timer, and no window
in which a seat is wrongly unavailable. **A sweeper** is housekeeping on top: every 30 seconds, reset expired
rows to `available`. Not needed for correctness once lazy validation exists, and still worth running, because
the hot seat-map read is faster against a plain indexed `status` than a disjunction with a timestamp
comparison. Make it version-guarded so a sweeper racing a live purchase loses.

**Do not schedule a per-hold timer** (Chapter 92's machinery). At 83 holds/sec per event that is a large
number of timers whose only job is to undo what lazy validation already handles, and each is a chance to
release a hold that converted to a sale a millisecond earlier. The general principle, recurring in Chapter
33's presence TTLs: **when state lives in two stores with different expiry semantics, the durable store must
be able to compute the truth without the volatile one.**

### 7.5 Payment failure, compensation, and idempotency

The purchase crosses a boundary you do not control: hold (yours) → authorize (processor) → order (yours) →
capture (processor). Two-phase commit is unavailable — it requires every participant to implement a prepare
phase and hold locks awaiting a coordinator, and a processor offers neither, only an HTTP call that may time
out leaving you ignorant of the outcome. The alternative is a **saga** (Chapter 01 §8):

```
1 acquire hold          compensate: release hold (Redis DEL + DB reset)
2 authorize payment     compensate: void the authorization
3 insert order + seats  compensate: cancel order, release seats
4 capture payment       compensate: refund — real, visible, slow
```

**Authorize before writing the order, capture after.** An authorization holds funds and is voided cheaply and
invisibly; a capture moves money and is undone only by a refund the customer sees and that costs fees. Order
the saga so the irreversible step is last, and every earlier failure compensates cleanly — a general rule:
**put the least reversible step last.**

**Everything is keyed by the order ID and everything is idempotent.** The idempotency key has a unique index
on `orders`; a retry returns the stored order, and it must return the **stored result** — order ID and
tickets — not a bare 200, because a client that retried a timed-out purchase and received `{"ok":true}` has
no order ID, cannot show a ticket, and will try again. The seat-level guarantee survives any bug here
anyway, because `order_seats` admits one row per seat.

**On payment failure, release the hold immediately** — compare-and-delete on the Redis key using the hold ID
as the value (Chapter 04 §2, so a late release cannot free someone else's hold) plus a version-guarded reset.
Do not wait for the TTL: declines run at several percent of attempts, and ten idle minutes per decline is
real lost revenue. **On a capture timeout you do not know the outcome** — do not guess, and do not blindly
retry a non-idempotent endpoint. Mark the order `pending`, keep the seats, and resolve asynchronously by
querying the processor for your idempotency key; Chapter 81 §7.5's reconciliation is the durable answer.

### 7.6 Serving the seat map to a million people

Polling the full map is 1,000,000 × 100 KB every 5 s = 20 GB/s, rejected on arithmetic alone. **Polling a
delta** works: geometry is fetched once from the CDN, then `/availability?since=v1043` returns only changes,
which are bounded by the real booking rate — 83 sales/sec plus holds and releases is perhaps 500 seats per
5-second window, about 5 KB. A million clients cost ≈ 1 GB/s, and every response is *identical*, so a
2-second edge TTL collapses a million origin requests into one per edge. This works only because §4 split
geometry from availability. **Pushing over SSE** is cheaper still but costs a million connections (Chapter
31) — not worth a dedicated tier for a burst lasting minutes, though the waiting room already needs a push
channel, so reuse those connections if they exist. Either way the map is stale and users click gone seats:
gray the seat optimistically, then confirm or revert on the hold response.

### 7.7 General admission: fungible inventory is a different problem

If the product sells "3 tickets" rather than named seats, the per-seat unique constraint has nothing to be
unique on, and the invariant becomes `sold <= capacity`. The tempting implementation is
`DECRBY tickets:{event} 3`, selling if the result is non-negative — atomic, no read-modify-write race,
enormous throughput. And again it is not the correctness boundary: a failover replays a stale counter and
oversells by whatever happened in the replication gap. The durable guarantee is a conditional update:

```sql
UPDATE inventory SET sold = sold + 3
 WHERE event_id = ? AND sold + 3 <= capacity;     -- 0 rows => sold out
```

All writes for one event serialize on one row, and Postgres does a few thousand single-row conditional
updates per second, far above the 83/sec the business needs. If it were not — a flash sale of a million units
in a minute — **shard the counter** into 50 buckets of 1,000, route randomly, and fall back when one empties;
that trades exact sold-out detection for throughput, and the last few units become hard to find. The
structural point: reserved seating and general admission share the whole architecture — waiting room, hold,
two-layer guard, saga — and differ only in how the constraint is spelled.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Redis lock cluster down | Hold acquisition falls through to the database | Degrade to DB-only holds under a strict gateway rate limit; slower but still correct. Never fail open on the constraint. |
| Redis failover loses keys | Two clients believe they hold one seat | Version-guarded `UPDATE` rejects one at hold time; `order_seats` PK rejects one at purchase. No overselling by construction. |
| Booking DB primary fails | Holds and purchases fail; browse unaffected | Synchronous replica with automatic failover; browse shares nothing with it, so the site stays up and the queue holds users |
| Waiting room down | Nobody is admitted; revenue stops | Degraded mode: admit everyone under a hard global rate limit; alert loudly |
| Payment processor slow | Holds convert slowly; seats sit reserved | Extend hold TTL when the processor is degraded; throttle admission from the same signal |
| Capture times out | Order stuck `pending`, seats held | Resolve asynchronously by idempotency key; reconcile against the settlement file (Ch. 81 §7.5) |
| Availability cache stale | Users click seats that are gone | 409 with conflicting IDs; client re-renders. The 409 rate is the staleness proxy. |
| Bot fleet drains inventory | Real users get nothing | Tokens bound to authenticated accounts; randomized draw over a registration window; account-age signals |

**Monitoring:** hold 409 rate (the contention signal — a spike means a stale cache or a scanning bot);
**Redis-succeeded-but-DB-rejected count, which should be near zero, and any sustained value is the leading
indicator of an overselling incident**; waiting-room depth and admission rate; hold-to-purchase conversion;
expired-hold rate; booking p99; and seats sold versus capacity per event, checked continuously, because its
only acceptable violation count is zero.

---

## 9. Common mistakes

1. **Offering a Redis lock as the whole answer.** It is fast, it is the right first move, and it is not a
   correctness mechanism. Without a database constraint underneath, the system oversells during exactly the
   failure that occurs during a high-profile sale. This is the most common way to fail this question.
2. **Holding a database transaction open across checkout.** Turns a 5 ms lock into a 3-minute lock. The hold
   must be committed state, not an open transaction.
3. **No admission control.** A design without a waiting room implicitly claims it can serve a million
   requests per second against a strongly consistent store. None can, and capping what enters is what makes
   the rest of the design's assumptions true (§7.2).
4. **Assuming OCC is fine without checking the conflict rate.** At 20 contenders per seat it is the wrong
   tool; the design must *earn* the low conflict rate before it may use OCC.
5. **Believing the Redis TTL releases the seat.** It releases the key. The row still says `held` until lazy
   validation or a sweeper says otherwise, and a design with neither leaks inventory permanently.
6. **A non-idempotent purchase**, or **not releasing the hold on payment failure.** A retry that creates a
   second order and a second charge is the worst outcome the system can produce, and mobile clients retry
   constantly; a decline left to expire idles inventory during the window it is most valuable.
7. **Treating browse and booking as one system**, and so sizing a transactional database for a million
   reads per second, or embedding availability in the seat map and making it uncacheable.
8. **Reaching for two-phase commit** across the payment processor. It is not available to you, and the saga
   is not a workaround but the right answer for a workflow spanning services you do not own.

---

## 10. Variants

**Airline seats.** Nearly identical, plus two twists: inventory is distributed across channels (direct, GDS,
partner airlines) so the authoritative record is not solely yours, and overbooking is *deliberate policy* —
the constraint becomes `sold <= capacity × overbook_factor`, and the interesting engineering moves to the
compensation workflow at the gate.

**Restaurant reservations.** A table × time-slot grid where adjacent slots interact: a 7:00 booking for two
hours consumes the 8:00 slot. The constraint becomes an exclusion over overlapping ranges, which Postgres
expresses directly on a `tstzrange` — the same idea one level more general.

**Flash sales and limited drops.** General admission (§7.7) at maximum contention, with an adversarial bot
population that *is* the engineering problem; the waiting room becomes the centerpiece rather than support.

**Hotel rooms.** A room-type × date grid where a five-night stay must acquire five consecutive nights
atomically, so the constraint spans rows — the one variant that strains single-partition atomicity; keep an
entire property in one partition. **Parking and appointment slots** are the reduced form: fungible units per
slot, low contention, no payment on the critical path.

---

## 11. Further reading

- Chapter 01 §8 (isolation, write skew, OCC vs pessimistic, sagas); Chapter 04 §2 (distributed locks,
  compare-and-delete release, fencing tokens); Chapter 81 (payment saga, idempotency, reconciliation);
  Chapter 82 (the same contention machinery applied to auctions)
- Martin Kleppmann, *Designing Data-Intensive Applications*, chapter 7 — the definitive treatment of write
  skew and of what serializability actually buys
- [Martin Kleppmann, "How to do distributed locking"](https://martin.kleppmann.com/2016/02/08/how-to-do-distributed-locking.html)
  — the fencing-token argument, and the reason this chapter refuses to let Redis be the guarantee
- [Redis — Distributed Locks with Redlock](https://redis.io/docs/latest/develop/use-cases/patterns/distributed-locks/),
  including its own discussion of the safety limits
- [HelloInterview — Design Ticketmaster](https://www.hellointerview.com/learn/system-design/problem-breakdowns/ticketmaster)
