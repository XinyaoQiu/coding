# Chapter 31 — Live Comments / Live Stream Chat

> **Prerequisites:** Chapters 02 (§8 client-push protocols, CDNs), 03 (partitioned logs, backpressure),
> 30 (persistent connections, connection registries — this chapter is defined by its differences from it)
> **Patterns:** broadcast fanout, tiered distribution, sampling as a design primitive, ephemerality

---

## 1. The problem

A live stream is playing. Along one edge of the screen, comments from other viewers scroll past. A viewer
types something and it joins the stream. Reactions — hearts, fire, applause — float up continuously.
Twitch chat, a YouTube or Instagram Live comment rail, a Facebook Live reaction stream, a sports app's
in-play chat.

It looks like Chapter 30 with a different skin, and every candidate who treats it that way produces a
design that cannot be built. The two systems share exactly one component — a fleet of persistent
connections — and disagree about everything else.

Chapter 30 is **point-to-point**: a message has a specific, enumerable, durable set of recipients, each of
whom must eventually receive it, once, in order, even after a month offline. This chapter is
**broadcast**: a comment has an audience rather than a recipient list, that audience is transient and
unenumerable, and a viewer who was not watching when a comment was posted has no claim on it. Every
requirement that made Chapter 30 expensive — durability, exactly-once delivery, per-recipient cursors,
receipts, ordering guarantees — evaporates here. What replaces them is worse.

**The property that makes it hard:** a single room can have millions of concurrent viewers, and the
comment volume a crowd that size produces, multiplied by the size of the crowd it must reach, is a
quadratic no infrastructure can satisfy. The §3 arithmetic comes out at terabytes per second for one
room. No distribution topology fixes this, because the problem is not delivery, it is the product:
**nobody can read a thousand comments per second anyway.** The correct response is not an engineering
optimization but a decision about what to deliver — you sample, you aggregate, and you accept that no two
viewers see the same chat. Everything here follows from being willing to say that out loud.

---

## 2. Requirements

### Functional

1. Post a comment to a live stream.
2. Receive comments from other viewers, in near-real time, while watching.
3. Post and receive lightweight reactions (hearts, emoji), which are far more frequent than comments.
4. On joining mid-stream, see some recent context rather than an empty rail.
5. Moderation: filter prohibited content before it is shown, and let creators and moderators remove
   comments and ban users.

Defer, but name: threaded replies (they are a different product — a comment rail scrolling at 20 items
per second cannot support a reply tree that anybody can follow), direct messaging between viewers,
comment search, and monetized highlighted messages, which are a genuine variant and are treated in §10.

### Non-functional

- **Delivery latency** — p99 under 2 seconds from post to appearing on other viewers' screens. That this
  is *looser* than Chapter 30's 500 ms is instructive: the video is itself 10–30 seconds behind real time
  on HLS or DASH, so a comment arriving a second late is still ahead of the moment it reacts to. The
  slack is a resource, and §7.4 spends it on moderation.
- **Durability — none required.** A dropped comment is gone and nobody can tell: no acknowledgment to
  honor, no cursor to advance, no inbox to reconcile. State this early, because it licenses everything
  cheap in the design exactly as "eventual consistency is fine" licensed Chapter 20's fanout.
- **Delivery completeness — explicitly not guaranteed.** No viewer is promised every comment. That is a
  requirement, not a failure (§7.2).
- **Ordering — best effort within a room, nothing more.** Comments displayed within a second or two of
  each other, in any order, are indistinguishable to a human reading a scrolling rail.
- **Availability** — the rail is a secondary surface. If it degrades the stream must keep playing;
  comments failing while video plays is acceptable degradation, the reverse is not.
- **Scale** — a single room with 10 million concurrent viewers. The design is for the largest room, not
  the average one.

### Explicitly out of scope

Video ingest, transcoding, and delivery (Chapter 40); stream recommendation; payments; and the analytics
pipeline that counts engagement (Chapter 70).

---

## 3. Estimation

Assume a platform with 50 million concurrent viewers across all streams, and a single flagship stream
with **10 million concurrent viewers**.

**Comment production**

Participation in a large room is low; most viewers never type. Assume 1% comment per minute, and that a
dramatic moment — a goal, a knockout, a reveal — collapses 5% of them into five seconds:

```
steady: 10,000,000 × 0.01 / 60 s ≈  1,667 comments/sec
burst:  10,000,000 × 0.05 /  5 s = 100,000 comments/sec
```

A **sixtyfold burst**, in a single room, with no warning and no way to smooth it. Reactions are worse —
they cost a tap, so a comparable moment produces on the order of a million events per second. §7.3 is
about surviving this, and the fact that reactions are counted rather than delivered is why they are
survivable at all.

**Naive delivery — the number that ends the naive design**

```
10,000,000 viewers × 1,667 comments/sec = 1.67 × 10^10 deliveries/sec
× 150 B                                 ≈ 2.5 TB/sec, one room, steady state
```

Two and a half terabytes per second of *text*, before the burst. No CDN, fanout tree, or budget makes
this deliverable. It is not a hard problem, it is an impossible one, and saying so immediately — rather
than proposing an architecture that would deliver it — is most of what this chapter tests.

**The per-viewer view, which is where the answer comes from**

```
1,667 comments/sec × 150 B ≈ 250 KB/sec of text per viewer
video bitrate at 1080p     ≈ 3 Mbps ≈ 375 KB/sec
```

The rail would be two-thirds the bandwidth of the video, on mobile connections, to deliver something no
human can process: a comfortable reading rate is about 5 comments per second, and 20 per second is
already the unreadable blur people describe as "chat is flying."

**Cap the delivered rate at 20 comments/sec** and the arithmetic changes character:

```
per viewer:   20 × 150 B     = 3 KB/sec
one room:     10M × 3 KB/sec = 30 GB/sec ≈ 240 Gbps
same room's video egress: 10M × 3 Mbps   = 30 Tbps
```

**The comment rail is 1% of the video egress for the same audience.** A platform already delivering the
video can deliver the comments. The entire design problem is turning 2.5 TB/sec into 30 GB/sec, and the
mechanism is not compression or a better protocol — it is deciding that 1,647 of every 1,667 comments
will not be sent to any given viewer.

**Fanout fleet**

At 100,000 concurrent connections per node (Chapter 02, §8; SSE connections are cheaper than WebSockets,
being unidirectional and needing almost no receive buffer, but keep the planning number):

```
one room:       10M / 100k = 100 edge nodes
whole platform: 50M / 100k = 500 edge nodes
per node: 3 KB/sec in  →  100k × 3 KB/sec = 300 MB/sec ≈ 2.4 Gbps out
```

Comfortable on a 10 Gbps NIC, and the ratio is the striking part: 3 KB/sec in, 2.4 Gbps out, an
amplification factor of 100,000. That ratio *is* a broadcast system, and it is why the architecture is a
tree rather than a mesh.

**Storage**

```
1,667/sec × 150 B × 86,400 s ≈ 21 GB/day for the flagship room
last 500 comments per room   = 75 KB      ← all the live path actually needs
```

You *could* archive everything, cheaply. Nothing in the live path needs it: what it needs is a Redis ring
buffer of recent comments for late joiners. Archival is a separate, optional, offline concern (§7.6).

---

## 4. API

The transport is **Server-Sent Events**: a single long-lived HTTP response the server writes to
incrementally. Posting a comment is an ordinary HTTP POST on a separate, stateless request.

```
GET /v1/streams/{streamId}/comments
  Accept: text/event-stream
  Last-Event-ID: <commentId>              # browser sends this automatically on reconnect

  ->  event: comment
      id: 4471829300
      data: {"id":"4471829300","author":{"id":"u_88","name":"...","badge":"mod"},
             "text":"...","streamPos":18422}

      event: batch
      data: {"comments":[ ...up to 4... ]}     # coalesced; see §6 attempt 4

      event: aggregate
      data: {"windowMs":2000,"reactions":{"heart":42117,"fire":9803},
             "commentsInWindow":3341,"viewers":10214882}

      event: pin
      data: {"id":"...","author":{...},"text":"..."}    # creator or moderator highlight

      event: retract
      data: {"id":"...","reason":"policy"}              # best effort only; see §7.4

      : keepalive                                        # comment line every 15 s

POST /v1/streams/{streamId}/comments
  body: {clientMsgId, text, streamPos}
  ->   202 {commentId, ts}

POST /v1/streams/{streamId}/reactions
  body: {type, count}                     # client-batched: "7 hearts in the last 2 seconds"
  ->   202

GET  /v1/streams/{streamId}/comments/recent?limit=50
  ->   200 {comments: [...]}              # backfill on join, from the ring buffer

POST /v1/streams/{streamId}/moderation/ban    { userId, durationSec }   -> 204
DELETE /v1/streams/{streamId}/comments/{id}                              -> 204
```

Four things worth defending.

**SSE, not WebSocket.** The data flows one way. A viewer posts a comment perhaps once per stream and
receives thousands, so the send path is a rounding error that does not justify a bidirectional protocol.
SSE buys automatic browser reconnection, resumption via `Last-Event-ID`, and plain HTTP semantics that
traverse every proxy (§7.5). Reaching for WebSocket because Chapter 30 used one is the most common
mistake in this question.

**`POST /comments` returns 202 Accepted, not 201 Created.** Not pedantry: 201 asserts a resource exists
and, by implication, will be visible, while 202 says only that the comment entered the pipeline. Given
sampling, the honest statement is that most viewers will never see this comment. Choosing the status code
that matches the guarantee signals you understand what you built.

**Reactions are a separate endpoint carrying a `count`.** They are 100× more frequent than comments and
individually meaningless. The client batches locally — "seven hearts in two seconds" — and the server
never delivers a reaction to anyone, only a count in the periodic `aggregate` event. Running reactions
through the comment path multiplies your hardest problem by a hundred.

**`aggregate` is a first-class event type.** The answer to "there are 100,000 comments per second" is
partly "here are 20 of them" and partly "here is the shape of the rest." Viewer counts, reaction totals,
and comment velocity are cheap to compute and are the information a viewer actually extracts from a
firehose they cannot read.

---

## 5. Data model

The striking thing about this section is how little there is in it. Durability was scoped out, so there
is no primary store on the live path.

**Redis, per room:**

```
room:{id}:recent        LIST (capped at ~500)     # ring buffer for late-joiner backfill
room:{id}:reactions     HASH  type → counter      # incremented, read and reset per window
room:{id}:viewers       counter (approximate)
room:{id}:banned        SET of user ids, TTL per entry
user:{id}:rate          token bucket per room     # Chapter 04, §3
```

**Kafka, for ingest** (Chapter 93):

```
topic: comments
  key:       (room_id, bucket)   where bucket = hash(user_id) % 32
  partitions: many; a room occupies 32 of them
  retention: 1 hour — long enough to replay through a downstream failure, no longer
```

**Cold storage, optional and off the live path:** an archival sink writing accepted comments to object
storage for moderation review, legal retention, and VOD replay alongside the recorded video. Affordable
at 21 GB/day; simply not part of the delivery system, and keeping it off to the side is the point.

Two decisions to justify. **The room is keyed by `(room_id, bucket)`, not `room_id`.** A single Kafka
partition tops out in the low tens of thousands of messages per second, so the 100,000/sec burst would
swamp one; thirty-two buckets bring it to ~3,100/sec each. The cost is that ordering holds only within a
bucket, not within the room — disqualifying in Chapter 30, free here, because the requirement was already
"best effort within a second." **This is the clearest example in the book of a relaxed requirement
directly purchasing a scaling property.**

**The ring buffer is capped, and it is the only history.** Five hundred comments is about thirty seconds
of a busy room. A viewer joining mid-stream gets context, not archaeology — which is what they want.

---

## 6. Architecture, derived

### Attempt 1: reuse Chapter 30

Treat the room as a large group chat: enumerate members, look each one up in a connection registry, route
a frame per member, persist for offline delivery.

It fails on three counts, each isolating a difference between point-to-point and broadcast.

**The recipient set does not exist.** A room's "membership" is 10 million viewers who joined and left over
the last hour with no durable relationship to the room. There is no `conversation_members` table to read,
and building one means 10 million rows churning at the rate people open and close a tab.

**The registry lookup is per-recipient.** Chapter 30 does one Redis lookup per delivery; at 1.67 × 10^10
deliveries per second that is six orders of magnitude beyond what any registry serves. Broadcast systems
cannot afford per-recipient addressing, and that is the structural difference.

**Durability is pure cost.** Persisting per recipient, maintaining a cursor per viewer per room, and
replaying on reconnect buys a guarantee nobody requested and that sampling will contradict anyway.

### Attempt 2: room-based pub/sub with one fanout process

Drop per-recipient addressing. Viewers subscribe to a room; a process fans each comment out to its
subscribers.

Better in principle, and it dies on a hard limit: a single process holds ~100k connections (§3), and the
room has 10 million viewers. It cannot hold them, and even if it could, 1,667 comments/sec × 10M sockets
is 1.67 × 10^10 writes/sec from one machine.

### Attempt 3: shard viewers across edge nodes

One hundred edge nodes, 100k viewers each, every node subscribed to the room's stream. Now the *inbound*
side is trivial — the broker sends each comment 100 times instead of 10 million times, an amplification
of 100 rather than 10^7 — and the arithmetic moves to the edge node, where it is still fatal:

```
1,667 comments/sec × 100,000 sockets = 166.7M socket writes/sec, per node
× 150 B                              = 25 GB/sec ≈ 200 Gbps, per node
```

Two hundred gigabits per second out of one box, and 167 million write syscalls per second. Sharding
distributed the problem without shrinking it. **This is the moment the design must stop being an
infrastructure problem and become a product decision.**

### Attempt 4: deliver less — sample, then batch

Two independent reductions, and you need both.

**Sample.** Deliver at most 20 comments/sec to any viewer, selected from the room's stream (§7.2 covers
*how*, which is the interesting part):

```
20 comments/sec × 100,000 sockets = 2M socket writes/sec = 300 MB/sec ≈ 2.4 Gbps per node
```

Bandwidth is now comfortable. Two million writes per second on one box is not: at even a microsecond of
syscall overhead each that is two cores doing nothing but `write()`, and TCP framing overhead on
150-byte payloads is a large fraction of the traffic.

**Batch.** Accumulate for 200 ms and send one frame carrying up to four comments:

```
5 frames/sec × 100,000 sockets = 500,000 writes/sec per node
```

An ordinary event-loop workload. The cost is 200 ms of latency, drawn from a 2-second budget that §2
established is generous because the video is 10–30 seconds behind anyway. **Sampling fixes bandwidth,
batching fixes syscalls, and they are different problems** — proposing sampling and stopping solves half.

### Attempt 5: tier the distribution

One publisher writing directly to 500 edge nodes works at this size but not in reality: edge nodes span
regions, so a direct topology sends each comment across every transoceanic link once per node; the
publisher becomes a single point of failure with 500 outbound connections; and adding a node grows its
fanout. Insert a tier — the room's ingest publishes once per region to a **regional relay**, and each
relay fans out to the edge nodes in its region.

```
tiers = ceil( log_F(edge nodes) )     with F = 25:  log_25(500) ≈ 1.9  → 2 tiers
```

Two tiers, and the cross-region link now carries each comment **once per region** instead of once per
node — a 100× reduction in the most expensive bandwidth you buy. §7.1 develops the topology.

### Attempt 6: protect the write path

Sampling happens on the *delivery* side; ingest still receives all 100,000 comments/sec. The ingest tier
is stateless and horizontally scalable, but it fronts a Kafka topic whose per-partition ceiling is in the
low tens of thousands per second, and a naive `key = room_id` puts the whole burst on one partition.
Bucketed keys (§5), a per-user token bucket, and — as a last resort — load shedding at ingest, uniquely
acceptable here because a dropped comment during a 100k/sec burst is statistically invisible. §7.3.

### Final architecture

```
                                   ┌───────────────────────────────┐
  Viewer  ──POST /comments──────►  │  Ingest service (stateless)   │
                                   │   · auth, per-user rate limit │
                                   │   · synchronous cheap filter  │
                                   │   · assign id + server ts     │
                                   └──────────────┬────────────────┘
                                                  │ key = (room, bucket)
                                                  ▼
                                   ┌───────────────────────────────┐
                                   │  Kafka: comments (32 parts/room)│
                                   └───────┬───────────────┬────────┘
                                           │               │
                    ┌──────────────────────▼──┐   ┌────────▼───────────────┐
                    │ Async moderation        │   │ Room aggregator        │
                    │  (ML classifier, ~500ms)│   │  · sampling policy     │
                    │  → suppress / retract   │   │  · reaction counters   │
                    └─────────────────────────┘   │  · ring buffer (Redis) │
                                                  │  · 200 ms batching     │
                                                  └────────┬───────────────┘
                                                           │ ~20 msg/sec, 3 KB/sec
                                        ┌──────────────────┼──────────────────┐
                                        ▼                  ▼                  ▼
                                 ┌────────────┐     ┌────────────┐     ┌────────────┐
                                 │ Relay: US  │     │ Relay: EU  │     │ Relay: APAC│
                                 └──┬───┬─────┘     └──┬───┬─────┘     └──┬───┬─────┘
                                    │   │              │   │              │   │
                                   ┌▼┐ ┌▼┐            ┌▼┐ ┌▼┐            ┌▼┐ ┌▼┐
                                   │E│ │E│  …         │E│ │E│  …         │E│ │E│   edge nodes
                                   └┬┘ └┬┘            └┬┘ └┬┘            └┬┘ └┬┘   100k SSE each
                                    │   │              │   │              │   │
                                 viewers            viewers            viewers

  Ring buffer backfill:  Viewer ──GET /comments/recent──► Redis room:{id}:recent
```

The shape to notice: **one comment enters, one comment reaches each region, and the multiplication by
100,000 happens only at the last hop, on the machine that already owns the sockets.** Every tier above
the edge carries the room's stream exactly once. That is what a broadcast architecture is.

---

## 7. Deep dives

### 7.1 Tiered fanout: deriving the topology

The distribution layer is a tree, and its shape is determined by two numbers: how many leaf nodes must be
reached, and how many outbound connections a node can drive without becoming a bottleneck.

```
leaves needed  = concurrent viewers / connections per node = 50M / 100k = 500 edge nodes
fanout per tier F: a relay pushing 3 KB/sec to each of F children is bounded by
    connection management and failure handling, not bandwidth — F ≈ 25–50 is comfortable
tiers = ceil(log_F(500)) = 2
```

Two tiers, and it is not close: even at F = 25, a third tier is needed only past 15,000 edge nodes, or
1.5 billion concurrent viewers. **Say this.** The instinct on seeing "tiered fanout" is to draw a deep
tree; the derivation shows two levels suffice for any audience that exists, and each extra level costs a
hop of latency and a class of partial-failure states.

Three design points the tree forces.

**Tier boundaries follow failure and cost boundaries, not arbitrary arithmetic.** The reason the first
tier is the region is that the region is where the expensive, failure-prone link is. Sending each comment
across the Pacific once rather than once per edge node is the topology's single largest saving, and it
comes from aligning the tree with the network, not from the branching factor.

**Every node is a subscriber, and subscription is the state to manage.** An edge node holding 100k
viewers of 40 different streams holds 40 room subscriptions, torn down when streams end and established
quickly when one goes viral. Subscription churn — not comment volume — is what makes this layer
operationally interesting.

**The tree tolerates a lost interior node without a correctness story.** Because the payload is ephemeral,
a dead relay costs its subtree a few seconds of comments and nothing more: edge nodes reconnect elsewhere
and resume from whatever is current — no replay, no gap detection, no reconciliation. Compare the same
failure in Chapter 30, where a lost frame must be recovered from a cursor. **Ephemerality is not merely a
storage saving; it removes entire categories of failure handling from the distribution layer**, and that
is the strongest argument for designing this as its own system rather than as chat with a bigger fanout.

### 7.2 Sampling and aggregation: a product decision wearing infrastructure clothes

The §3 arithmetic says a viewer receives at most ~20 comments/sec out of 1,667. Which 20?

This is where the question gets interesting, because the answer is not an algorithm but a policy, and
different products choose differently and are all correct.

**Uniform random sampling** is the baseline. Every comment has an equal 1.2% chance of reaching any given
viewer, each viewer sees a different slice, and the rail feels alive and representative. It is trivial —
each edge node picks independently, needing no coordination — and it has one serious weakness: a viewer
who posts a comment probably does not see it, which reads as the product being broken. Fix that specific
case by always delivering a viewer their own comments, locally, at the edge.

**Weighted sampling** ranks before it selects: comments from accounts the viewer follows, from the creator
or moderators, in the viewer's language, that drew replies or reactions, from accounts with good history.
It produces a far better rail and costs a per-viewer decision — exactly the cost the architecture was
built to avoid, since the edge node can no longer write one identical frame to 100,000 sockets. The
workable compromise is **two-track delivery**: one shared sampled stream, identical for everyone on the
node and written once, plus a small per-viewer track carrying the handful of specifically relevant
comments. The shared track keeps the amplification factor; the personal track is low-volume by
construction.

**Aggregation** replaces delivery entirely for the highest-volume signals. Reactions are the pure case:
nobody wants a hundred thousand heart events, they want a counter climbing and an animation whose density
tracks it, so `{heart: 42117, fire: 9803}` every two seconds is strictly better information in a fraction
of the bytes. The same applies to comments during an extreme burst — above a velocity threshold, send
fewer sampled comments and more `{commentsInWindow: 100000}`, so the client can render "chat is going
wild," which is genuinely what a viewer wants to know at that moment.

Say that the choice is a product decision, then choose and name the cost. **Recommended: uniform sampling
with your own comments guaranteed, a small personalized track for followed accounts and creator activity,
and aggregation for reactions.** The cost is that no two viewers see the same chat, so viewers cannot
reliably reply to each other — a live rail is crowd noise, not a discussion — and any feature assuming
shared context (a poll, an in-joke) must ride the guaranteed track instead. Products that want real
conversation cap room size, which is what Twitch effectively does by having many rooms rather than one
enormous one (§10).

### 7.3 The write path when 100,000 people comment on the same second

Sampling protects delivery. Ingest still receives everything, and the burst is 60× steady state with a
rise time of under a second — far faster than any autoscaler reacts.

**Ingest is stateless and pre-provisioned.** A comment POST does auth, a per-user token bucket (Chapter
04, §3), a cheap synchronous content filter, ID and timestamp assignment, and a Kafka produce — a few
milliseconds, and 100,000/sec across a fleet sized for the event is unremarkable. Critically, **the burst
is predictable at the room level even though it is unpredictable at the second level**: a championship
final's peak concurrency is known days ahead, so scheduled pre-scaling covers the foreseeable cases and
headroom covers the rest.

**The partition ceiling is the real constraint.** A single Kafka partition sustains order 10^4
messages/sec, and `key = room_id` puts all 100,000 on one; bucketing to `(room_id, hash(user) % 32)`
brings each to ~3,100/sec, at the price of intra-room ordering that §5 already gave away.

**Backpressure and shedding.** If the pipeline saturates anyway, shed at ingest and return 202 regardless.
The client already rendered the comment optimistically, the user cannot distinguish "dropped" from "not
sampled to anyone I can see," and the alternative is queue growth that turns a five-second burst into a
five-minute latency incident. This is Chapter 03, §6 with an unusually permissive product: **live
comments is one of the few systems where silently dropping user writes under load is correct**, and
saying so is a stronger signal than an elaborate durable-queue scheme protecting data nobody will read.

Two guards make shedding safe. Shed **uniformly at random** rather than rejecting everyone who arrives
after a threshold, so loss is unbiased instead of concentrated on the slowest network paths. And never
shed comments from creators, moderators, or paid highlighted messages — a small allowlist the burst
cannot touch, because those are the comments the product actually promises to deliver.

**Reactions bypass all of this**, landing as client-batched counts on a sharded Redis counter via a
separate path and never entering the comment topic. A million reactions per second becomes tens of
thousands of `INCRBY` operations, read by the aggregate event every two seconds.

### 7.4 Moderation latency, and why retraction barely works

Moderation pushes back hardest against everything else in the design, because it is the one place where
"best effort" is not acceptable: a slur delivered to ten million people is not repaired by deleting it
afterward.

The budget is better than it looks — the video is 10–30 seconds behind real time on segmented HTTP
streaming and the latency target is 2 seconds, so there is room. Two stages:

**Stage 1, synchronous, ~5 ms, in the ingest path.** Hashed blocklist lookup, pattern matching on known
evasions, per-user rate limiting, ban-list check. Cheap, deterministic, high-precision, catching the
large majority of straightforward violations. It must be fast enough to sit inline, because anything it
rejects is never published and therefore never has to be retracted.

**Stage 2, asynchronous, ~200–800 ms, a classifier consuming the topic.** Context-aware models catch what
patterns miss. The question is whether its verdict lands before or after delivery, and the answer should
be **before, by deliberately delaying publication**: hold each comment for ~500 ms between ingest and the
aggregator and publish only what stage 2 has cleared. Spending a quarter of the latency budget to make
moderation *pre*-publication is the highest-value trade in this chapter, and most designs miss it because
they treat the budget as something to minimize rather than something to spend.

**Retraction is the fallback, and it is weak.** Publish a `retract` event and edge nodes forward it, but:
viewers who joined afterward never saw the original; viewers who received it may have already rendered
and scrolled past it; viewers who disconnected in between never get the retraction; and — the structural
problem — **because delivery was sampled, the system does not know who received the comment**, so the
retraction must be broadcast to everyone and costs more than the comment did. Build it as the only tool
for what slips through, and be explicit that its coverage is partial.

For low-trust users a stronger option exists: **hold rather than sample**, routing their comments through
a moderation queue or delivering them only to themselves (a shadow ban). The economics work because
low-trust users are a small fraction of the population, so the per-viewer handling is affordable.

### 7.5 SSE, not WebSocket — and what falls away with it

The traffic is server-to-client with a negligible client-to-server component that has no ordering or
session relationship to the downstream flow. That is exactly the profile SSE exists for, and it buys four
things.

**Automatic reconnection with resumption.** The browser reconnects on its own and sends `Last-Event-ID`.
Chapter 30 needed a hand-built reconnect protocol, a cursor table, and a sync procedure; here the platform
supplies the mechanism, and the semantics are trivially satisfied because "resume" means "send me what is
current," not "send me everything I missed."

**Plain HTTP all the way down.** SSE is a response with `Content-Type: text/event-stream`: it traverses
proxies and corporate firewalls that mangle WebSocket upgrades, carries ordinary auth headers, and is
terminated by standard HTTP infrastructure without special configuration.

**A far simpler stateful tier.** The edge node holds a response writer and a room subscription, not a
session. There is no per-user registry, because nobody ever routes a message *to* a specific viewer — the
central piece of Chapter 30's architecture is simply absent. Point at this: the disappearance of the
connection registry is the clearest evidence that broadcast and point-to-point are different systems
rather than the same system at different sizes.

**Cheaper connections**, since unidirectional streams need essentially no receive buffer — a meaningful
fraction of per-connection memory at 100k connections per node.

What you give up: the client cannot send over the same channel, so posting is a separate HTTP request
(mitigated by HTTP/2 connection reuse), and there is no server-initiated ping to detect a dead client —
send SSE comment lines (`: keepalive`) every 15 seconds instead, which also stops intermediaries timing
out the response. One caveat worth knowing: under HTTP/1.1 browsers cap connections per origin at six and
an SSE stream holds one for its whole life, so serve the event stream from a dedicated origin or over
HTTP/2, where multiplexing removes the limit.

### 7.6 Joining mid-stream, room lifecycle, and the long tail of small rooms

A viewer opening a stream to an empty rail sees a dead product, so the client fetches
`GET /comments/recent?limit=50` from the ring buffer while the SSE stream connects. Fifty comments of a
busy room is a few seconds of history — enough to establish that people are here, which is all it must
do. Notice the shape: **the backfill is a cache read, not a query**, because there is no store to query.

Room lifecycle is the operational reality the 10-million-viewer example obscures. A platform runs one
enormous room and *hundreds of thousands* of tiny ones, and the tiny ones are the bulk of the management
work. A 12-viewer stream must not be allocated a 100-node fanout tree; it should sit on a single edge
node with no relay tier at all. **The topology is per-room and elastic**: rooms start flat — one node,
direct subscription — and are promoted to the tiered structure when concurrency crosses a threshold. The
promotion is the interesting operation, because it happens exactly when a stream is going viral and the
system is least able to absorb a reconfiguration. Do it by adding tiers *above* the existing edge nodes
rather than migrating viewers between them, so no connection is disturbed.

This mirrors Chapters 20 and 30 exactly: **a threshold on audience size that switches distribution
strategy, chosen because the population is power-law distributed.** Three products, one structural
answer. When a stream ends, subscriptions are torn down, the ring buffer expires by TTL, and the archival
sink flushes; nothing is reconciled, because nothing was promised.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Edge node dies | 100k viewers lose the rail; video keeps playing | SSE auto-reconnects to another node; ring buffer backfills the gap; no state to recover |
| Relay dies | Its subtree stops receiving comments | Edge nodes fail over to another relay in-region, or cross-region at higher latency; ephemerality means no replay |
| Comment burst exceeds ingest capacity | Latency rises, then queues grow | Uniform random shedding at ingest with creators and moderators allowlisted; return 202 regardless |
| Kafka partition hot | One room's ingest lags | Bucketed keys (32 per room); raise bucket count for known large events ahead of time |
| Moderation classifier degraded | Held comments back up, or unfiltered content publishes | Fail closed for low-trust users (hold), fail open for high-trust; alert on hold-queue depth, never let it silently drain |
| Redis ring buffer unavailable | Late joiners see an empty rail until the first live comment | Degrade silently; the live stream is unaffected because backfill is a separate call |
| Sampling misconfigured too high | Edge node egress and syscall rate spike together | Rate cap enforced at the edge node as a hard ceiling, independent of the policy that selects comments |
| Viral room promotion under load | Reconfiguration during peak growth | Promote by adding tiers above existing nodes, never by migrating connections |
| Thundering reconnect after a deploy | 100k SSE clients reconnect at once | Jittered reconnect (`retry:` field in the SSE stream sets the client's backoff), staggered drains |

**Monitoring:** concurrent connections per edge node and per room; the fanout amplification ratio
(inbound bytes vs outbound bytes per node — a sudden fall means sampling has broken); ingest accept rate
versus shed rate, which should be zero outside bursts; end-to-end latency from POST to edge write,
bucketed by tier; moderation hold-queue depth and stage-2 verdict latency, which is the metric that
turns into a public incident when it degrades; room count by size bucket, to see promotion behavior; and
comment velocity per room, which is the leading indicator of everything else.

---

## 9. Common mistakes

1. **Designing this as Chapter 30 with a larger group.** Per-recipient addressing, durability, and
   delivery guarantees are structurally wrong here; carrying them over produces a design requiring 10^10
   registry lookups per second.
2. **Reaching for WebSocket.** The flow is one-way. SSE gives reconnection, resumption, and plain HTTP
   semantics for free, and needs no per-user connection registry.
3. **Promising to deliver every comment to every viewer** — 2.5 TB/sec for one room, and not what anyone
   wants even if it were free, because nobody reads 1,667 comments per second.
4. **Sampling but not batching.** Sampling fixes bandwidth; two million socket writes per second per node
   still saturates the CPU. Separate problems, separate fixes.
5. **Treating reactions as small comments.** They are 100× the volume and individually meaningless. Count
   them and ship the count.
6. **Making moderation purely post-publication.** The budget is generous because the video is already
   10–30 seconds behind; spend some of it holding comments until the classifier rules, so retraction is
   the exception rather than the mechanism.
7. **Assuming retraction works.** Sampled delivery means the system does not know who received a comment,
   so retraction must be broadcast to everyone and still misses anyone who disconnected.
8. **Building one topology for all rooms.** Nearly all rooms have a handful of viewers and need a single
   node; the tiered tree is for the tail, and the promotion path between them is the real design.
9. **Refusing to shed load.** This is one of the few systems where silently dropping writes during a burst
   is correct; durability machinery protecting ephemeral data is effort spent backwards.

---

## 10. Variants

**Twitch chat.** Many medium rooms rather than one enormous one, deliberately kept small enough that chat
is a conversation, with monetization layered on: subscriber-only mode, slow mode (§7.3's token bucket
exposed as a product control), and paid highlighted messages — the one class of comment with a **delivery
guarantee**, and therefore riding §7.2's personalized track rather than the sample. Once a comment is
paid for, ephemerality and sampling stop being acceptable, and that single product decision reintroduces
per-recipient delivery for a small, bounded subset of traffic.

**YouTube Live / Instagram Live.** Closest to the design above: enormous rooms, aggressive sampling,
prominent aggregate reaction counts, strong bias toward creator-authored and pinned content.

**Live sports scores and play-by-play.** The same tiered broadcast layer with a very low message rate and
a *high* completeness requirement — you cannot sample a goal. The tree is identical, the sampling stage
is removed, and the write path is trivial because there is one authoritative publisher rather than ten
million.

**Live auctions (Chapter 82).** Superficially the same rail, one bidder's action fanning out to all
watchers, but bids are money: ordering is a correctness requirement, durability is mandatory, and every
watcher must see the current price. That puts it back in Chapter 30's world with an authoritative
sequencer added — a useful demonstration that "broadcast" alone does not determine the design; the value
of the payload does.

**Collaborative cursors and presence overlays (Chapters A0, 33).** Extremely high-frequency, entirely
disposable, consumed as an aggregate impression rather than as discrete events — the same sampling and
aggregation logic applies almost unchanged.

---

## 11. Further reading

- Chapter 30, for the point-to-point design this chapter is defined against
- Chapter 02, §8, for the SSE-versus-WebSocket comparison in general form
- Chapter 03, §6, for backpressure and shedding
- Chapter 20, §7.1, for the audience-size threshold pattern in its original form
- [WHATWG HTML Living Standard — Server-Sent Events](https://html.spec.whatwg.org/multipage/server-sent-events.html)
  — the normative description of `Last-Event-ID`, the `retry:` field, and reconnection behavior
- [Discord Engineering — "Maintaining a Chat Room at Scale"](https://discord.com/blog/how-discord-scaled-elixir-to-5-000-000-concurrent-users)
  — a public account of fanout for very large rooms and the cost of per-message work at the edge
- [Twitch Engineering blog](https://blog.twitch.tv/en/tags/engineering/) — posts on chat infrastructure,
  including the move to a tiered edge distribution model
