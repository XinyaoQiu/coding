# Chapter 81 — Payment System (Stripe-like)

> **Prerequisites:** Chapters 01 (§8 transactions and sagas, §9 outbox), 03 (§3 delivery semantics, §4 idempotency, §5 retries), 04 (§7 observability)
> **Patterns:** idempotency keys, append-only double-entry ledger, saga compensation, reconciliation, at-least-once webhooks

---

## 1. The problem

A merchant integrates an API. A customer enters a card. Money leaves the customer's account, a fee is
retained, and the remainder arrives in the merchant's bank days later, through a chain of parties —
acquirer, card network, issuer — that you do not own and cannot inspect.

Every other chapter in Part II tolerates being a little wrong: a stale timeline, an approximate top-K, a
lost click event. **Here, being wrong means somebody's money is gone**, and there is no cache to
invalidate, no rebuild path, and no apology that makes the ledger balance again. Correctness is not one
non-functional requirement among several; it is the requirement, and everything else is subordinate.

**The property that makes it hard:** the authoritative record of what happened lives partly in systems you
do not control, reached by network calls that are neither atomic nor observable on failure. When a capture
times out, the money may have moved or it may not, and no amount of local rigor tells you which. You cannot
make that uncertainty go away. You can only build a system that **records every intent before acting, never
destroys evidence, and reconciles continuously against the outside world** — which is why the three central
ideas here are the idempotency key, the append-only ledger, and the reconciliation job, in that order.

---

## 2. Requirements

### Functional

1. Create a payment: charge a customer's payment method for a given amount.
2. **Authorize** (reserve funds) and **capture** (move them) as separate steps.
3. Refund a payment, fully or partially.
4. Report a merchant's balance and the transactions composing it.
5. Deliver **webhooks** to the merchant for every state change, and pay out their balance on a schedule.

Defer, but name: subscriptions, fraud scoring, disputes and chargebacks, marketplace splits, and payment
methods other than cards.

### Non-functional

- **Correctness, expressed as invariants** rather than percentiles: no double charge; no captured payment
  missing from the ledger; every ledger transaction sums to zero; every processor-side movement has a
  matching local record and vice versa. These are continuously checkable, and they should be checked.
- **Auditability** — for any balance, produce the complete ordered list of events that produced it, for
  seven years. This requirement alone eliminates a mutable balance column (§7.2).
- **Idempotency** — every mutating endpoint is safely retryable, and a retry returns the *original result*.
- **Availability** — 99.99% on the authorization path; merchants lose revenue per second of downtime.
  **Latency** — p99 under 1 s, dominated by the processor's own 300–800 ms, so your budget is the 100–200
  ms around it and the design must add no synchronous work.
- **Consistency** — strong within your ledger, eventually consistent with the processor. The gap between
  the two is where reconciliation lives (§7.5).

### Explicitly out of scope

Card network and issuer internals, KYC and merchant onboarding, the fraud model itself (this chapter covers
where it plugs in, not how it scores), and the dispute-evidence interface.

---

## 3. Estimation

Assume 100 million payments per day.

`100M / 86,400 s ≈ 1,160 payments/sec average; peak (Black Friday, ~5×) ≈ 6,000/sec.` Six thousand
authorizations per second is a moderate transactional workload and is not what makes this system hard. The
interesting arithmetic is downstream.

**Ledger write amplification.** One payment is not one row. A captured card payment moves money through
several accounts, and every movement is at least one debit and one credit.

```
authorize  0 entries (no money has moved)
capture    2: DR processor_clearing    CR merchant_payable
fee        2: DR merchant_payable      CR platform_fee_revenue
payout     2: DR merchant_payable      CR platform_cash
           ≈ 6 ledger entries per completed payment
6,000 payments/sec × 6  ≈ 36,000 ledger inserts/sec at peak
```

Thirty-six thousand append-only inserts per second, partitioned by account, is comfortable for a modern
relational store — and note they are *inserts*, never updates, which is the workload an LSM or an
append-friendly B-tree handles best (Chapter 01 §1).

**Storage cannot be reclaimed.** `100M/day × 6 entries × 200 B ≈ 120 GB/day`, or ≈ 300 TB over seven years
of audit retention — unremarkable in size, except that **none of it may be deleted or modified**, which
rules out designs that "clean up" history and forces corrections to be new, compensating entries.

**The constraining number is not QPS. It is the absolute error count.**

```
1,160/sec × 86,400 = 100,000,000 payments/day
a one-in-a-million defect rate  =  100 broken payments/day,
   each needing human investigation, a support contact, possibly a refund
```

Any error rate you would happily accept in another system produces, here, a daily queue of financial
incidents worked by hand. So the design does not aim to prevent all divergence — unachievable when a
processor call can time out — but to **detect every divergence automatically and drive the exception queue
to zero.** A design whose reconciliation story is "we'll check the logs" has not understood the number.

---

## 4. API

```
POST /v1/payment_intents                          Idempotency-Key: <client-generated>
  body: {amount: 4999, currency: "usd", payment_method: "pm_1H...",
         capture_method: "manual"|"automatic", metadata: {...}}
  ->    201 {id:"pi_3N...", status:"requires_capture", amount:4999, currency:"usd", created:...}

POST /v1/payment_intents/{id}/capture             Idempotency-Key
  body: {amount_to_capture?: 4999}
  ->    200 {id, status:"succeeded", latest_charge:"ch_3N..."}

POST /v1/refunds                                  Idempotency-Key
  body: {payment_intent:"pi_3N...", amount?: 2000, reason?: "requested_by_customer"}
  ->    201 {id:"re_3N...", status:"pending"}

GET  /v1/balance               -> 200 {available:[{amount, currency}], pending:[...]}
GET  /v1/balance_transactions  -> 200 {data:[...], has_more, next_page}
POST /v1/webhook_endpoints     -> 201 {id, url, secret:"whsec_...", enabled_events[]}
```

**`Idempotency-Key` is required, not optional, on every mutating call.** It is client-generated, one UUID
per logical operation (not per HTTP attempt), and it is the most important header in the API; §7.1 covers
what the server must do with it, because the obvious implementation is wrong. **Amounts are integers in the
currency's minor unit,** paired with a currency code: `4999` and `"usd"` is $49.99. No decimal point appears
anywhere in the API and no float anywhere in the system (§7.6).

**Authorization and capture are separate calls** because they are separate events in the outside world with
different reversibility. An authorization reserves funds and is voided cheaply and invisibly; a capture
moves money and is undone only by a refund the customer sees and that costs fees. Exposing both lets a
merchant authorize at checkout and capture at shipment, and it gives your own saga a cheap compensation for
every step before the last (§7.3).

**Every response is the full resource, not an acknowledgment.** A retry answered with `{"ok": true}` has
destroyed the information the client needed — §7.1's rule, seen from the client's side.

---

## 5. Data model

```
payment_intents(id PK "pi_...", merchant_id, customer_id, amount_minor BIGINT,
    currency CHAR(3), processor_ref TEXT NULL, created_at, updated_at,
    status ENUM(requires_payment_method, processing, requires_capture,
                succeeded, canceled, failed))

ledger_transactions(txn_id UUID PK, kind ENUM(capture, fee, refund, payout, adjustment),
    source_type, source_id, effective_at, created_at)      -- the unit of atomicity

ledger_entries                                             -- APPEND ONLY. No UPDATE. No DELETE.
  entry_id BIGSERIAL PK, txn_id UUID -> ledger_transactions, account_id BIGINT
  direction ENUM(debit, credit), amount_minor BIGINT CHECK (amount_minor > 0)
  currency CHAR(3), created_at TIMESTAMPTZ
  -- invariant, checked at write and continuously: per txn_id and currency,
  --   SUM(debit) = SUM(credit)

accounts(account_id PK, owner_type ENUM(merchant, platform, processor, customer),
    owner_id, currency, type ENUM(asset, liability, revenue, expense))
    -- NOTE: deliberately no `balance` column. See §7.2.

account_balance_snapshots(account_id, currency, as_of_date, balance_minor, last_entry_id)

idempotency_keys
  PRIMARY KEY (merchant_id, key)
  request_fingerprint TEXT            -- hash of method + path + body
  state ENUM(in_progress, completed), created_at, expires_at
  response_status INT, response_body JSONB, resource_id TEXT   -- THE STORED RESULT

webhook_events(event_id PK, merchant_id, type, payload JSONB, sequence BIGINT, created_at)
webhook_deliveries(delivery_id PK, event_id, endpoint_id, attempt, status, next_attempt_at)
settlement_lines(processor_ref, amount_minor, currency, fee_minor, settled_on, raw)
reconciliation_exceptions(id, kind ENUM(missing_locally, missing_at_processor,
    amount_mismatch), processor_ref, local_ref, delta_minor, opened_at, resolved_at)
```

**There is no balance column.** A merchant's balance is `SUM(credits) - SUM(debits)` over their account's
entries, made fast by daily snapshots (§7.2) — the schema-level expression of "the ledger is the source of
truth," and reversing it is the classic way to lose money. **`ledger_entries` is append-only and its
transactions must balance:** a correction is a *new* transaction reversing the old one, never an `UPDATE`.
That makes the seven-year audit requirement satisfiable, and it makes bugs survivable, because history is
intact and damage can be computed and undone.

**`idempotency_keys` stores the response body, not just the key.** The `state` column exists so a
concurrent duplicate — the same key arriving while the first is still in flight — is told to retry rather
than allowed to execute in parallel (§7.1). **`reconciliation_exceptions` is a first-class table, not a log
line**; designing the divergence queue into the schema is the difference between discovering a $40,000 gap
in a quarterly audit and opening a ticket the next morning.

---

## 6. Architecture, derived

### Attempt 1: a balance column

Each merchant has a row with a balance; a payment updates it inside a transaction.

```sql
BEGIN;
  UPDATE accounts SET balance = balance + 4999 WHERE id = ?;   -- merchant
  UPDATE accounts SET balance = balance -  145 WHERE id = ?;   -- fee
  INSERT INTO payments ...;
COMMIT;
```

Atomic, fast, and what almost everyone writes first. It fails on questions a payments company is asked
constantly. *"Why is this merchant's balance $12,403.17?"* There is no answer: the number is the accumulated
result of prior updates, each of which destroyed the value before it. *"A bug double-applied fees for an
hour last Tuesday — undo it."* You cannot compute what to undo; at 1,160 payments per second an hour is 4.2
million mutations whose inputs no longer exist. *"The process crashed between the two updates."* In one
database the transaction handles it; between your database and the processor nothing does, and that is the
normal case here.

The failure is not throughput. **A mutable balance column is a data model that discards the evidence needed
to detect and repair its own errors**, in a domain where errors are guaranteed and repair is mandatory.

### Attempt 2: an append-only double-entry ledger

Stop storing state; store the movements that produce it.

```
txn 8f2c (capture of pi_3N…, $49.99):   DR processor_clearing:usd   4999
                                        CR merchant_4471_payable    4999
txn 91ab (fee, 2.9% + 30¢):             DR merchant_4471_payable     175
                                        CR platform_fee_revenue      175
balance(merchant_4471) = Σ credits - Σ debits = 4999 - 175 = 4824
```

Every question above is answerable by reading rows, every transaction sums to zero so a missing or
duplicated entry is *detectable* rather than merely regrettable, and corrections are new transactions, so
the audit trail survives them. **It breaks on reads:** a merchant two years in has tens of millions of
entries, and summing them per dashboard load is not viable at any query rate.

### Attempt 3: snapshots over the ledger

Nightly, write `account_balance_snapshots(account_id, as_of_date, balance_minor, last_entry_id)`; a balance
read becomes the snapshot plus entries after `last_entry_id`. At 100M payments/day over ~500,000 merchants,
one merchant's daily delta is a few hundred entries — a bounded, indexed range scan. The snapshot is
**derived and rebuildable**: if it is ever wrong, recompute it from the entries, which are the truth.

### Attempt 4: idempotency and the outbox

Two cross-cutting mechanisms belong in the write path itself. **Idempotency keys** (§7.1) wrap every
mutating request: claim the key, do the work, store the response. **The outbox** (Chapter 01 §9) makes the
ledger write and the downstream notification atomic — writing to the database and then publishing to Kafka
leaves a window where one succeeds and the other does not, meaning a captured payment whose merchant is
never told, or a webhook for a payment that was rolled back.

```
BEGIN;
  INSERT INTO ledger_transactions ...;
  INSERT INTO ledger_entries ...;                -- balanced
  UPDATE payment_intents SET status = 'succeeded' ...;
  INSERT INTO webhook_events (...);              -- the outbox
  UPDATE idempotency_keys SET state='completed', response_body = ...;
COMMIT;
```

Everything that must be consistent is in one transaction in one database; everything else is at-least-once
delivery over that transaction's output, consumed idempotently. That is Chapter 01 §8's pragmatic advice —
co-locate the invariant, make the rest eventually consistent — applied literally.

### Attempt 5: the saga, and reconciliation as a permanent component

The processor call cannot be inside that transaction: it takes 500 ms and may time out. The flow becomes a
saga (§7.3) whose steps commit locally and whose compensations are voids and refunds. Because a saga step
can end in an *unknown* state, the design needs a component whose job is comparing your records against the
processor's settlement file and opening an exception per discrepancy (§7.5). **Reconciliation is not an
operational afterthought but a load-bearing component**, designed in on the assumption that divergence will
happen, because it will.

### Final architecture

```
 Merchant API ──► Idempotency layer (claim key → replay stored result)
   (authorize)         └─► Payment service ──► Processor (authorize)  ~500 ms
                             │
                             ▼  one DB transaction
                ┌────────────────────────────────────────────┐
                │ payment_intents (state machine)            │
                │ ledger_transactions + ledger_entries       │  Postgres,
                │ idempotency_keys (response stored)         │  partitioned
                │ webhook_events  ◄── outbox                 │  by account
                └───────────────┬────────────────────────────┘
          ┌─────────────────────┼───────────────────────────────┐
          ▼                     ▼                               ▼
 Outbox relay ──► Kafka   Snapshot job (nightly)     Reconciliation service
          │                     │                               ▲
          ▼                     ▼                     daily settlement file
 Webhook dispatcher    account_balance_snapshots       (SFTP, T+1) from processor
 (retry + HMAC sig) ──► Merchant endpoint                        │
                                                                 ▼
                                                    reconciliation_exceptions
                                                      → alerts, ops queue
```

Note what is *not* in the picture: no distributed transaction, no two-phase commit, and no service that can
mutate a balance without writing an entry.

---

## 7. Deep dives

### 7.1 Idempotency keys, and the mistake everyone makes

The client sends `Idempotency-Key: 7f3a…` with a payment. The request times out. The client retries with
the same key. What must the server do? The naive implementation stores keys in a set and returns success:

```
if key in seen: return 200 OK        # WRONG
seen.add(key); charge(); return 201 {id: "pi_3N..."}
```

**This loses the payment ID.** The first attempt created `pi_3N…` and the client never received it, because
the response was lost. The retry gets a bare 200 with no resource, so the client cannot show a receipt,
cannot reconcile its own order, and cannot refund later. Many clients treat the ambiguous response as a
failure and create a *second* payment with a *new* key — a genuine double charge produced by the
idempotency mechanism itself.

**The server must return the stored result of the first attempt**, same status code and same body:

```
1. INSERT INTO idempotency_keys (merchant_id, key, request_fingerprint, state)
     VALUES (?, ?, ?, 'in_progress') ON CONFLICT DO NOTHING;
2. If no row was inserted, read the existing one:
     state='completed'   -> return stored response_status + response_body
     state='in_progress' -> return 409; the client retries shortly
     fingerprint differs -> return 422; the key was reused for a different request
3. Otherwise do the work, and in the SAME transaction as the ledger write:
     UPDATE idempotency_keys SET state='completed', response_status=?,
            response_body=?, resource_id=?;
```

Four details separate a correct implementation from a plausible one. **The key row is written before the
work**, so a crash mid-flight leaves an `in_progress` marker rather than nothing and the retrying client
waits instead of charging twice. **The response is stored in the same transaction as its effects**, or a
window exists where the ledger entry is durable and the stored response is not, and a retry re-executes.
**The request fingerprint is checked**, because a client reusing a key with a different body has a bug that
a silent replay would hide. And **keys expire** — 24 hours is typical — after which safety comes from the
natural idempotency of the resource (a `succeeded` intent cannot be captured again) plus reconciliation.

The general form, from Chapter 03 §4: **idempotency is not "ignore duplicates," it is "produce the same
observable result" — and the observable result includes the response body.**

### 7.2 The double-entry ledger, and why a mutable balance loses money

Double-entry bookkeeping is six hundred years old and survives because it is an *error-detecting code* for
money. Every movement is recorded twice in opposite directions, and the sum over any complete transaction
is zero, so a single lost, duplicated, or corrupted entry breaks the sum and is detectable by a query
anyone can run.

```
1 (per transaction): Σ debits = Σ credits, per currency
2 (global, per currency): Σ all entries = 0
3: every payment_intent in 'succeeded' has exactly one capture transaction
```

Run invariant 2 continuously: a single aggregate over an append-only table, and the cheapest, highest-value
alarm in the system.

**Contrast with the mutable balance.** `UPDATE accounts SET balance = balance + 4999` has none of these
properties: not self-checking, so a lost, doubled, or sign-flipped update produces a number with nothing to
compare it against; not reconstructible, because the prior value is gone; not auditable. It also invites
the read-modify-write in application code (`bal = read(); write(bal + x)`) that loses concurrent updates
outright — Chapter 01 §8's lost-update anomaly, applied to money.

Two objections deserve answers. *"Isn't `balance = balance + x` atomic, and therefore safe?"* Atomic yes,
safe no: atomicity prevents lost updates within one database, but gives no audit trail, no way to compute a
bug's blast radius, and no help when the update commits and the processor call did not. *"Isn't summing
millions of rows slow?"* Yes, which is what snapshots are for — and the discipline that matters is that
**the snapshot is a cache and the entries are the truth.** A team that "fixes" a balance by editing a
snapshot has stopped treating the ledger as the source of truth, and the property that made the design
correct is gone. The genuine cost is six inserts instead of two updates plus a snapshot pipeline: worth
paying for money, not for a video view counter.

### 7.3 Sagas and compensating transactions, not 2PC

The authorize → capture → settle flow spans your database and a payment processor. Two-phase commit is not
available, and it is worth being precise about why rather than reciting that 2PC is slow. **The processor
does not implement a prepare phase** — 2PC requires every participant to expose "prepare and hold this,"
and no card processor's public API does. **2PC blocks on coordinator failure**, and holding a lock inside a
third party for an unbounded time is not something you can do. And **the failure you actually face is a
timeout**, which leaves the outcome unknown regardless of protocol.

The alternative is a **saga** (Chapter 01 §8; Garcia-Molina and Salem, 1987): local transactions, each with
a compensating action that semantically undoes it.

```
step                  forward                      compensation
1 reserve intent      INSERT payment_intent        mark canceled
2 authorize           processor.authorize()        processor.void()    cheap, invisible
3 record capture      ledger txn + outbox          reversing ledger txn
4 capture             processor.capture()          processor.refund()  visible, costs fees
5 payout              ledger txn + bank transfer   reversing entry + claw-back (hard)
```

**Order the saga so the least reversible step is last.** An authorization is free to void, a capture costs
a refund, a completed payout is nearly irreversible; every failure before the expensive step compensates
invisibly. **Compensation is semantic, not a rollback:** a refunded payment is not an un-happened payment,
the customer sees both lines on their statement, and the ledger shows the capture and its reversal.
Intermediate states being visible is the price sagas charge, and it is correct here — pretending the
payment never happened would itself falsify the record.

**Unknown outcomes get their own state.** When step 4 times out, the intent moves to `processing`, not
`failed`; a background resolver polls the processor by your idempotency key (processors accept one for
exactly this reason) until the outcome is known, with reconciliation as the backstop. **Never infer failure
from a timeout.** Treating a timed-out capture as failed and retrying it against a non-idempotent endpoint
is the most common way to double-charge a customer.

### 7.4 Webhooks: at-least-once, signed, and out of order

Merchants need to know when a payment succeeds. A webhook is an HTTP POST to a URL the merchant controls —
a target that is frequently slow, occasionally down, and sometimes behind a load balancer that retries on
its own.

**Delivery is at-least-once and cannot be otherwise.** If the merchant processes the event and the response
is lost, you must retry and they will see it twice. Exactly-once delivery is unavailable across a network
(Chapter 03 §3); exactly-once *effect* is, and it is the merchant's side of the contract: send a stable
`event_id`, tell them to deduplicate on it, and make every event carry enough state to be applied
idempotently. **Retries follow exponential backoff with jitter** — roughly 1 min, 5 min, 30 min, 2 h, 5 h,
over about three days — after which the endpoint is disabled and the merchant emailed. Deliveries live in
their own table with `next_attempt_at` and the dispatcher is a scheduler over that column (Chapter 92). A
slow merchant must not block others: partition the dispatcher by merchant and cap in-flight deliveries per
endpoint, or one merchant's 30-second timeouts consume the whole worker pool.

**Signature verification is mandatory and its details matter.** Sign `timestamp + "." + raw_body` with
`HMAC-SHA256(endpoint_secret, ...)` and send it as `t=1699..., v1=<hex>`. The timestamp is *inside* the
signed payload and the receiver rejects timestamps outside a tolerance (five minutes is typical), which is
what prevents replay of a captured legitimate webhook. Signing the parsed JSON instead of the raw bytes
breaks verification the moment either side reserializes — a real and common integration bug.

**Out-of-order arrival is guaranteed** once retries exist: `payment_intent.succeeded`, retried at t+5 min,
arrives after `charge.refunded` from t+1 min. Attach a **monotonic per-object sequence number** to every
event and have consumers ignore any event below the highest already applied for that object. Ordering the
*delivery* instead — a per-merchant serial queue — is worse: one stuck event blocks every later event for
that merchant, converting a delivery problem into an outage. Push ordering to the consumer; keep delivery
parallel.

### 7.5 Reconciliation, designed for divergence

Once a day the processor produces a settlement file: every transaction it believes it processed, with
amounts, fees, and settlement dates. Reconciliation compares that file against your ledger. It is not a
health check — it is the mechanism by which correctness is actually established, because the two records
are maintained independently and *will* drift.

| Class | Meaning | Typical cause | Response |
|---|---|---|---|
| In the file, not in your ledger | The processor moved money you did not record | A capture that timed out and was never resolved | Create the missing ledger transaction; the money is real |
| In your ledger, not in the file | You recorded a movement they do not have | A capture recorded optimistically before confirmation | Investigate, then reverse with a compensating transaction |
| In both, amounts differ | Fee change, FX, partial capture | Rounding, conversion, processor-side adjustment | Post an adjustment transaction for the delta |

**Match on a stable shared key** — the `processor_ref` you store the moment the processor returns one —
never on `(amount, timestamp)`, which collides constantly at 100M payments a day. **Every unmatched line
becomes a row in `reconciliation_exceptions`, not a log line**, with an owner, an age, and alerts on count
and oldest-age. "Open exceptions older than 48 hours" is arguably the best single health metric this system
has, because it aggregates every silent failure in every component into one number a human can act on.
**Resolution is always a new ledger transaction** of kind `adjustment`; never edit history to make
reconciliation pass, which converts a detected error into an undetectable one.

**Reconcile more often than daily where you can.** A T+1 file means up to 24 hours of undetected divergence.
Most processors expose near-real-time event streams; consuming those gives continuous partial
reconciliation with the daily file as the authoritative sweep. That is the difference between finding a
systematic bug in an hour and finding it tomorrow, after another 100 million payments.

### 7.6 Money: integers, rounding, and currency

**Money is never a float.** `0.1 + 0.2 != 0.3` in IEEE 754 because tenths are not representable in binary,
and the error accumulates over sums. In a ledger that must balance to the cent across billions of rows this
is fatal: the invariant `Σ = 0` fails on rounding noise alone.

Represent money as a **64-bit integer in the currency's minor unit**, always paired with a currency code.
The range is ample — `2^63 - 1 ≈ 9.2 × 10^18` minor units is about 92 quadrillion dollars. The minor unit is
currency-specific and there is no universal factor of 100: JPY has zero decimal places, KWD has three, and
hard-coding two is a bug that appears the first time a Japanese merchant is onboarded.

**Rounding must be explicit and allocated.** Splitting $10.00 three ways gives 333.33 each and a lost cent.
Use **largest-remainder allocation** — integer shares, then the remainder distributed deterministically so
the parts sum exactly to the whole: `allocate(1000, [1,1,1]) -> [334, 333, 333]`. Never round each share
independently and hope. Same discipline for percentage fees: integer arithmetic, with the rounding rule
(half-up, banker's, or always toward the platform) defined once, in one documented function.

**Multi-currency means multi-account.** An account holds exactly one currency, so a merchant with USD and
EUR balances has two accounts and the zero-sum invariant holds per currency. Never sum across currencies.
Conversion is an explicit transaction with two legs in different currencies plus an FX-gain or FX-loss
account absorbing the difference — which is exactly how double entry handles the fact that conversion is
not value-preserving at any single rate. Store the rate and its timestamp on the transaction; without them
the conversion is unauditable.

### 7.7 PCI scope reduction through tokenization

Handling raw card numbers subjects every system that touches them to PCI DSS: segmentation, quarterly
scans, annual assessment, encryption at rest and in transit, restricted access, and an audit that grows
with the number of systems in scope. The engineering goal is therefore not "comply harder" but **keep card
data out of your systems entirely.**

The mechanism is tokenization at the edge. The merchant's page embeds an iframe or client-side SDK served
by the payment provider; card details go from the customer's browser **directly to the provider**, never
touching the merchant's server or yours, and what returns is an opaque token (`pm_1H…`). Every subsequent
call references the token, and the card number lives only in the provider's vault, the sole PCI-scoped
system. This collapses the merchant's obligation to the simplest self-assessment questionnaire, and it
means your logs, databases, error trackers, and analytics can never accidentally contain a card number,
because they were never given one.

Two consequences worth naming. **The token is scoped**, typically to one merchant, so it is not useful if
leaked elsewhere — but it *is* sufficient to charge within that merchant, so it remains a secret. And **the
vault is the highest-value target in the architecture**, isolated accordingly: separate service, separate
credentials, keys in an HSM or KMS with per-record data keys, access logged and reviewed. This is
deliberately at the level of what the architecture must do; real compliance involves a qualified assessor
and a scoping exercise, and claiming otherwise is worse than saying you would involve one.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Processor call times out | Outcome unknown; funds may or may not have moved | State `processing`, never `failed`; resolver polls by idempotency key; reconciliation as backstop |
| Duplicate request from client retry | Risk of double charge | Key claimed before work; stored response replayed (§7.1) |
| Crash between ledger write and publish | Merchant never learns of a real payment | Outbox in the same transaction; relay publishes after commit (Ch. 01 §9) |
| Merchant endpoint down | Events undelivered | Backoff retries over ~3 days, per-endpoint concurrency caps, then disable and email |
| Settlement file late or malformed | Reconciliation stalls; divergence undetected | Alert on file-arrival SLA; parse strictly, fail loudly, never partially apply a bad file |
| Ledger invariant violated (Σ ≠ 0) | Money provably missing or duplicated | Continuous invariant query with a page; freeze the account; correct with an adjustment, never an edit |
| Snapshot job wrong or failed | Balances read stale or wrong | Snapshots are derived — recompute from entries; fall back to a full sum for one account |
| DB failover mid-transaction | Uncommitted work lost | Synchronous replication; the idempotency row makes the client's retry safe by construction |

**Monitoring:** open reconciliation exceptions by age (the headline number); ledger imbalance count, which
must be exactly zero; authorization success rate by processor and issuer; processor-call p99 separated from
your own latency; idempotency replay rate; webhook success and queue depth; and intents stuck in
`processing` beyond a threshold, the leading indicator of an unresolved timeout class.

---

## 9. Common mistakes

1. **A mutable balance column.** The classic way to lose money: unauditable, unreconstructible, and
   silently wrong after any bug. Serious payments systems store movements and derive balances.
2. **Idempotency that returns a bare success on replay.** It destroys the resource ID the client needed and
   pushes clients into creating a second payment with a new key — a double charge caused by the very
   mechanism meant to prevent one (§7.1).
3. **Inferring failure from a timeout.** The most reliable way to double-charge is to treat an ambiguous
   capture as failed and retry it. Unknown is its own state.
4. **Reaching for two-phase commit** across the processor. There is no prepare phase to reach for; the
   answer is a saga with compensations, ordered so the irreversible step is last.
5. **Floating-point money**, or a hard-coded factor of 100. Integers in minor units, currency always
   attached, JPY and KWD in the test suite.
6. **No reconciliation, or reconciliation as a quarterly spreadsheet.** The design assumption must be that
   divergence *will* happen; without an automatic detector, discrepancies are found by customers.
7. **Assuming webhooks arrive once and in order.** They arrive at least once and out of order: sequence
   numbers on the event, deduplication on the consumer, and never a per-merchant serial queue that one
   stuck event can block. Related: **signing the parsed body instead of the raw bytes**, or omitting the
   timestamp from the signed payload, which leaves the endpoint open to replay.
8. **Letting card data into your systems** because "we'll encrypt it." Tokenization in the browser keeps it
   out of your logs, your database, and your PCI scope, which is a strictly better position.

---

## 10. Variants

**Digital wallet / P2P transfer (Venmo, PayPal balance).** Both sides are internal accounts, so a transfer
is a single balanced ledger transaction in one database — strictly easier, with no external processor and
therefore no unknown outcomes. The interest shifts to velocity limits, holds, and the social feed.

**Marketplace split payments.** One charge fans out to several merchant accounts plus platform fees, so a
transaction has many legs and §7.6's allocation arithmetic becomes central: the legs must sum exactly to
the charge, with the remainder deterministically assigned.

**Subscription billing.** Adds a scheduler (Chapter 92) firing millions of renewals at period boundaries,
proration arithmetic, and dunning — a failed renewal retried on a schedule tuned to issuer behavior, each
attempt an idempotent charge with its own key.

**Bank core ledger.** The same double-entry model with strict ordering, regulatory reporting, and often a
single-writer design per account; Chapter 83's deterministic loop is a closer relative than it appears.
**Crypto exchange:** an internal ledger plus an on-chain settlement layer with irreversible transfers and
probabilistic finality, where reconciliation against the chain replaces reconciliation against a file.

---

## 11. Further reading

- Chapter 01 §8 (sagas, isolation) and §9 (the outbox); Chapter 03 §4–§5 (idempotency, retries);
  Chapter 80 §7.5, for this saga applied to inventory
- Hector Garcia-Molina and Kenneth Salem, "Sagas" (SIGMOD 1987) — the original paper
- Pat Helland, "Life Beyond Distributed Transactions: An Apostate's Opinion" (CIDR 2007) — why you design
  to avoid distributed transactions rather than to implement them
- Martin Fowler, [Accounting Patterns](https://martinfowler.com/eaaDev/AccountingNarrative.html) — the
  ledger, account, and adjustment patterns in general form
- [Stripe API — Idempotent requests](https://docs.stripe.com/api/idempotent_requests) and
  [Webhooks](https://docs.stripe.com/webhooks) — the reference implementation of §7.1 and §7.4
- [PCI Security Standards Council — Document Library](https://www.pcisecuritystandards.org/document_library/)
  for the SAQ types referenced in §7.7
