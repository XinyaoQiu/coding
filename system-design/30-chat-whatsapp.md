# Chapter 30 — Chat (WhatsApp)

> **Prerequisites:** Chapters 01 (wide-column stores, partitioning, clustering keys), 02 (§8 client-push
> protocols — read it before this chapter), 03 (at-least-once delivery, idempotency), 04 (Snowflake IDs)
> **Patterns:** stateful connection fleets, connection registries, per-conversation ordering, cursor-based
> offline delivery, fanout on write vs on read

---

## 1. The problem

Two people exchange messages. A message sent from one phone appears on the other phone in well under a
second. If the recipient's phone is off, in a tunnel, or out of battery, the message appears the moment
they come back. The sender sees a single check when the server has the message, a double check when the
recipient's device has it, and blue checks when it has been read. Groups work the same way, from three
people to a thousand.

This is the second-most-asked system design question after the feed, and unlike the feed it is not
primarily a data problem. The message volume is large but the messages are tiny, the storage arithmetic
comes out to a boring number, and the query pattern is a single ordered range scan. Approach it as a
database question and you will finish in ten minutes with nothing interesting to say.

**The property that makes it hard:** delivery is *pushed*, not pulled, so the server must be able to
reach a specific client at an arbitrary moment. That requires a live connection per online user, which
makes the serving fleet **stateful** — a machine is no longer interchangeable with any other, because it
holds a socket that one specific person's phone is attached to. Every hard part of this chapter follows
from that sentence: capacity is sized by concurrent connections rather than request rate, sending a
message means *finding the machine that holds the recipient*, a deploy or a crash disconnects a hundred
thousand users at once, and everything missed while disconnected must be reconstructed on reconnect.

Statelessness is what makes ordinary web services easy. This chapter is about giving it up on purpose,
and confining the damage to as small a tier as possible.

---

## 2. Requirements

### Functional

1. Send a message to another user (1:1).
2. Send a message to a group.
3. Receive messages in near-real time while online.
4. Receive everything missed while offline, on reconnect, in order.
5. Delivery and read receipts: `sent → delivered → read`.
6. Fetch conversation history with pagination.

Defer, but name: media attachments (§7.7 covers *where* they go, not how they are processed — that is
Chapter 21), voice and video calls (a different system: signaling plus WebRTC, with the server out of the
media path), search, message deletion and editing, and disappearing messages.

The one clarifying question worth asking up front: **is this end-to-end encrypted?** It is not a detail.
It removes server-side search and server-side content-based processing from the design, and it changes
group fanout from "one ciphertext to N recipients" to "N ciphertexts." Ask, then scope it out with a
reason (§7.7).

### Non-functional

- **Delivery latency** — p99 under 500 ms end-to-end for an online recipient, measured from the sender's
  socket write to the recipient's socket read. This is what rules out polling.
- **Durability** — a message acknowledged to the sender must never be lost. The single check mark is a
  promise, and it is the strongest promise in the product.
- **Ordering** — messages within a conversation are delivered and displayed in a consistent order for
  every participant. **There is no global ordering guarantee across conversations**, and there must not
  be one; see §7.3. Refusing to promise global ordering is the correct answer, not a cop-out.
- **Availability** — the connection tier tolerates losing whole machines, with clients reconnecting in
  seconds. Degraded (delayed) beats lost.
- **Consistency** — read-your-writes for the sender (your own message appears optimistically, before the
  server acknowledges), eventual for receipts.
- **Scale** — sized by **concurrent connections**, not by requests per second. State this explicitly; it
  is the axis nearly everyone gets wrong.

### Explicitly out of scope

Voice/video calls, media processing pipelines, spam classification, multi-device key exchange, backup and
export, and cross-platform bridging.

---

## 3. Estimation

Assume 2 billion users, of whom the app is in the foreground an average of 30 minutes per day.

**Concurrent connections — the number that constrains everything**

```
2e9 users × 1,800 s/day        = 3.6e12 connection-seconds/day
3.6e12 / 86,400                ≈ 41.7M average concurrent connections
peak (3×, time-zone clustered) ≈ 125M concurrent connections
```

At a planning figure of **100,000 concurrent connections per connection server** (Chapter 02, §8):

```
125,000,000 / 100,000 = 1,250 connection servers
```

Twelve hundred and fifty machines whose job is to hold sockets. Note what did *not* enter this
calculation: the message rate. If chat traffic doubled tomorrow with the same people online, the
connection tier would not grow at all; if people kept the app open twice as long while sending the same
number of messages, it would double. **Sizing by connections rather than by QPS is the single most
important framing in this chapter**, and it is why the connection tier is separated from everything else.

**Message rate**

```
100e9 messages/day / 86,400    ≈ 1.16M messages/sec average
peak (3×)                      ≈ 3.5M messages/sec
```

Sanity check against the connection count: 3.5M/sec across 125M connections is one message per connected
user every 36 seconds at peak. Plausible.

**Delivery fanout**

Assume 70% of messages are 1:1 and 30% go to groups averaging 10 members:

```
recipients per message = 0.7 × 1 + 0.3 × 9 ≈ 3.4
1.16M × 3.4 ≈ 3.9M deliveries/sec average
peak         ≈ 12M deliveries/sec
```

**Receipts — the surprise**

Each delivery produces two events routed *back* to the sender (`delivered`, then `read`):

```
3.9M × 2 ≈ 7.8M receipt events/sec average
peak      ≈ 24M receipt events/sec
```

**Receipt traffic is twice the message traffic.** A 100-person group message produces one inbound message
and 198 outbound receipt events, each of which is a tiny frame carrying almost no information. This is
the number that forces batching (§7.5), and most candidates never compute it.

**Storage and registry**

```
100e9 msgs/day × 400 B ≈ 40 TB/day → 30-day retention ≈ 1.2 PB → ×3 replicas ≈ 3.6 PB
125M online users × ~64 B (user id, server id, connection id, epoch) ≈ 8 GB registry
```

The message store is the boring part: append-only, uniformly distributed across billions of
conversations, read almost exclusively as recent ranges. The registry is eight gigabytes — the routing
table for the entire online population fits in a small Redis cluster, which is what makes §7.2 cheap
enough to be uninteresting. Say so out loud; candidates routinely over-engineer this component.

**Keepalive overhead**

```
125M connections × 1 ping / 30 s ≈ 4.2M pings/sec fleet-wide ≈ 3,300/sec per server
```

Trivial per box, which is why the keepalive interval is 30 seconds rather than 5. Shorten it and you buy
faster disconnect detection at a proportional cost in radio wakeups and battery — a product trade-off,
not a free tuning knob.

---

## 4. API

The interface is two things: a **frame vocabulary** over a persistent socket, and a small set of ordinary
HTTP endpoints for everything that does not need to be pushed.

### WebSocket frames

```
--- client → server ---
CONNECT   {authToken, deviceId, protocolVersion}
SEND      {clientMsgId, conversationId, type, body?, mediaRef?}
ACK       {messageId}                       # "I have it on durable local storage"
READ      {conversationId, upToMessageId}   # cumulative, not per-message
TYPING    {conversationId, state}           # fire-and-forget, never persisted (Ch. 33)
PING

--- server → client ---
CONNECTED {sessionId, serverTime}
SENT      {clientMsgId, messageId, serverTs}   # the single check
MESSAGE   {messageId, conversationId, senderId, serverTs, type, body?, mediaRef?}
RECEIPT   {conversationId, upToMessageId, userId, state: delivered|read, ts}
SYNC      {conversationId, messages[], hasMore}
ERROR     {clientMsgId?, code, retryable}
PONG
```

### HTTP endpoints

```
POST /v1/sessions
  ->  200 {wsUrl, connectToken}              # token, and which edge to dial

GET  /v1/conversations?since=<cursor>
  ->  200 {conversations: [{id, lastMessageId, lastReadId, unread}], nextCursor}

GET  /v1/conversations/{id}/messages?before=<messageId>&limit=50
  ->  200 {messages: [...], hasMore}          # history / scrollback, not live delivery

POST /v1/groups                    -> 201 {conversationId}
POST /v1/groups/{id}/members       -> 204

POST /v1/media/upload-url
  body: {contentType, sizeBytes, sha256}
  ->  200 {uploadUrl, mediaRef, expiresAt}    # presigned; bytes never cross the socket
```

Four decisions worth defending.

**`clientMsgId` is supplied by the client and is mandatory.** Mobile networks drop connections between
the write and the acknowledgment constantly, and the client's only correct response to an unacknowledged
send is to send again. Without a client-supplied idempotency key you cannot distinguish a retry from a
genuine second message, and duplicated messages in a chat are a highly visible bug. This is Chapter 03's
idempotency key with a product-specific name (§7.3).

**The server assigns `messageId` and `serverTs`; the client's clock is never trusted.** Phone clocks are
wrong by minutes routinely and by years occasionally, and users set them deliberately. If ordering
derived from client timestamps, one misconfigured device would pin its messages to the top or bottom of
everyone's conversation forever.

**Receipts are cumulative, not per-message.** `READ {upToMessageId}` collapses any number of per-message
read events into one frame, and it is idempotent by construction: applying `upTo=500` twice, or
`upTo=400` after `upTo=500`, is a no-op because the state is a high-water mark. This one schema choice
removes most of the receipt volume computed in §3 before any batching logic exists.

**History is HTTP, not WebSocket.** Scrollback is request/response, cacheable, and paginated; it should
not compete for the socket or occupy connection-server memory. Only the *live* path uses the socket.

---

## 5. Data model

The primary store is a wide-column database — Cassandra or equivalent (Chapter 01, §2). The workload is
append-heavy, read as ordered ranges within a known partition, with no cross-partition queries on the hot
path. That is the canonical fit.

```
messages
  conversation_id   UUID        partition key
  message_id        BIGINT      clustering key, ASC    (Snowflake — time-sortable)
  sender_id         BIGINT
  type              TINYINT     (text | image | video | system)
  body              BLOB
  media_ref         TEXT NULL
  created_at        TIMESTAMP

conversation_members
  conversation_id   UUID        partition key
  user_id           BIGINT      clustering key
  joined_at         TIMESTAMP
  role              TINYINT

user_conversations                                   # per-user view, and the delivery cursors
  user_id           BIGINT      partition key
  conversation_id   UUID        clustering key
  last_message_id   BIGINT                            # for unread counts and list ordering
  last_delivered_id BIGINT                            # the offline-delivery cursor  (§7.4)
  last_read_id      BIGINT                            # the read high-water mark
  muted             BOOLEAN
```

And in Redis:

```
conn:{user_id}  →  {server_id, connection_id, connected_at}   TTL ~60 s, refreshed by heartbeat
```

**`(conversation_id, message_id)` is the whole design.** Partitioning by conversation and clustering by a
time-sortable ID means "the last 50 messages in this conversation" and "everything after message X" are
both a **single ordered slice of a single partition** — one seek followed by a sequential read, no
merging, no sorting, no scatter-gather. Contrast the obvious alternative, partitioning by `user_id`:
every group message would then be written once per member (N× write amplification for data that is
logically one row), and any edit or deletion would have to be applied in N places. Store the message
once, in the conversation.

The cost is real and should be named. A partition grows without bound over a conversation's lifetime,
which wide-column stores handle badly, and a very active group is a **hot partition** (Chapter 01, §4).
The standard fix is a time bucket in the partition key (`(conversation_id, month), message_id`), which
caps partition size and costs a second partition read whenever a scrollback page straddles a boundary.
Mention the bucket; it shows you know "partition key = conversation" is not the end of the sentence.

**`last_delivered_id` is a cursor, not a queue** — three tables' worth of design compressed into one
integer. §7.4 is entirely about why.

---

## 6. Architecture, derived

### Attempt 1: HTTP polling

Clients `GET /messages?since=<cursor>` every two seconds.

Nothing is stateful; every server is interchangeable; the whole thing is a normal web service. And it is
dead on arrival:

```
125M concurrent clients / 2 s = 62.5M requests/sec
```

Sixty-two million requests per second, of which the overwhelming majority return an empty array, plus a
two-second latency floor plus radio wakeups that would ruin battery life on every phone running the app.
Long polling (Chapter 02, §8) fixes the empty responses and the latency but not the fundamental cost: it
still holds 125M connections open, so you have paid the stateful-fleet price anyway and received a
unidirectional, one-message-per-connection channel in exchange.

Since the client sends as often as the server does, **WebSocket** is correct here. This is the case the
protocol exists for, and Chapter 02's warning against reaching for it reflexively does not apply.

### Attempt 2: WebSocket, one monolithic fleet

Clients connect to a fleet of servers that terminate the socket, authenticate, persist the message, and
deliver it.

The socket is established as an ordinary HTTP request carrying `Upgrade: websocket`; the server answers
`101 Switching Protocols` and from then on the same TCP connection carries framed, full-duplex traffic
both ways with a few bytes of per-frame overhead. That is the entire mechanism, and it is worth being
able to state in one sentence, because everything expensive about it follows: **the TCP connection
persists, so the server holding it is now stateful.**

This breaks immediately in an obvious place. Alice is on server 7, Bob is on server 412. Server 7 has
Alice's message and no way to reach Bob; with 1,250 servers the probability that sender and recipient
share one is under 0.1%.

It also breaks in a less obvious place: it puts business logic — persistence, group expansion, receipts,
moderation, rate limiting — inside the process holding a hundred thousand sockets. Every business-logic
deploy now disconnects 125M users. Chat products ship business logic daily and connection-handling code
rarely; coupling their deploy cycles is a self-inflicted operational wound.

### Attempt 3: split the connection tier from the message service

```
Client ══ws══► Connection server (stateful, dumb) ──RPC──► Message service (stateless, smart)
```

The **connection server** does exactly four things: terminate TLS and the WebSocket, authenticate on
`CONNECT`, publish `conn:{user_id} → {server_id}` into the registry, and shuttle frames both ways. It
holds no business logic and is deployed rarely. The **message service** is an ordinary stateless service
sized by request rate (3.5M/sec at peak, sharded like any other): it assigns the message ID and server
timestamp, writes to Cassandra, expands group membership, and decides who must receive what.

Delivery now works: the message service looks up `conn:{bob}`, finds `server_412`, and forwards the frame
to that server, which writes it to Bob's socket (§7.2). The general principle is worth stating: **when
you are forced to have a stateful tier, make it as thin and as stupid as possible.** The state is the
liability; minimize the surface area that shares its deploy cadence and failure domain.

### Attempt 4: offline delivery

Bob is offline. There is no socket to write to. What now?

The instinctive answer is a **per-user inbox queue** — a durable queue per user, drained on connect. It
works, and it is wrong at this scale: 12M deliveries/sec becomes 12M durable enqueues/sec on top of the
3.5M message writes, and a billion users becomes a billion queues to create, monitor, and garbage-collect.
You have quadrupled write volume to store, a second time, data already durable in `messages`.

The right answer is a **cursor**. `user_conversations.last_delivered_id` records, per user per
conversation, the highest message ID that user has confirmed receipt of; on reconnect the server reads
that row set and replays the difference from the conversation partitions. The write cost of "Bob was
offline" is one integer update per conversation when he acknowledges, not one enqueue per message. §7.4
has the mechanics and the full comparison.

### Attempt 5: groups, and where fanout on write stops working

For a 10-member group the message service expands membership and delivers 9 copies. Cheap. For a
100,000-member broadcast group it is not, and it is exactly Chapter 20's celebrity problem wearing a
different hat: **one logical write amplified into a number of physical deliveries proportional to a
power-law-distributed group size.** Above a threshold, stop pushing: write the message, notify members
cheaply that the conversation changed, and let clients pull the content (§7.6).

### Final architecture

```
                        ┌──────────────────────────────────────────────┐
                        │  Redis registry:  conn:{user} → server_id    │
                        └───────▲───────────────────────┬──────────────┘
                                │ register/heartbeat    │ lookup
                                │                       │
   Alice ══ws══► ┌──────────────┴────┐            ┌─────▼──────────────┐ ══ws══► Bob
                 │ Connection server │            │ Connection server  │
                 │      #7           │            │      #412          │
                 │ (100k sockets)    │            │ (100k sockets)     │
                 └────────┬──────────┘            └─────▲──────────────┘
                          │ SEND                        │ MESSAGE frame
                          ▼                             │
                 ┌────────────────────────────────────┐ │
                 │  Message service (stateless)       │─┘   routed via inter-server
                 │   · dedupe on clientMsgId          │     RPC or a Kafka topic
                 │   · assign message_id (Snowflake)  │     partitioned by server_id
                 │   · assign server timestamp        │
                 │   · expand group membership        │
                 └──┬───────────────┬─────────────┬───┘
                    │               │             │
                    ▼               ▼             ▼
          ┌──────────────┐  ┌───────────────┐  ┌──────────────────┐
          │ Cassandra    │  │ Cassandra     │  │ Push service     │
          │ messages     │  │ user_convos   │  │ APNs / FCM       │
          │ (conv, msg)  │  │ (cursors)     │  │  (Chapter 32)    │
          └──────────────┘  └───────────────┘  └──────────────────┘

          Media:  Client ──presigned PUT──► Object store ──► CDN ──► Client
                  (only the mediaRef travels over the socket)
```

Three properties of this diagram are worth naming: the stateful tier is one box wide and contains no
business logic; the registry is 8 GB and therefore not a scaling problem; and every path that could be
slow — persistence, group expansion, push, media — is off the socket's critical write path.

---

## 7. Deep dives

### 7.1 Sizing a stateful fleet, and what statefulness actually costs

A hundred thousand connections per box is a planning number, and what bounds it is not what people assume.

It is **not memory**. A tuned connection — small socket buffers, a few kilobytes of session state — costs
on the order of 10 KB, so 100k connections is roughly 1 GB. Default socket buffer sizes are the enemy:
leaving them at a few hundred kilobytes each turns 100k connections into tens of gigabytes, and tuning
them down is the first thing you do. Nor is it file descriptors, which are a configuration line.

It is **CPU during connection churn**, and **the blast radius of losing a box**. A TLS handshake costs
single-digit milliseconds of CPU; steady state is cheap, a reconnect storm is not:

```
one server dies       → 100k clients reconnect
one AZ (1/3) is lost  → ~40M clients reconnect
40M handshakes over 60 s ≈ 667k handshakes/sec across the surviving fleet
```

That is the number that breaks you, and it breaks you *during* an incident, when capacity is already
reduced. The mitigations are all client-side and must ship before you need them: **exponential backoff
with jitter** (Chapter 03, §5), a randomized initial delay so a fleet-wide deploy does not produce a
synchronized reconnect, and session resumption that avoids a full handshake. A client that reconnects
instantly and retries every second is an amplifier pointed at your own infrastructure.

Two further costs. **Deploys are disconnections** — rolling a connection server severs 100k live sockets,
so deploy the tier rarely (which attempt 3 makes possible), drain by refusing new connections over
minutes rather than seconds, and keep reconnect so cheap that a severed connection is invisible. And
**load balancing is by connection count, not request count**: least-connections is mandatory, because
round-robin over long-lived connections produces permanent imbalance — a server that was briefly
unhealthy during a churn event stays underloaded for hours while its peers sit at capacity. Connections
must also be *rebalanceable*, with the server able to ask a client to reconnect elsewhere, or you can
never drain a hot box without dropping users.

### 7.2 Routing a message from one connection server to another

The message service knows Bob is on `server_412`. Getting the frame there has three plausible mechanisms.

**Direct RPC.** The message service maintains connections to all 1,250 connection servers and sends the
frame to the right one. Lowest latency, one hop, no intermediary to operate. Costs an
O(services × connection-servers) mesh and inline handling of "that server just died."

**A pub/sub channel per user.** Each connection server subscribes for every user it holds. Elegant on a
whiteboard, terrible in practice: 125M subscriptions churning at connection-churn rate is more work for
the pub/sub system than the message delivery itself.

**A partitioned log with one partition set per connection server** (Chapter 93), produced to by the
message service and partitioned by `server_id`. Buys durability and replay through a restart; costs a hop
of latency and a rebalance whenever the fleet changes size.

**Choose direct RPC, with the registry as the address book.** The payload is a live frame for a live
socket; if the socket is gone the frame is worthless, and the message is already durable in Cassandra, so
the recipient gets it from their cursor on reconnect. Paying for durable, replayable transport of a
payload whose value expires in milliseconds is the wrong trade. The cost is that the message service must
handle routing failures itself: connection refused, server draining, or — the interesting one — **a stale
registry**.

Staleness is a race worth stating precisely. Bob's phone switches from Wi-Fi to cellular; the new
connection to `server_88` registers before `server_412` notices the old socket is dead and deletes its
entry. Now a message routes to a server that no longer holds Bob. Three defenses, used together:

1. **Register with a compare-and-set on a monotonically increasing connection epoch**, so a later
   connection always wins and a dying server's cleanup cannot delete a newer entry.
2. **Give the entry a TTL** (~60 s) refreshed by the connection server's heartbeat, so a server that dies
   without cleaning up leaves entries that expire rather than entries that lie forever.
3. **Make delivery fail-safe rather than fail-fast.** If routing fails, do nothing beyond recording it.
   The message is durable and the cursor will deliver it.

That third point is the one to make aloud. The cursor is not merely an offline-delivery mechanism; it is
the correctness backstop that lets the entire live path be best-effort, which is why the whole class of
routing bugs degrades to "delivered a little later" rather than "lost."

### 7.3 Server-assigned IDs, per-conversation ordering, and idempotency

Three questions that are really one question: what defines the order of a conversation?

**IDs and timestamps are assigned by the message service, never by the client.** Phone clocks drift, users
change them, time zones are misconfigured. If display or storage order derived from a client timestamp, a
single device with its clock set to 2038 would sit at the bottom of every participant's conversation
permanently, unfixably. The server assigns a Snowflake-style ID (Chapter 04, §1) whose high bits are a
timestamp, so the ID sorts by time and doubles as the clustering key and the pagination cursor. The
client's timestamp may still be carried as a display hint, clearly labeled untrusted.

**Ordering is guaranteed per conversation, and nowhere else.** Within a conversation all participants see
the same order, because its messages are sequenced by one logical sequencer — the message-service shard
that owns the conversation, or a Snowflake generator whose ordering is good enough at the granularity
conversations actually experience. Across conversations, no guarantee: if Alice and Bob message you in
the same instant, the order they appear in your conversation list is arbitrary.

Do not be talked out of this. Global ordering would require a single global sequencer — one machine, or a
consensus round, in the path of 3.5M messages per second — and would buy nothing, because **no user can
observe a violation of it**. Users observe order within a conversation. They cannot observe that Bob's
message in one group was assigned an ID after Carol's in an unrelated chat. Scoping the ordering
guarantee to the unit of observation is the correct senior answer; offering global ordering unprompted is
a red flag, because it means you have not thought about who is looking.

Even the per-conversation guarantee has a boundary worth being honest about. Two participants sending
simultaneously are ordered by whichever message reaches the sequencer first, which is arbitrary but
*consistent* — everyone sees the same arbitrary order, which is all that matters. Causal ordering (a
reply never appearing before the message it replies to) is stronger; if the product needs it, carry a
`replyToId` and enforce it on render rather than in the transport.

**Idempotency comes from `clientMsgId`.** Retries over mobile networks are not an edge case, they are the
normal operating condition: a client sends, loses the connection before `SENT` arrives, reconnects, and
cannot know whether the server got it, so it must resend. The message service keeps a short-lived
deduplication index on `(sender_id, clientMsgId)` — a few minutes is enough, since retries happen within
the reconnect window — and on a hit returns the original `SENT` frame with the original `messageId`
rather than writing a second message. This is Chapter 03, §4 verbatim; what is chat-specific is that the
failure it protects against is *guaranteed*, thousands of times per second, rather than rare.

The dedupe index needs neither durability nor global consistency. Losing it produces a duplicate message,
a visible but minor bug; making it strongly consistent would put a synchronous coordinated write in the
path of every message. Redis with a TTL is the right store.

### 7.4 Offline delivery: a cursor, not an inbox

The comparison deserves its own arithmetic, because "queue per user" is the answer most candidates give
and it is defensible enough that you must be able to say precisely why the alternative is better.

| | Per-user inbox queue | Cursor + replay |
|---|---|---|
| Writes per delivery | 1 durable enqueue (12M/sec peak) | 0 on send; 1 integer update on ack |
| Storage | Second full copy of every message, per recipient | None beyond the message itself |
| Objects to manage | ~1B queues, created and GC'd per user | One row per (user, conversation) |
| Dormant users | Queue grows unboundedly or needs a TTL policy | Cursor stays stale; costs nothing |
| Read on reconnect | Drain a queue | Ordered slice per changed conversation |
| Group message | N enqueues | 1 message row, N cursors lag behind it |

The cursor wins because **the message store is already an ordered, durable, per-conversation log**. An
inbox queue is a second, redundant, per-recipient copy of a log you already have. The insight generalizes
past chat: when the primary store is already ordered along the axis the consumer reads, a per-consumer
queue is a materialization you do not need — you need a position.

```
SEND → message service → write messages row
                       → update user_conversations[recipient].last_message_id
                       → attempt live delivery (best effort)

client ACK {messageId} → update user_conversations[user].last_delivered_id = messageId

CONNECT → read user_conversations[user]                       # one partition scan
        → for each row where last_message_id > last_delivered_id:
              slice messages[conversation] where message_id > last_delivered_id
        → stream as SYNC frames, oldest first
```

Two details that show you have implemented something like this.

**The acknowledgment must come after the client has written to durable local storage**, not on receipt of
the frame. If the client acks on receipt and then crashes before persisting, the cursor has advanced past
a message the user will never see. The cursor's semantics are "this device definitely has this," which is
exactly the semantics the `delivered` receipt needs — one ack drives both (§7.5).

**Bound the replay.** A user returning after three months may have 50,000 unread messages across 400
conversations, and streaming all of it over a fresh mobile connection is bad for both sides. Cap the
sync: replay at most the last N messages per conversation and the top M conversations by recency, mark
the rest "has more," and let the client backfill over HTTP as the user scrolls. The sync path and the
history path then converge on the same primitive, which is the sign the data model was right.

Finally: if the recipient is offline and the conversation is unmuted, the message service also emits a
native push notification (Chapter 32) — the only mechanism reaching a device whose app is not running.
The push carries a notification, not the message; the message arrives through the cursor when the app
opens. Treating push as the delivery mechanism rather than as a doorbell is a common and expensive
mistake, because push is best-effort and its payload is small.

### 7.5 Receipts, and why groups make them the dominant traffic

Receipts are a three-state machine per (message, recipient):

```
sent       server has durably stored it            → sender, immediately, one frame
delivered  recipient's device has it on disk       → sender, on the recipient's ACK
read       recipient has viewed the conversation   → sender, on the recipient's READ
```

Each transition is an event routed back to the sender through the same registry-lookup-and-route
machinery as a message. From §3, that is 7.8M receipt events per second average and 24M at peak — twice
the message traffic, for frames carrying a few dozen bytes of information.

In a group it is worse, and the shape of "worse" is the point. A message to a group of N produces up to
2(N−1) receipt events, all converging **on one sender's one connection**. For a 500-member group, one
message produces 998 inbound frames for a single phone, arriving over the following minutes as people
open the app. Rendering "998 people read this" does not require 998 frames.

Three mitigations, in the order to present them:

1. **Cumulative high-water marks, already in the API.** `READ {conversationId, upToMessageId}` collapses
   a user catching up on 200 messages into one frame instead of 200. Free, and the largest single
   reduction available.
2. **Aggregate group receipts server-side.** Above a small group size the sender does not need per-member
   events, only a count and, on demand, a list. Keep per-member state in `user_conversations` where it
   already lives and push the sender a periodic summary (`{messageId, deliveredCount, readCount}`) on a
   timer — every few seconds while the counts move, then stop. This turns 2(N−1) events into a handful,
   with per-member detail fetched over HTTP only when the user taps "message info."
3. **Batch and coalesce at the connection server.** Receipt frames for the same conversation bound for
   the same socket within a short window merge into one. This is the generic treatment for any
   high-frequency low-value signal and applies to typing indicators too (Chapter 33, §7.4).

Then say the thing worth saying: **read receipts are optional, and many users disable them.** That is a
design input, not a footnote — the system needs a per-user setting suppressing both sending and receiving
them, which means the receipt path carries a privacy check the message path does not. Products that treat
receipts as pure infrastructure discover this late.

### 7.6 Groups: fanout on write until it isn't

For a group of size N, delivery reads `conversation_members` and, per member, either writes to their
socket or lets the cursor handle it. That is **fanout on write**, and it is correct for the overwhelming
majority of groups, because the overwhelming majority of groups are small.

It stops being correct for the same reason it does in Chapter 20: group size is power-law distributed. A
100,000-member broadcast channel produces, per message:

```
100,000 registry lookups
~40,000 socket writes (the fraction currently online)
100,000 cursor rows now behind
up to 200,000 receipt events converging on one sender
```

The pull design is the mirror image. Above a membership threshold the message service writes the message
and does **not** expand membership. Members learn of it from a cheap, aggressively coalesced
"conversation changed" signal — coalescing is safe because a broadcast channel's readers do not need
sub-second latency — or simply on their next foreground sync, and then fetch content by slicing the
conversation partition themselves, the operation the schema is best at.

The trade-offs are Chapter 20's, with one asymmetry: in chat the recipient set is *explicit and stored*
in a membership table rather than implicit in a follow graph, so the threshold decision is a single cheap
lookup at write time and is reversible if the group shrinks. Store `member_count` on the conversation and
branch on it.

The cost of the pull path is latency and the loss of per-member receipts. Both are acceptable *for the
kind of conversation that is large*: a 100,000-member broadcast channel is not interactive, nobody
expects sub-second delivery, and nobody wants 100,000 read receipts. **The conversations where push
matters are small, and the conversations where push is expensive are ones where it does not matter.**
That alignment is what makes the threshold a design rather than a compromise, and it is the sentence to
say out loud. Set the threshold as a tuned parameter — a few hundred members is a reasonable start —
changeable without a deploy; presenting a hard number as derived is over-claiming.

### 7.7 What end-to-end encryption removes, and why media never travels over the socket

Both of these are about the same discipline: keeping content out of paths that cannot afford it.

**End-to-end encryption.** With E2EE (Signal protocol or equivalent), the server stores and routes
ciphertext it cannot read. What that *removes* from the design is not a detail:

- **Server-side search is gone.** History search happens on-device against the local database, so the
  client must retain everything it wants to search and the feature is bounded by phone storage. There is
  no server-side inverted index (Chapter 61), because there is no plaintext to index.
- **Server-side content processing is gone**: no spam classification on message bodies, no server-generated
  link previews, no content moderation of message contents. Abuse detection must work from metadata —
  rates, graph shape, user reports — which is genuinely harder, and is why E2EE products lean on reporting.
- **Group fanout changes shape.** The server can no longer send one ciphertext to N recipients, because
  each decrypts with a different key. Either the sender encrypts a per-message key once per recipient
  device (sender keys), or you encrypt N times. Either way client work and upload size scale with group
  size and device count — a real constraint on maximum group size, and a reason large broadcast channels
  are often *not* E2EE in products offering both.
- **Multi-device becomes a key-distribution problem**, not a data-sync problem, and the history a new
  device can see is bounded by what can be re-encrypted to it.

Scope it out with that list rather than with "out of scope for time." E2EE is a constraint on the design,
not a component of it, and the transport, ordering, cursor, and receipt machinery in this chapter are all
unchanged by it, because none of them read the body. "Here is exactly what the design loses if we turn it
on, and here is why nothing in my diagram moves" is a far stronger answer than hand-waving it in.

**Media never travels over the socket.** A 20 MB video sent as WebSocket frames occupies one connection
server's memory and outbound bandwidth for the duration of the upload, head-of-line-blocks every other
frame on that socket (including the recipient's incoming text messages), cannot be resumed after a
disconnect, cannot be served from a CDN, and drags the stateful tier — the tier you worked hardest to
keep thin — into the bulk-data business.

Instead: the client requests a presigned upload URL over HTTP, `PUT`s the bytes directly to object
storage, and sends a message whose body is a `mediaRef` plus an inlineable thumbnail. The recipient
fetches from a CDN and the socket carries a few hundred bytes. The secondary benefits are large: uploads
become resumable independently of the chat connection; content-hash deduplication means a video forwarded
10 million times is stored once (Chapter 41); the CDN absorbs read traffic that would otherwise hit your
origin; and transcoding happens asynchronously with nobody waiting on a socket. Under E2EE the same
structure holds — the client encrypts the blob before upload and the `mediaRef` carries the key — which
demonstrates the split was right for reasons independent of encryption.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Connection server crashes | 100k users disconnected; in-flight frames lost | Clients reconnect with jittered backoff; cursor replays anything missed; registry entries expire by TTL |
| Reconnect storm after an AZ loss | ~667k TLS handshakes/sec against reduced capacity | Mandatory client-side exponential backoff with jitter; session resumption; capacity headroom sized for N−1 AZs |
| Registry stale (client migrated) | Frame routed to a server that no longer holds the socket | Epoch-based CAS on register; TTL on entries; routing failure is a no-op because the cursor is authoritative |
| Registry cluster loss | No live delivery at all; messages still persisted | Degrade to store-and-sync: messages land in Cassandra, clients receive them on their next reconnect or foreground sync |
| Message service shard down | Sends to affected conversations fail | Client retries with the same `clientMsgId`; dedupe index makes the retry safe |
| Duplicate delivery (at-least-once) | The same message rendered twice | `messageId` is the client-side dedupe key; the client's local store is keyed by it, so duplicates are absorbed |
| Hot conversation partition | One Cassandra partition saturated by a very active group | Time-bucketed partition key; large groups already on the pull path (§7.6) |
| Receipt storm from a large group | Sender's socket flooded with thousands of frames | Cumulative high-water marks; server-side aggregation above a group-size threshold |
| Client with a broken clock | Nothing | Server assigns all IDs and timestamps; client time is a display hint only |
| Push provider (APNs/FCM) degraded | Offline users are not woken | Messages are durable; delivery happens on next foreground. Push is a doorbell, not the delivery path (Chapter 32) |

**Monitoring:** concurrent connections per server and the variance across the fleet (imbalance predicts
the next hot-box incident); connection churn and reconnect rate, the leading indicators for everything in
this chapter; end-to-end delivery latency measured from sender ack to recipient ack, not hop to hop;
registry stale-route rate; sync payload size at connect, bucketed by how long the client was away;
receipt events per message by conversation size; and the ratio of live deliveries to cursor-replayed
deliveries, which tells you how much traffic the fast path is actually serving.

---

## 9. Common mistakes

1. **Sizing the fleet by requests per second.** The connection tier is sized by concurrent connections,
   and the two numbers move independently. Computing messages/sec and dividing by a server's request
   throughput demonstrates you have not understood what the tier does.
2. **Putting business logic in the connection server**, coupling the deploy cadence of code that changes
   daily to a tier whose every deploy disconnects a hundred thousand users.
3. **Trusting client timestamps for ordering.** One device with a wrong clock corrupts ordering for
   everyone in the conversation, permanently and unfixably.
4. **Promising global message ordering.** It requires a global sequencer in the path of millions of writes
   per second, and no user can observe whether you provide it. Order per conversation and say so.
5. **Building a durable inbox queue per user.** A second copy of every message, quadrupled write volume,
   and a billion objects to garbage-collect — to replace one integer. Its close cousin: acknowledging on
   frame receipt rather than on durable local write, which advances the cursor past messages the device
   never persisted and loses them silently.
6. **Ignoring receipts in the estimation.** They are twice the message volume and, in groups, converge on
   a single connection. Sizing for messages alone misses the actual bottleneck.
7. **Treating push notifications as the delivery mechanism.** Native push is best-effort, small, and
   platform-controlled: it wakes the app, and the app syncs. Putting message content in the push payload
   makes correctness depend on a third party.
8. **Sending media over the WebSocket**, which head-of-line-blocks the socket, prevents CDN delivery and
   resumable upload, and drags your thinnest tier into bulk data transfer.
9. **Omitting `clientMsgId`.** Retries on mobile networks are guaranteed, not hypothetical, and without
   an idempotency key every one of them is a duplicate message the user sees.

---

## 10. Variants

**Slack / Teams (workplace chat).** Channels rather than conversations, same model: partition by channel,
cluster by message ID. Two differences matter. Channels are typically *not* E2EE, which restores
server-side search, and enterprise compliance requires full retention and export — so the storage
arithmetic changes from "30-day window" to "forever, with eDiscovery," making the message store the
dominant cost. Threading adds a second clustering dimension.

**Discord (large public communities).** Rooms with tens of thousands of concurrent members make §7.6's
broadcast case the *normal* case rather than the tail. Presence and typing indicators for a 50,000-member
server become harder than the messages (Chapter 33), and the design converges toward Chapter 31's tiered
broadcast fanout.

**Live comments (Chapter 31).** The same connection tier, but one-way and ephemeral. Both changes are
simplifications: SSE replaces WebSocket, and durability collapses. Reading Chapter 31 immediately after
this one is the fastest way to see which parts of this design were essential and which were consequences
of point-to-point durable delivery.

**Notification systems (Chapter 32).** The delivery half of this chapter without the conversation half:
fanout, provider semantics, retries, idempotency — no ordering requirement, no receipts.

**Collaborative editing (Chapter A0).** Same persistent-connection substrate, but the messages are
operations on shared state rather than independent items, so ordering stops being cosmetic and becomes a
correctness requirement — which is why OT and CRDTs exist, and why chat needs neither.

---

## 11. Further reading

- Chapter 02, §8, for the protocol comparison this chapter assumes
- Chapter 20, §7.1, for the fanout threshold argument in its original form
- Chapter 32, for the native-push path taken when the recipient is offline
- Rick Reed, "Scaling to Millions of Simultaneous Connections" (Erlang Factory SF, 2012) — the primary
  public account of WhatsApp's connection tier, and still the clearest treatment of what limits
  connections per box
- [Discord Engineering — "How Discord Stores Billions of Messages"](https://discord.com/blog/how-discord-stores-billions-of-messages)
  — a detailed public account of the `(channel_id, message_id)` partitioning decision and the hot-partition
  problems it produces
- [Signal Protocol documentation](https://signal.org/docs/) — the specifications (X3DH, Double Ratchet,
  Sender Keys) behind the E2EE constraints in §7.7
- RFC 6455, *The WebSocket Protocol* — worth reading the handshake section once, so that "upgrade an HTTP
  request to a persistent full-duplex connection" is a mechanism you can describe rather than a phrase
