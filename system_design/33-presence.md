# Chapter 33 — Presence / Online Status

> **Prerequisites:** Chapters 02 (§8 client-push protocols, TTLs), 04 (§3 rate limiting), 30 (connection
> fleets — presence is usually a passenger on that infrastructure)
> **Patterns:** heartbeat with TTL, derived state, pull-on-demand, deliberate staleness

---

## 1. The problem

Next to each contact in a messaging app there is a green dot, or the words "last seen 14 minutes ago," or
"typing…". That is the entire feature.

It is a small problem attached to a large system, and it is asked because it is small: there is no
interesting storage question, no interesting consistency question, and no algorithm to recall. What it
tests is whether you will build the obvious design — a durable online flag, pushed to everyone who cares
— without noticing that the obvious design costs more than the chat system it decorates.

So state the interesting thing plainly, in the first minute: **the notable property of presence is how
much of it you are allowed to get wrong.** A green dot thirty seconds stale is not a bug. A dot that stays
lit for a minute after someone's battery dies is not a bug. A contact whose status you never fetched
because they were off screen is not a bug. Nobody has filed a ticket about presence accuracy, and no
downstream system may make a decision from it — if a message is routed, a call placed, or a notification
suppressed because a flag said "online," that system is broken, because the flag is a guess about a phone
in someone's pocket.

**The property that makes it hard** — genuinely the only one — is that presence is a **many-to-many
derived signal over a social graph**, so the naive push design's cost is the product of two large numbers:
how often people's status changes, and how many people might care. §7.1 derives that product and finds it
exceeds the message traffic of the entire chat system by an order of magnitude, for information discarded
99.9% of the time. Every good decision here comes from refusing to pay it.

---

## 2. Requirements

### Functional

1. Show whether a contact is currently online.
2. Show "last seen" when they are not.
3. Show a typing indicator inside an open conversation.
4. Let a user disable both presence and last-seen sharing, with the setting applying symmetrically.

Defer, but name: rich status ("in a meeting," "away," custom text), per-device presence detail
("online on desktop"), and presence within very large groups, which is a different problem (§10).

### Non-functional

- **Freshness** — a status change visible within about 30 seconds. Not 1 second. Deriving that number
  rather than assuming "real time" is most of the design.
- **Accuracy — explicitly best effort.** False positives (online for up to ~30 seconds after a device
  vanishes) are accepted by design; false negatives should be rare.
- **Durability — none for presence.** It is derived from live connections and fully recomputed within one
  heartbeat interval of any data loss. Last seen is different (§7.3) and is the only durable thing here.
- **Cost** — presence must be a small fraction of the messaging system it accompanies. This is the real
  non-functional requirement, and the one that eliminates the naive design.
- **Privacy** — presence is behavioral data. Suppression must be enforced server-side and symmetrically:
  a user who hides their status does not get to see others', or the setting is a one-way mirror.

### Explicitly out of scope

Presence federation across organizations, rich custom statuses, and calendar-derived availability.

---

## 3. Estimation

Assume 1 billion registered users, 200 million concurrently online at peak, and an average of 200
contacts per user.

**Heartbeat write load**

With a heartbeat every 10 seconds:

```
200,000,000 online / 10 s = 20,000,000 presence writes/sec
```

At a pipelined ~500,000 simple ops/sec per Redis node:

```
20e6 / 500e3 = 40 nodes, purely to record who is online
```

Forty machines to maintain a green dot is a real cost, and it scales inversely with the interval: 14
nodes at 30 seconds, 80 at 5. **The interval is the cost dial**, and §7.2 sets it deliberately rather
than defaulting to "as fast as possible." Memory, by contrast, is nothing —
`200M keys × ~100 B ≈ 20 GB` — which establishes that presence is a *throughput* problem, not a storage
one, the opposite of most chapters in this book.

**Status transitions — the input to the push-versus-pull decision**

Mobile connectivity flaps constantly: backgrounding, elevators, Wi-Fi handing off to cellular. Assume 20
transitions per user per day, and compare the two delivery strategies.

```
transitions:  1e9 × 20 / 86,400            ≈    231,000/sec
push cost:    231,000 × 200 contacts       ≈ 46,000,000 events/sec
```

Chapter 30's entire chat system at this scale is 3.9 million message deliveries per second. **Presence
pushed to contacts is twelve times the traffic of every message in the product**, to render a dot. This
is the number that constrains the design; put it on the board.

Presence only matters while someone is looking at it. Of 200 million online users, assume 5% have a
contact or conversation list on screen, each showing about 20 rows:

```
viewers:      200M × 0.05                  =  10,000,000
pull cost:    10M / 10 s polling interval  =   1,000,000 batched requests/sec
              × 20 ids per batch           =  20,000,000 key reads/sec
```

One million requests per second against a cluster already sized for the writes — **forty-six times
cheaper**, and, more importantly, a cost that scales with how many people are *looking* rather than with
the size of the social graph. §7.1 develops why that difference matters more than the ratio.

---

## 4. API

```
# Maintaining presence: preferably no endpoint at all. The chat WebSocket's existing
# keepalive (Chapter 30) tells the connection server who is present; it batches the writes.
POST /v1/presence/heartbeat          -> 204     # fallback for clients with no live socket

GET  /v1/presence?ids=u_12,u_88,...             # capped at 100 ids
  ->  200 {"u_12": {"state":"online"},
           "u_88": {"state":"offline","lastSeen":1725300000},
           "u_91": {"state":"hidden"}}

PUT  /v1/settings/presence           -> 204
  body: {shareOnline: bool, shareLastSeen: "everyone"|"contacts"|"nobody"}

# Typing, over the existing chat socket:
C→S  TYPING  {conversationId, state: "start"|"stop"}
S→C  TYPING  {conversationId, userId, expiresAt}
```

Three decisions worth defending.

**Reads are batched and capped.** The client asks about the contacts it is rendering, in one request,
never one at a time. This turns 20 million key reads per second into 1 million requests per second, and
it is why pull is affordable. The cap stops a client asking about all 200 contacts when 20 are visible —
the API should make the cheap usage the natural one.

**There is deliberately no presence subscription endpoint.** "Subscribe to my contacts' presence and
stream me updates" is the naive push design wearing an interface, and it commits you to §3's 46 million
events per second. Refusing to offer it *is* the design decision; drawing a `presence.subscribe` frame
usually means it was never costed.

**`hidden` is a distinct state from `offline`.** A user who disables sharing must not be reported offline,
because "offline" is a claim observers reason about ("they've been offline all day"). An explicit third
state lets the client render "no status" honestly and stops the privacy setting leaking through inference.

---

## 5. Data model

```
Redis (the whole live data model):

presence:{user_id}   →  {devices: 2, since: 1725300000}    EX 30
typing:{conv}:{user} →  1                                   EX 5

Durable store (one column on the existing user row):

users.last_seen_at   TIMESTAMP     # written on disconnect and periodically; see §7.3
users.share_online   BOOLEAN
users.share_last_seen ENUM
```

**Absence of the key is the offline state.** No `offline` value, no tombstone, no cleanup job: expiry is
the state machine. A process that dies without disconnecting — the *normal* way mobile clients disappear
— resolves itself in one TTL with no code involved.

**TTL is three heartbeat intervals.** At 10 seconds and 30, a client survives two consecutive lost
heartbeats before being declared offline. One missed heartbeat is common on mobile; three in a row is
evidence. TTL equal to the interval produces a status that flickers constantly, which users notice far
more than being 30 seconds late.

**Presence is never written durably.** Losing the entire Redis cluster costs one heartbeat interval of
correctness. Anything durable would need cleaning up, and cleanup is what TTLs exist to avoid.

**Multi-device is a counter, not a set.** Online on phone and laptop is online; losing the laptop is still
online. Store the count of live connections and treat presence as an OR across devices. Per-device detail
means a small hash — but ask whether anyone reads it, because it multiplies §3's write volume by the
average device count.

---

## 6. Architecture, derived

### Attempt 1: an `is_online` flag on the user row

Set it true on connect, false on disconnect.

It fails on the event that never arrives. A phone whose battery dies, whose app is force-killed, or which
walks into a tunnel sends no disconnect, so the flag stays true forever. This is the most common presence
bug in shipped products — users permanently lit green — and it is unfixable within the design, because
nothing distinguishes "still connected" from "gone without saying goodbye." The secondary failure is
cost: 231,000 durable row updates per second on a table that also serves profile reads, for a value with
no durability requirement at all.

### Attempt 2: heartbeat plus TTL

The client — or the connection server holding its socket — writes `presence:{user}` with a 30-second TTL
every 10 seconds. Reading is a key lookup; a miss means offline.

The stuck-online bug is gone by construction, and it is worth being precise about why: **the system never
has to observe the departure.** Liveness is asserted continuously by the party that knows, and its absence
is self-evidently offline. Nothing detects a failure, runs a sweeper, or reconciles. This is the right
core, and §3 prices it at ~40 Redis nodes.

### Attempt 3: push presence changes to contacts

When a user's presence changes, look up their contacts and push an event to each one online.

```
231,000 transitions/sec × 200 contacts ≈ 46,000,000 events/sec
```

Twelve times the message traffic of the entire chat product, of which §7.1 shows ~99.9% is discarded by
clients not rendering that contact. It also inherits Chapter 20's power law: a user with 100,000 contacts
generates 100,000 pushes every time they walk past a dead zone. Reject it — with the arithmetic, not with
intuition.

### Attempt 4: pull, scoped to what is on screen

Invert it. Clients ask about the contacts they are rendering, in one batched request, every 10 seconds,
and only while that view is visible: `10M viewers / 10 s = 1M batched requests/sec`.

The cost now tracks attention rather than the social graph (§7.1), and when the app is backgrounded the
polling stops and the cost goes to zero — something push can never do, because the *sender* decides when
to send. One refinement: in the conversation currently open, presence matters more and the contact set is
one person, so push that one over the existing chat socket. **Pull for the list, push for the open
conversation** bounds the expensive case to a single contact.

### Final architecture

```
                      writes presence in batches
   ┌─────────────────────────┐          ┌──────────────────────────────┐
   │ Connection servers      │─────────►│  Redis presence cluster       │
   │ (Chapter 30 fleet)      │  pipelined│  presence:{user}  EX 30       │
   │  · socket keepalive IS  │  MSET     │  typing:{conv}:{user} EX 5    │
   │    the heartbeat        │  every 10s└──────────────┬───────────────┘
   └─────────────────────────┘                          │ batched MGET
                                                        │
   ┌─────────────────────────┐                  ┌───────▼───────────────┐
   │ Client                  │  GET /presence?  │  Presence service     │
   │  · polls only visible   │─────ids=…───────►│   · privacy filter    │
   │    rows, every 10 s     │◄─────────────────│   · contact check     │
   │  · stops when back-     │                  └───────────────────────┘
   │    grounded             │
   │  · open conversation:   │      TYPING / presence for one peer
   │    pushed over the      │◄─────────────────  chat WebSocket
   │    chat socket          │
   └─────────────────────────┘
                                   on disconnect
   Connection server ─────────────────────────►  users.last_seen_at  (durable, §7.3)
```

The thing to notice: **there is no presence pipeline.** No queue, no fanout workers, no subscription
registry. The two arrows that exist are a batched write from infrastructure that already knows the answer
and a batched read from clients that already know what they need. Presence is cheap when you refuse to
build it as a distribution problem.

---

## 7. Deep dives

### 7.1 Why push-based presence does not scale

The full derivation, because it is the content of the chapter.

```
transitions/sec          = 1e9 users × 20/day / 86,400 ≈ 231,000
events/sec if pushed     = 231,000 × 200 contacts      ≈ 46,000,000
```

Then ask what fraction is useful. A presence event changes a pixel only if the recipient is online, has a
list on screen, and that specific contact is among the rows rendered:

```
P(recipient online)                = 200M / 1B         = 0.20
P(list on screen | online)         ≈ 0.05
P(this contact among ~20 of 200)   = 0.10
                                     ─────
useful fraction                    ≈ 0.001
```

**One event in a thousand changes anything.** The system would move 46 million events per second to
deliver about 46,000 useful ones, using the full fanout machinery of Chapter 20 — reverse index, workers,
queue — to distribute data whose value expires in seconds.

There is a second, worse property. Push cost is `transitions × contacts`, and **neither factor is under
your control**. Transition rate is set by mobile network quality, which degrades in exactly the conditions
where your system is already struggling — a subway train losing signal produces a synchronized burst of
offline events followed a minute later by a synchronized burst of online ones. Contact count is power-law
distributed, so one popular account toggling generates 100,000 pushes, and Chapter 20's threshold trick
does not rescue you, because unlike a tweet the event is worthless: you would be building a hybrid fanout
system to distribute a boolean.

Pull cost is `viewers × visible contacts / interval`, and **every factor is controllable**: the client
stops polling when backgrounded, the interval is a config value, the visible count is bounded by screen
size. That is the real argument. The 46× ratio is a snapshot; the difference in *what the cost is
proportional to* is why the decision does not reverse at another scale.

The honest cost of pull: one extra polling interval of staleness, and a floor of 1 million requests per
second even when nothing changes — you pay for looking rather than for changing. Since most contacts are
offline most of the time, and offline is the cheapest possible answer to serve, that is a good trade.

### 7.2 Staleness as a deliberate product choice

Every parameter here is a staleness decision, and they should be made explicitly rather than inherited.

```
heartbeat interval  10 s  → write load, battery, radio wakeups
TTL                 30 s  → worst-case false "online" after a device vanishes
poll interval       10 s  → worst-case delay in seeing a change
worst-case total   ≈ 40 s  (TTL expiry + one poll interval)
```

Forty seconds of potential wrongness. Argue the other side properly: a 2-second heartbeat with a 6-second
TTL gives near-real-time presence, and there are products where that matters. It costs 5× the write fleet
(200 nodes rather than 40) and 5× the mobile radio wakeups — a genuine battery complaint, not a
theoretical one, because each wakeup holds the cellular radio in a high-power state for seconds after the
packet — and it makes the dot flicker on every network hiccup, which users read as a broken app.

Choose 10/30, then be specific about the failure it buys: **for up to 30 seconds after someone's phone
dies, the system confidently shows them online.** That is a real lie told to real users on purpose, and
it is acceptable because its only consequence is that someone sends a message that gets delivered a
little later than they expected — which would have happened anyway.

The corollary is a hard rule: **nothing downstream may branch on presence.** Do not suppress a push
notification because presence says online, do not route a call differently, do not skip a delivery path.
Presence is a rendering hint with a 30-second error bar, and any system treating it as fact fails in
exactly the window where it is wrong. Chapter 30 gets this right: it decides whether to send a push
notification from whether a socket write actually succeeded, not from a presence lookup.

### 7.3 "Last seen" is a separate, cheaper, and more sensitive problem

Last seen looks like presence and is nothing like it.

**It is durable, and presence is not** — a single timestamp per user on the existing user row, written
when a connection ends and periodically while it is open so a crash does not lose the session. No TTL, no
heartbeat, no fleet. 231,000 durable writes/sec is too much for a user table, so coalesce: write each
user's last-seen at most once per minute, cutting it to tens of thousands per second at a resolution
nobody notices.

**It is read only when presence says offline**, which makes it strictly cheaper than presence: served
from the same batched query, from a value that changes rarely and caches perfectly.

**It is far more sensitive.** A green dot reveals a moment; a last-seen history reveals a sleep schedule,
a work pattern, and whether two people were online together. That is why §4's privacy settings are more
elaborate than the feature seems to warrant, and why they must be enforced server-side — a client-side
filter is bypassed by anyone calling the API directly.

Symmetry is the part that gets missed. A user who hides their last seen but still reads everyone else's
has a one-way mirror, and it will be adopted by exactly the people you least want to advantage. Enforce
reciprocity: hiding yours hides theirs from you. It is a privacy rule that has to live in the read path
rather than on the settings screen.

### 7.4 Typing indicators

The highest-frequency, lowest-value signal in the product, and a good final test of whether the chapter's
principles have landed.

A typing event fires every few seconds while someone composes, and there are far more compositions than
messages — but the scope is what saves it. A typing indicator goes to **the participants of one open
conversation**, usually one other person, not to 200 contacts. The fanout that made presence expensive
does not exist here.

So the design is one line: send `TYPING {start}` over the chat socket (Chapter 30, §4), route it to the
conversation's participants, never persist it, let it expire. Three rules keep it cheap and correct:

- **Throttle at the client.** Send `start` at most once every few seconds, not per keystroke. The
  per-keystroke version is the classic bug and multiplies volume twentyfold for no visible difference.
- **Expire at ~5 seconds, on both sides.** Never rely on receiving a `stop`, for the same reason presence
  never relies on a disconnect: the message signalling the end is the one most likely to be lost. A TTL
  makes the missing `stop` a non-event.
- **Never store it, and never deliver it late.** An indicator arriving after the message it predicted is
  worse than useless. Shed it first under load; nobody will miss it.

Group conversations multiply the cost by participant count, so cap it: above a small size, stop sending
individual events and show "several people are typing," or drop the feature. That is Chapter 31's
aggregation move at a much smaller scale, which is the point of noticing it here.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Client vanishes without disconnecting | Shows online for up to the TTL | Accepted by design; the TTL *is* the mitigation, which is why no disconnect handler is required for correctness |
| Redis presence cluster lost | Everyone appears offline | Self-heals within one heartbeat interval as connection servers re-assert; degrade the UI to "no status" rather than to "offline" |
| Heartbeat storm after a mass reconnect | 200M writes arrive in a burst | Jitter the heartbeat phase per client; the connection servers batch, so the burst is bounded by fleet size, not user count |
| A hot key (a very popular account) | One shard reads far more than its peers | Presence values are tiny and immutable within a TTL; cache them briefly in the presence service's local memory |
| Poll traffic during an outage of the chat socket | Clients fall back to polling and load rises | Rate-limit per user (Chapter 04, §3); presence is the correct thing to shed first |
| Privacy setting not applied on a cached read | A hidden user's status leaks | Filter after the cache, never before; cache the raw value, apply the policy per requester |
| Clock skew between servers | `lastSeen` timestamps inconsistent | Server-assigned timestamps from a single source, as in Chapter 30, §7.3 |

**Monitoring:** presence write rate versus concurrent connections (they should track exactly; divergence
means heartbeats are being lost or double-counted); Redis key count as an independent estimate of online
users; batched-query size distribution, where a rising tail means a client is asking about contacts it is
not rendering; poll request rate per user, which should be flat; and the ratio of presence reads to chat
message sends, which is the single number that tells you whether this feature is staying cheap.

---

## 9. Common mistakes

1. **Storing presence as a durable boolean.** The disconnect is the event that never arrives, so the flag
   sticks on and users are permanently green. TTL expiry removes the failure rather than handling it.
2. **Pushing presence changes to every contact** — 46 million events per second, twelve times the entire
   chat system's traffic, of which one in a thousand changes a pixel.
3. **Polling presence for all contacts rather than the visible ones.** A tenfold multiplier on the read
   path that renders nothing, and the reason the API caps the batch.
4. **Setting the TTL equal to the heartbeat interval**, so one lost heartbeat flips a user offline; the
   flicker is far more noticeable than a 30-second delay.
5. **Letting other systems branch on presence.** Suppressing a push notification because presence says
   "online" fails in exactly the 30-second window where presence is wrong.
6. **Treating "last seen" as the same feature.** It is durable rather than derived, written on a different
   trigger, read on a different condition, and far more privacy-sensitive.
7. **Enforcing privacy in the client.** A hidden user must be hidden by the presence service, and
   reciprocally, or the setting is decorative.
8. **Sending a typing event per keystroke**, and depending on a `stop` that will sometimes be lost rather
   than on a short TTL.

---

## 10. Variants

**Slack / Teams (workplace).** Smaller graphs — hundreds per workspace, not billions globally — and a
narrower attention window, so §3's arithmetic is far more forgiving: push to a whole workspace's active
members is genuinely affordable below a few thousand people. It also adds calendar-derived and manual
statuses, which are *not* derived from connections and belong in the durable user record beside last seen.

**Discord (large servers).** A 50,000-member server makes member-list presence harder than in any 1:1
messenger, and the answer is Chapter 31's: do not send everyone everything. Deliver presence only for the
slice of the list scrolled into view and show an aggregate ("1,204 online") for the rest.

**Multiplayer games (Chapter A1).** Presence is authoritative rather than advisory — the server knows who
is in a session because it is simulating them — and staleness is unacceptable because game logic branches
on it. A useful contrast for why this chapter is allowed to be so casual.

**Device and service liveness monitoring (Chapter 73).** Structurally identical (heartbeat, TTL, absence
means down), but the consumer is an alerting system rather than a person, so tolerance for a false "up"
inverts and the detection window tightens dramatically.

---

## 11. Further reading

- Chapter 30, §7.1, for the connection fleet that presence rides on
- Chapter 31, §7.2, for aggregation as an alternative to delivery, which is the answer for large-group
  presence
- [RFC 6121 — XMPP: Instant Messaging and Presence](https://datatracker.ietf.org/doc/html/rfc6121), whose
  presence model is subscription-and-push; reading it against §7.1 is a good exercise in why a protocol
  designed for federated, modest-scale deployments does not transfer to a billion-user graph
- [Redis — key expiration](https://redis.io/docs/latest/commands/expire/), including the distinction
  between lazy and active expiry, which determines how promptly a "went offline" is actually observable
- Hayashibara et al., "The φ Accrual Failure Detector" (SRDS 2004) — the rigorous treatment of inferring
  liveness from heartbeat arrival, and worth reading to see how much machinery you are correctly choosing
  to skip here
