# Chapter 32 — Notification System

> **Prerequisites:** Chapters 02 (native push, §8), 03 (queues, at-least-once, idempotency, retries, DLQs), 04 (token bucket, §3)
> **Patterns:** fanout pipeline, audience snapshot, idempotent consumers, provider error classification, priority isolation

---

## 1. The problem

Something happens — a comment on a post, a password reset, a package out for delivery, a marketing team
pressing a button — and one or more people should be told, on whichever of push, SMS, email, and in-app
they have not opted out of, in their language, not at three in the morning.

Every product grows this system, and almost every product grows it wrong first: the service that
generated the event calls the push provider inline, and notification logic ends up smeared across a dozen
codebases with a dozen retry policies and no deduplication anywhere.

**The property that makes it hard:** the work is *unbounded fanout across untrusted external dependencies
whose failure modes are semantically meaningful.* A single API call can mean fifty million provider
requests, to servers that throttle you, that go down, and that answer with status codes where one means
"retry in a second" and a nearly identical one means "this device does not exist anymore, and if you
retry it you will do so forever." And because the last hop leaves your infrastructure entirely, **you can
prove a notification was accepted by Apple and you can never prove a human saw it.**

---

## 2. Requirements

### Functional

1. A producer triggers a notification for one user, a list of users, or an audience segment.
2. Deliver over four channels: **push** (APNs for iOS, FCM for Android, Web Push), **SMS**, **email**,
   and **in-app** (a persistent inbox the client reads).
3. Respect preferences: per-channel opt-out, per-category opt-out, quiet hours in the user's local time.
4. Templating and localization — one logical notification renders differently per channel and locale.
5. Campaigns: schedule a send to a segment, with pause, throttle, and progress reporting.
6. Device token registration and lifecycle, and delivery status to whatever fidelity providers give us.

Defer, but name: in-app presentation rules, copy A/B testing, per-user send-time optimization, rich media.

### Non-functional

- **Transactional latency** — p99 under 10 s from trigger to provider-accepted. A login OTP is worthless
  late; a user staring at a screen abandons around thirty seconds.
- **Marketing latency** — minutes to hours. **That these two numbers differ by three orders of magnitude
  is what licenses the priority isolation in §7.3**; a candidate who never separates them builds one
  queue and is surprised.
- **Delivery guarantee** — at-least-once transport plus deduplication, giving *effectively* once, and no
  duplicate that reaches a human. Exactly-once across a provider boundary does not exist (Chapter 03,
  §3); do not claim it.
- **Availability** — intake must accept writes while every provider is down. Degraded means *delayed*,
  never *rejected at the door*, because the producer cannot retry later: the event already happened.
- **Isolation** — a campaign must not be able to delay transactional traffic. State it as a requirement;
  it is the one the architecture is really for.
- **Compliance** — unsubscribe honored promptly and durably. In several jurisdictions that is a legal
  deadline, and an opt-out evaluated against a stale snapshot is a regulatory finding, not a bug.

### Explicitly out of scope

The push transport itself, the model that decides *whether* to notify, and the client's
notification-permission UX. Name them; the second is a common and legitimate follow-up.

---

## 3. Estimation

Assume 500 million registered users, 200 million daily active.

**Notification volume**

```
transactional / social:  200M DAU × 10/day       = 2.00B/day
marketing:               500M × 2 per week / 7   = 0.14B/day
2.15e9 / 86,400                                  ≈ 25,000 notifications/sec average
diurnal peak (3×)                                ≈ 75,000/sec
```

**Push sends, which are a different number**

```
devices: 500M × 1.8                        = 900M tokens
25,000 × 70% routed to push × 1.8 devices  ≈ 31,500 sends/sec average, ~95,000 peak
```

A worker holding a persistent HTTP/2 connection to APNs with a few hundred concurrent streams sustains
roughly 500 sends/sec, so ~200 workers cover 100,000/sec. **The steady state is not the problem.** Say so
early; the temptation is to size for the average and declare victory.

**The burst — the constraining number**

```
one campaign, one API call:                50,000,000 recipients
fleet capacity:                               100,000 sends/sec
drain time if the campaign owns the fleet:        500 s ≈ 8.3 minutes
```

Restated so it hurts: 50 million messages is **2,000 seconds of normal traffic arriving in one HTTP
request.** In a shared queue, a password reset enqueued behind it waits up to 500 s against a 10 s SLO —
a fiftyfold violation produced entirely by a queueing decision, with no component slow or broken. This,
not average throughput, is what §6 is derived from.

**Deduplication store.** At-least-once means one dedup claim per notification, with a TTL covering any
plausible retry window — 24 hours is standard (Chapter 03, §4).

```
25,000/sec × 86,400 = 2.16e9 live keys × ~100 B (16 B digest + value + overhead) ≈ 216 GB RAM
```

Two hundred gigabytes whose only job is remembering what you already sent — which is why the digest is
truncated and the TTL is a tuning knob. Truncation is safe: over 2.16 × 10^9 keys in a 128-bit space the
birthday collision probability is `n²/2^129 ≈ 4.7e18 / 6.8e38 ≈ 7 × 10^-21`. A Bloom filter would cut
this to two gigabytes and is **wrong**, because its error is a false *positive* — silently dropping a
real notification that will never be resent. Get the direction of the error right before optimizing
memory.

**Storage**

```
in-app inbox:  2.15e9/day × 30 days × 300 B  ≈ 19 TB
device tokens: 900M × 200 B                   ≈ 180 GB     (delivery attempts, 7 d: ≈ 5.4 TB)
```

Nineteen terabytes of inbox is a sharded wide-column table and nothing more. The 180 GB token table is
small enough to be uninteresting — worth noticing, because it is the table the entire push path depends
on, so the conclusion is to make it fast and highly available rather than clever.

---

## 4. API

```
POST /v1/notifications                       # intake
  Idempotency-Key: <optional client key>
  body: { recipients: {userId} | {userIds:[...]} | {segmentId},
          type: "comment_reply" | "password_reset" | ...,
          priority: "transactional" | "marketing",
          eventId: <producer's id for the triggering event>,
          templateId, templateVersion?, data:{...},
          channels: ["push","email","sms","in_app"],
          collapseKey?, ttlSeconds?, sendAt? }
  -> 202 { notificationId, status: "accepted" }

POST /v1/campaigns  { segmentId, templateId, throttlePerSecond?, startAt, ttlSeconds }
  -> 202 { campaignId, estimatedAudience }
GET  /v1/campaigns/{id}        -> 200 { state, snapshotSize, sent, failed, suppressed }
POST /v1/campaigns/{id}/pause  -> 204

POST /v1/devices  { platform, token, appVersion, locale, timezone } -> 204   # + DELETE
GET  /v1/users/me/preferences  -> 200 { channels{}, categories{}, quietHours{}, timezone }
PUT  /v1/users/me/preferences  -> 204
GET  /v1/users/me/inbox?cursor=&limit=50 -> 200 { items:[...], unreadCount, nextCursor }
POST /v1/callbacks/{provider}  # inbound: bounces, complaints, unsubscribes, invalidations
```

**202, never 201, and never a delivery status.** Intake accepts an *intent*; when it replies it has sent
nothing and cannot know whether it will be able to. That is what lets intake stay available while every
provider is down, and it stops producers from branching on a result the API cannot produce.

**The producer supplies `eventId`, not a dedup key.** The key is derived server-side (§7.1) so that a
Kafka replay, a worker restart, a producer retry, and *two different producers reacting to the same
event* all converge without having to agree on anything. `Idempotency-Key` sits on top for producers
guarding their own retries of the API call; the two deduplicate at different layers.

**Segments are referenced, never inlined.** `{userIds: [...50 million...]}` is not an API, it is a denial
of service — and referencing a saved query is what makes the snapshot in §7.4 possible.

**Channels are a request, not an instruction.** The caller says which channels are *eligible*;
preferences evaluated at send time decide which are used. Producers must not be able to override an
opt-out, and the only reliable enforcement is making it structurally impossible in the API.

---

## 5. Data model

```
device_tokens
  token       VARCHAR   PRIMARY KEY        # unique on the token, deliberately
  user_id     BIGINT    secondary index
  platform    ENUM(ios, android, web)
  timezone    VARCHAR                      # IANA name, not an offset
  state       ENUM(active, stale, invalid)
  app_version, locale, created_at, last_seen_at, invalidated_at

notification_prefs
  user_id     BIGINT    partition key
  channel     ENUM      clustering key     # push | sms | email | in_app
  category    VARCHAR   clustering key     # social | billing | security | marketing
  enabled     BOOLEAN
  # one row per user: quiet_start, quiet_end, timezone, digest_mode

templates
  template_id, version, locale, channel    # composite key
  subject, title, body, deep_link, required_vars[]

notifications                              # in-app inbox + audit trail
  user_id          BIGINT  partition key
  notification_id  BIGINT  clustering key desc   # Snowflake (Chapter 04, §1)
  type, template_id, rendered_payload, created_at, read_at

deliveries    # one row per channel attempt, 7-day TTL
  notification_id, channel, provider, provider_message_id, status, attempts, last_error_code
campaigns
  campaign_id, segment_id, snapshot_uri, snapshot_size, state, throttle_rps,
  cursor, started_at, template_id, template_version
```

Four decisions worth defending.

**The token is the primary key, not `(user_id, token)`.** A device moves between accounts — one user logs
out, another logs in, and the OS hands the app the same token. Without uniqueness both rows survive and
the second user's notifications land on a device the first is holding. That is a privacy incident,
prevented by a constraint rather than by application code remembering to clean up; registration is an
upsert that *reassigns* `user_id`.

**Preferences are a matrix, not a boolean.** `(channel × category)` is the minimum shape expressing
"email me about billing, never text me, push me about comments but not product news" — what users
actually ask for, and what most first implementations cannot represent. Adding the dimensions later is a
migration with no correct default.

**The inbox is not a disposable cache.** Chapter 20 could treat its materialized timeline as derived
state and rebuild it after a Redis loss. This table holds `read_at`, *original* state existing nowhere
else, so it must be durable. That is the one structural difference between a notification feed and a
social feed, and it is why two systems that look identical on a whiteboard have different storage bills.

**Timezone lives on the device as well as the user.** Quiet hours are local, and the device is the only
thing that knows where the user is.

---

## 6. Architecture, derived

### Attempt 1: the producer calls the provider

`POST /comments` writes the comment, then calls `APNs.send(token)`, then returns 201. Fine for one
recipient. A post from an account with 500,000 followers, at 20 ms per provider round trip with 200
concurrent requests, needs `500,000 × 0.02 / 200 = 50 seconds` inside an HTTP handler — and it fails
worse than the arithmetic suggests, because an APNs outage now fails the comment write. Notification
delivery has become a dependency of core product functionality that does not need it. **Breaks at:** any
audience above roughly a hundred, and at any provider degradation at all.

### Attempt 2: one queue, one message per recipient

The producer expands the audience, enqueues one message per recipient, returns; workers drain and send.
Two things break. **The producer still does the fanout** — expanding 50 million rows inside a request
handler means holding them in memory, and a handler that dies at row 30 million cannot resume; expansion
must be a restartable, observable job, not a loop. And **head-of-line blocking**: one queue puts the
campaign's 50 million messages in front of everything enqueued after them, 500 seconds of drain against a
10-second SLO (§3). Adding workers shrinks the magnitude while preserving the violation. **Breaks at:**
the first campaign above roughly 100,000 recipients — which is to say immediately, and invisibly in
testing, because test campaigns are small.

### Attempt 3: two-stage fanout with priority-isolated topics

Split the pipeline where one message becomes many, and split the transport by priority. Stage one carries
the *intent* — one small message regardless of audience size. Stage two expands it against an immutable
snapshot with a persisted cursor (§7.4). The two priority classes ride separate topics with separately
provisioned consumer groups, so a campaign is physically incapable of getting in front of a password
reset.

This is the right shape. Three things are still wrong. **Duplicates reach users**, because at-least-once
means a fanout worker that crashed after message 400,000 of a chunk resends the whole chunk. **Dead
tokens are retried forever**, because a worker treating every provider error as retryable retries
`410 Unregistered` on every attempt of every notification, permanently. And **the campaign DDoSes the
product**, because everyone who taps arrives at once (§7.5).

### Attempt 4: claim, classify, govern

1. **An idempotency claim before every send** — `SET key PENDING NX EX 86400` in Redis, keyed on
   `SHA256(userId + eventId + type)`. Claim wins → send; claim loses → drop silently. This converts
   at-least-once transport into effectively-once delivery, and is why the rest of the pipeline is
   allowed to be at-least-once.
2. **A provider error classifier** ahead of the retry logic, mapping each status code to exactly one of
   three actions: retry with backoff, mark the token invalid, fail terminally to the DLQ (§7.2).
3. **A dispatch governor** — a distributed token bucket (Chapter 04, §3) — pacing campaign fanout to
   downstream headroom rather than to how fast the fleet can go (§7.5).

### Final architecture

```
 product services ─ POST /v1/notifications ─► 202 accepted
        ▼
  ┌────────────┐ validate · resolve template · classify priority
  │  Intake    │ persist intent (durable) · publish intent
  └──┬──────┬──┘
     ▼      └──► notify.campaign ──► Audience service: segment query →
 notify.transactional  │             immutable S3 manifest, chunks of 10k
     │                 ▼
     │       Dispatch governor (token bucket) ◄── headroom signal from API
     ▼                 ▼
  ┌──────────────────────────────────────────────────────┐
  │ Fanout workers → per recipient, then per device      │
  ├──────────────────────────────────────────────────────┤
  │ Send pipeline: 1 idempotency claim (SET NX, 24 h)    │
  │  2 preferences · opt-out · quiet hours   (LIVE)      │
  │  3 render + localize   4 route to eligible channels  │
  └──┬─────────┬─────────┬──────────┬────────────────────┘
     ▼         ▼         ▼          ▼
  push.q    email.q    sms.q    in_app.q ─► inbox + websocket nudge
     │         │         │
  APNs·FCM    SES     carrier
  Web Push     │         │
     │ 410/400 │ bounce  │ reject
     ▼         ▼         ▼
  Token reaper / suppression list ─► device_tokens.state = invalid
     retries exhausted ─► DLQ ─► alert + TTL-aware replay tool
```

Note the two feedback edges — provider errors flowing into the token table, downstream load into the
dispatch rate. They are what make this a system rather than a pipe.

---

## 7. Deep dives

### 7.1 Idempotency: the key, the claim, and the gap

```
key = SHA256(userId ‖ eventId ‖ type)[:16]
```

Derivation matters more than the hash: because every stage can compute the key from data it already has,
deduplication survives a Kafka replay, a worker restart mid-chunk, a producer retry, and two independent
producers reacting to the same event — none of which a client-generated key covers, because no client is
common to all four. Include `type` (a comment and a like on the same post are different notifications)
and the channel (if a notification legitimately goes to both push and email); do **not** include a
timestamp or attempt number, which reintroduces exactly the duplicates the key exists to remove. The
claim is atomic (`SET ... NX`): check-then-set lets two workers both read "absent" and both send.

**The gap nobody mentions:** claiming before sending means a crash between claim and send loses the
notification permanently, because the key now says "already handled." Claiming after sending duplicates
it. No ordering avoids both — the same impossibility as exactly-once across a service boundary (Chapter
03, §3), because the claim store and the provider are two systems.

The workable answer is a two-state claim: write `PENDING` with a short lease (60 s), send, then overwrite
with `SENT` and the full 24-hour TTL; a worker finding a `PENDING` claim past its lease may take it over.
The residual risk is then confined to a genuinely ambiguous window, and you choose *per notification
type* which way to resolve it — a password reset leans toward a possible duplicate, a marketing blast
toward a possible loss. Saying which way you lean and why it differs by type is the answer this deep dive
wants; "we use an idempotency key" is not.

### 7.2 Provider error classification

Unglamorous enough that most candidates skip it, and where these systems actually break. Chapter 03, §5's
rule — 4xx terminal, 5xx retryable — is nearly right, and its exceptions are the ones that matter.

| Response | Meaning | Correct action |
|---|---|---|
| `200`/`202` | Provider accepted the request | Record `provider_message_id`; **not** a delivery |
| `400 BadDeviceToken`, `403 SenderIdMismatch` | Malformed token, or a token belonging to another app | Terminal; mark invalid; alert on any nonzero rate — it means a registration bug |
| `401` / `403` (auth) | Credentials wrong or expired | Terminal for the message, **page someone**: every send is failing, not this one |
| `404 Unregistered` / `410 Gone` | App uninstalled or token rotated | Terminal. **Mark the token invalid immediately** — the primary cleanup signal |
| `413` | Payload over 4 KB | Terminal; truncate at render time instead |
| `429` | You are being throttled | Retry honoring `Retry-After`; also lower the governor |
| `500`/`502`/`503`, timeout, reset | Provider trouble, or an unknown outcome | Retry, exponential backoff with full jitter; timeouts are §7.1's ambiguous window |

Three consequences. **`410` is a data-repair signal, not an error**; treating it as failure and retrying
is the most common production bug here, producing a slowly growing fraction of wasted calls nobody
notices until a campaign takes twice as long as it used to. Write `state = invalid` before acknowledging
the message. **`401` deserves different escalation than everything else** — every other row is a
per-message concern, but a revoked service account means the whole channel is down, so classify by blast
radius as well as retryability. **Exhausted retries go to a DLQ with an alert and a documented replay
procedure**, both halves (Chapter 03, §5), with one wrinkle specific to notifications: replaying a DLQ
two days later delivers "your ride is arriving" long after it arrived. Every message carries
`ttlSeconds` and the replay tool drops expired ones, because a notification that is too late is worse
than one that was lost — and only the sender knows where that line is.

### 7.3 Priority isolation, quantified

From §3: 50 million campaign messages ahead of a password reset is 500 seconds of queueing delay against
a 10-second target. Write that on the board — it converts an aesthetic preference into arithmetic.

**Separate topics with separate consumer groups** gives complete isolation: each class has its own fleet,
transactional sized for peak burst with large headroom, marketing for average throughput with none. The
cost is capacity fragmentation — at 75,000/sec peak against 100,000/sec provisioned, that idle
transactional headroom is real money the marketing fleet cannot borrow.

**Weighted fair queueing over one shared fleet** — nine transactional pulls per marketing one — utilizes
better and bounds transactional delay by the weight rather than by campaign size. The cost is that the
guarantee becomes soft and lives in worker code rather than in the topology, so a weighting bug is
invisible until an incident. **Per-campaign throttles** are required on top of either, because §7.5 shows
the correct rate is set by something outside this system entirely.

Choose separate topics with per-campaign throttles, and add a third class for time-sensitive
non-transactional traffic (a live stream starting) so the binary does not force everything into the
expensive lane. Let marketing scale into shared capacity while the transactional reservation stays hard.
Name the cost: "we'll use separate queues," without acknowledging that you are paying for two fleets, is
the answer of someone who has not operated one.

### 7.4 The audience snapshot, and what is evaluated when

A campaign takes 8 to 27 minutes to send, and during that window the audience changes: users unsubscribe,
users are deleted, users cross the segment predicate — in the nastiest case *because of this campaign*,
as when the segment is "has not opened the app in 7 days" and opening the push removes them from it.

**Streaming the segment query live** is always current and otherwise indefensible: not reproducible, no
stable denominator for progress, a user can enter mid-send and be selected twice, and the feedback case
turns the query into a moving target that may never terminate. **Snapshotting at start** materializes the
segment to an immutable manifest in object storage — 50 million IDs at 8 bytes is 400 MB, chunked into
5,000 files of 10,000 — with the URI and size on the campaign row and a persisted cursor: stable
denominator, restartable job, auditable record of exactly who was targeted, idempotent chunk replay.

But the snapshot alone is insufficient, and the resolution is the actual answer: **split the audience
question in two.** *Membership* — "is this user in the segment?" — is answered once, from the snapshot,
because it is a targeting decision and freezing it is what makes the campaign well-defined. *Suppression*
— opt-out, quiet hours, frequency cap, deletion, dead token — is answered per recipient at the moment of
send, against live state, because a user who unsubscribes at minute three of a twenty-seven-minute send
must not receive it at minute twenty. **Inclusion is snapshotted; exclusion is live**, and wherever the
two disagree the safe direction is not to send. The cost is one keyed preference read per recipient — at
§7.5's throttled rate, 31,000 cached reads/sec, free next to 400 MB of snapshot I/O. There is no excuse
for evaluating opt-out at trigger time.

**Quiet hours are a suppression predicate with a twist.** "Nothing between 22:00 and 08:00 local" needs
the device's timezone, and offers three policies: drop, hold until the boundary, or roll into a morning
digest. Holding is popular and manufactures a second thundering herd — 40 million users in one timezone
releasing at 08:00 — so release over a jittered window; a held-notification store with a due time is
literally Chapter 92's problem. Transactional traffic bypasses quiet hours entirely: a fraud alert at
3 a.m. is not a violation of the setting, it is why security is a separate row in the preference matrix.

### 7.5 The notification thundering herd

Under-discussed and genuinely dangerous: **a successful campaign is a self-inflicted denial of service on
your own product API.** Every push that lands is an invitation to open the app, and a tap is not one
request — a cold start authenticates, fetches config, loads the feed, fetches the deep-linked object,
clears the badge, and posts telemetry.

```
send rate at full fleet                     100,000 pushes/sec
tap-through rate                                  3%
peak-to-mean concentration of taps                2×
API requests per cold-start open                  8
induced load = 100,000 × 0.03 × 2 × 8    ≈ 48,000 RPS
```

Against a product API at 60,000 RPS peak with ~15,000 RPS of headroom, that is a 3.2× overshoot.
Autoscaling does not save you — it reacts in minutes and this arrives in seconds (Chapter 03, §6). The
campaign succeeds and the app is unusable for exactly the people who responded to it. Invert it to get
the send rate:

```
max send rate = 15,000 / (0.03 × 2 × 8)  ≈ 31,250 pushes/sec
campaign duration = 50e6 / 31,250        ≈ 1,600 s ≈ 27 minutes
```

**The send rate is set by the downstream API's headroom, not by the push fleet's capacity.** The fleet
could finish in 8 minutes; finishing in 8 minutes takes the site down. Carry this out of the chapter,
because the reflex — "the bottleneck is the provider" — is wrong, and the real bottleneck is a service
that is not part of the notification system at all.

Implementation: a distributed token bucket (Chapter 04, §3) shared by all campaign fanout workers, with
refill driven by a live signal — product API p99 and CPU headroom — rather than a constant, because
tap-through varies by an order of magnitude between campaigns and a static rate is either unsafe or
needlessly slow. Add a circuit breaker that *pauses* the campaign when the API's error rate crosses a
threshold: a campaign finishing an hour late is a non-event, an outage is not. Two cheaper mitigations
stack with it — **stagger by timezone**, which spreads a global campaign over the natural 24-hour
rotation for free, and make the deep-linked screen cacheable, which cuts the 8 in the formula.

### 7.6 Device token lifecycle

Tokens are the one piece of state here you neither own nor control, and they decay.

**Rotation.** The OS reissues tokens on reinstall, OS restore, and device migration, so the client must
register on **every app launch**, not only on first install; an app that registers once becomes a fleet
of devices that silently stop receiving notifications over the following year. Registration also
refreshes `last_seen_at`, the only positive liveness signal available. **Uninstalls generate no event** —
the only signal is a `410` on the next send, so a token belonging to a user you never notify stays
"active" forever. At 3% monthly install churn with no cleanup:

```
live fraction after 24 months = 0.97^24 ≈ 0.48   →   ~52% of the table is dead
```

More than half of every campaign's provider calls are spent to receive a `410`: the 50-million-recipient
campaign is really 104 million requests, consuming the same connection capacity as real ones, so
neglecting token hygiene directly doubles campaign duration and therefore doubles §7.5's window. Both
cleanup paths are required — **reactive**, marking invalid on `410`/`404`/`400 BadDeviceToken`, and
**proactive**, retiring tokens unseen for 90 days, since the client re-registers on launch and an unseen
token means an unopened app, the least valuable send available.

**Multi-device fanout** is the other half. At 1.8 devices per user a notification is 1.8 sends, and
ringing every device is usually right for transactional traffic and usually wrong for marketing. Two
details appear only once the app is real. A notification read on one device should clear the badge on the
others, which means a *silent* push to the siblings — notifications generating notifications, on a path
that must be excluded from the dedup key or it deduplicates against the original. And `collapseKey`
(FCM) / `apns-collapse-id` (APNs) lets the provider replace an undelivered notification with a newer one,
which is how "3 new messages" avoids becoming three alerts: free deduplication of a kind you cannot do
yourself, because only the provider knows what is still undelivered.

### 7.7 Templating, localization, and what "delivered" honestly means

**Render at send time, not trigger time.** The template may be corrected mid-campaign, the locale may be
known only from the device row, and rendering 50 million payloads up front means storing them. Store
`templateId` plus a variables map and render in the send pipeline — but pin `templateVersion` on the
campaign, so a mid-flight edit does not produce two populations with different copy and no record of
which got which.

Per-channel constraints are hard limits and belong in the renderer, not in a review comment. **APNs and
FCM payloads cap at 4 KB**; over is a `413`, which is terminal, so the notification is simply lost —
truncate deterministically. **An SMS segment is 160 characters in GSM-7 but 70 in UCS-2**, and one emoji
or curly apostrophe forces the whole message to UCS-2: a copywriter adding "🎉" silently turns a
one-segment message into three, tripling campaign cost while changing nothing in the preview, so compute
segment count in the authoring tool. **Email** needs a plain-text alternative, a `List-Unsubscribe`
header, and a per-recipient unsubscribe token. **Localization** is ICU message format, not concatenation,
because plural rules and word order differ; the fallback chain is device locale → user preference →
account default → `en-US`, and a missing translation renders the fallback, never the key.

Now the honest part. The funnel is:

```
accepted by intake         observable, exact
accepted by the provider   observable, exact      ← the last honest number
delivered to the device    partially observable (receipts exist on some paths)
displayed on screen        NOT observable
seen by a human            NOT observable
opened                     observable, and biased
```

Push is explicitly best-effort: a device that is off, out of storage, in battery-saver mode, or has your
app's notifications muted will not display something the provider happily accepted, and no API tells you.
The one downstream signal is an app-reported open, and it undercounts badly, because most notifications
that "work" are read on the lock screen and dismissed without a tap.

So **report "accepted by provider" as your delivery metric and label it as such.** A dashboard reading
"delivered: 49.7M" when it means "APNs returned 200 for 49.7M requests" will be quoted in a meeting as
evidence that 49.7 million people saw something — a claim your system cannot support. Treat open rate as
a product metric with known bias, useful for comparing campaigns and never as an absolute. Saying this in
an interview is worth more than another five minutes of architecture.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| APNs or FCM outage | Push stalls, queue depth grows | Backoff with jitter; the queue absorbs it (bounded — know the bound); fall back to in-app and email for high-value types; **never** fail intake |
| Redis dedup store lost | Claims all succeed; duplicates possible for the TTL window | The safe failure direction here; replicate it, but never let dedup be a hard dependency that blocks sending |
| Campaign targeted at the wrong segment | Millions of wrong, unrecallable notifications | Mandatory dry run against the snapshot; approval above an audience threshold; pause that takes effect within one chunk |
| Fanout worker crash mid-chunk | Chunk reprocessed | Persisted cursor plus idempotency claim make replay safe by construction |
| Dedup key gains a timestamp (a bug) | Every retry becomes a new notification | Contract-test the derivation; alert on same-user-same-type send rate |
| Provider credentials expire | 100% failure on one channel | Classify `401` separately and page; monitor key expiry as a scheduled check |
| Token table full of dead tokens | Campaign duration and provider cost double | Reactive `410` reaping plus 90-day proactive retirement (§7.6) |
| Campaign induces API overload | Product outage during your best campaign | Governor paced by downstream headroom; circuit-break to pause (§7.5) |
| DLQ replayed late | Stale notifications delivered | Per-message TTL enforced by the replay tool |
| Quiet-hours release spike | Herd at 08:00 in each timezone | Jittered release window |

**Monitoring:** queue depth per priority class (the leading indicator for everything else); provider
response-code distribution by provider and channel — a rising `410` means client registration broke, a
rising `429` means the governor is set too high; dedup claim hit rate, whose sudden drop means the key
derivation changed and duplicates are shipping *right now*; transactional trigger-to-provider p99 tracked
separately from marketing; campaign progress against snapshot size; DLQ growth, alerting on any nonzero
rate. The most useful dashboard is §7.7's funnel with absolute counts at each stage, because every
interesting failure appears as a drop between two specific stages.

---

## 9. Common mistakes

1. **Sending synchronously from the triggering service.** Couples product write availability to a third
   party and makes fanout latency the user's problem. The queue is not an optimization; it is the design.
2. **One queue for everything.** The 500-second head-of-line delay in §3 means password resets do not
   arrive during campaigns — and it is invisible in testing, because test campaigns are small.
3. **Retrying a `410`.** The most common real bug in production notification systems. A dead token is
   data to repair, not a request to retry, and the waste compounds silently until half your provider
   calls accomplish nothing.
4. **Evaluating preferences and quiet hours at trigger time.** On a 27-minute campaign this honors an
   unsubscribe up to 27 minutes late — a compliance problem, not a latency problem.
5. **No idempotency, or a key containing a timestamp or attempt number.** Either way duplicates reach
   humans. The key must be derivable by every stage from data every stage already has.
6. **Claiming you can measure delivery.** You measure provider acceptance. Presenting that as "delivered
   to the user" is a claim the architecture cannot support, and anyone who has run one of these notices.
7. **Ignoring the tap-back load.** Designing the send path perfectly and taking down the product API when
   the campaign works. Send rate is bounded by downstream headroom, not fleet capacity.
8. **Treating the in-app inbox as a rebuildable cache**, when it holds read state that exists nowhere
   else — or **modeling preferences as one boolean**, which cannot express what users ask for and leaves
   a later migration with no correct default.

---

## 10. Variants

**Transactional email platform (SES, SendGrid).** Same pipeline, one channel, and the interesting parts
move to reputation: dedicated IP warm-up, bounce and complaint feedback loops that must suppress
addresses within hours, SPF/DKIM/DMARC alignment, and per-tenant isolation so one customer's spam does
not poison another's deliverability. The suppression list becomes the most important table in the system.

**Outbound webhooks.** Notifying *servers* rather than people — structurally identical, with three
differences: the recipient is an arbitrary URL that may be slow or hostile, requiring per-tenant
concurrency limits and SSRF protection; payloads are signed with a per-tenant secret and a timestamp to
prevent replay; and ordering is often demanded, which forces per-endpoint serialization and reintroduces
head-of-line blocking deliberately.

**Alerting and paging.** Small audience, extreme latency and reliability requirements, and an escalation
state machine — notify, wait, escalate, repeat until acknowledged. The fanout problem vanishes, replaced
by a scheduling problem (Chapter 92) and by deduplication of *alerts* rather than sends (Chapter 73).

**Chat message push (Chapter 30).** Push is the fallback for a client with no open socket, so the
interesting decision is the handoff: deliver over the socket if connected, push if not, reconcile so the
user does not get both. Collapse keys and badge synchronization dominate. If in-app becomes the *primary*
channel rather than a fourth one, the inbox turns into a per-user materialized feed with read state —
Chapter 20's fanout math under §5's durability constraint.

---

## 11. Further reading

- Chapter 03, §3–§5, for delivery semantics, idempotency keys, retries, and DLQs in general form; Chapter
  02, §8, for why native push is the only option when the app is closed; Chapter 04, §3, for the token
  bucket behind the dispatch governor; Chapter 92, for the scheduled and quiet-hours-delayed send path
- Apple, [Sending notification requests to APNs](https://developer.apple.com/documentation/usernotifications/sending-notification-requests-to-apns)
  — the authoritative status-code table, including the `410` semantics this chapter leans on
- Google, [Firebase Cloud Messaging — send messages and error codes](https://firebase.google.com/docs/cloud-messaging/send-message)
- [RFC 8030](https://datatracker.ietf.org/doc/html/rfc8030) (HTTP Web Push),
  [RFC 8291](https://datatracker.ietf.org/doc/html/rfc8291) (message encryption), and
  [RFC 8292](https://datatracker.ietf.org/doc/html/rfc8292) (VAPID) — Web Push is the only one of the
  three push transports that is an open specification
- GSM 03.38, for the GSM-7 versus UCS-2 boundary that determines SMS segment count and cost
