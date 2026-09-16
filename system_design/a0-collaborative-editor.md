# Chapter A0 — Collaborative Editor (Google Docs)

> **Prerequisites:** Chapters 01 (logs, derived state), 02 (§8 client-push protocols), 03 (idempotency), 04 (leases, fencing)
> **Patterns:** operation log as source of truth, central sequencer, convergence, derived materialization

---

## 1. The problem

Several people open the same document and type into it at once. Each sees their own keystrokes instantly,
sees everyone else's within a few hundred milliseconds, and — this is the whole requirement — when everyone
stops typing, every screen shows identical text.

The product is Google Docs. It is also Notion, Quip, Confluence's editor, and the shared-cursor mode of every
modern IDE: a shared mutable sequence, edited concurrently by replicas that cannot wait for each other.

**The property that makes it hard:** an edit is expressed as a position — "insert `x` at offset 42" — and by
the time it reaches anyone else, offset 42 no longer means what its author meant, because every other replica
has applied its own edits underneath it. No global order was waited for, since waiting would put a round trip
between a keystroke and the character appearing. So replicas apply operations in different orders and must
nonetheless converge — the rest of this chapter is machinery for **manufacturing commutativity out of
operations that are not naturally commutative.**

---

## 2. Requirements

### Functional

1. Open a document and see its content.
2. Insert and delete ranges of text; apply formatting.
3. See others' changes live, and see their cursors and selections, labeled.
4. Share with people or by link, with roles (owner, editor, commenter, viewer).
5. Browse and restore revision history; edit offline and reconcile on reconnect.

Defer, but name: comments, suggestion mode, images, tables, export. Ask early whether **offline editing is in
scope** — the one requirement that can flip the core algorithm choice (§7.1, §7.5).

### Non-functional

- **Convergence.** Replicas that have received the same operations display identical content. A correctness
  requirement, not a performance one, and the requirement the design exists to satisfy.
- **Local echo latency: zero.** A keystroke renders before any network I/O, which alone rules out any design
  that serializes edits through the server before displaying them. Remote edits: p99 under 200 ms.
- **Intention preservation.** Convergence is not enough — replicas could converge on text nobody meant. If A
  types at the start of a line while B types at the end, both survive, each where its author put it.
- **Durability.** No acknowledged edit is lost. Acknowledged means "in a durable log," not "a peer saw it."
- **Concurrent editors per document** — 50 comfortably, degrading past 100; an unreachable document stays
  readable from cache while edits queue locally.

### Explicitly out of scope

The rendering and layout engine, the rich-text model beyond "a sequence with attributed ranges," and search
across a corpus (Chapter 61).

---

## 3. Estimation

Assume 100 million daily active editor users.

```
100M users × 3 documents/day × 10 min open = 3 × 10^9 user-minutes/day
3e9 / 1,440 min-per-day                     ≈ 2.08M concurrent open documents
peak (3×)                                   ≈ 6.3M concurrent WebSocket connections
6.3e6 / 100,000 conn-per-server             ≈ 63 session servers at peak
```

Sixty-three machines hold every open document in the world; connection count is not the problem.
**Operation rate.** Clients hold at most one unacknowledged operation (§6), composing while they wait, so a
typist emits roughly 10 operations per second; assume 2% of open sessions are typing at any instant.

```
2.08M sessions × 2% typing × 10 ops/sec = 416,000 ops/sec average, ~1.25M peak
416,000 × 60 B/op                        ≈ 25 MB/sec of operation log
```

Four hundred thousand durable appends per second sounds alarming and is not: a shard holding 5,000 actively
edited documents group-commits into one write every 20 ms — 50 writes per second per shard. The gap between
416,000 and 50 is entirely a batching decision.

**Transformation cost — the number that defends the algorithm.** An operation arrives tagged with the
revision it was written against and must be transformed against everything committed since:

```
ops since the client's revision ≈ (ops/sec on this document) × RTT
5 concurrent typists × 10 ops/sec × 0.1 sec = 5 operations
```

**Five.** The cost is proportional to the *concurrency window*, bounded by network latency and the number of
simultaneous typists — not by document size and not by edit history. That is the answer to "isn't operational
transformation expensive?", and the intuition that it must be is what misleads people in §7.1. The one case
where the window explodes is the offline client (§7.5).

**Storage — the constraining number.** A 3,000-word document is about 18,000 characters, and if each arrived
as its own operation carrying revision, author, position, and timestamp:

```
18,000 ops × 60 B = 1.08 MB of log for 18 KB of text          → 60×
1B documents × 5 KB median text                               = 5 TB of content
1B documents × ~10× log after composing same-author runs      = 50 TB, monotonically growing
```

Composition brings 60× down to 5–15×, and never to 1×, because the log records history and the text records
only the present. **The document is small; its history is not, and the history is what you are actually
storing.** That forces §7.2's snapshot-and-compaction design.

---

## 4. API

The edit path is a WebSocket, because both sides send continuously (Chapter 02 §8). Everything else is HTTP.

```
GET  /documents/{docId}   ->  200 {revision, content, collaborators[], role}     # bootstrap

WS   /documents/{docId}/session
  C→S {t:"submit",   revision:R, clientSeq:N, ops:[{retain:12},{insert:"hi"},{delete:3}]}
  S→C {t:"ack",      clientSeq:N, revision:R', contentHash}
  S→C {t:"apply",    revision:R'', authorId, ops:[...]}
  C→S {t:"cursor",   revision:R, anchor:120, head:145}
  S→C {t:"presence", editors:[{sessionId, userId, color, anchor, head}]}
  S→C {t:"revoked",  reason:"access_removed"}

GET  /documents/{docId}/operations?since={revision}   # reconnect / offline catch-up
GET  /documents/{docId}/revisions                     # named versions
POST /documents/{docId}/permissions {principal, role}
POST /documents/{docId}/links {role, expiresAt}       # link sharing -> capability token
```

**Every message carries a revision, and that is the entire concurrency-control mechanism.** The client's
revision says how stale its view was when it composed the operation, which says exactly which committed
operations the server must transform against. No locks and no version vectors are needed, because a single
server assigns revisions (§6) and a single assigner means a single linear history.

**Operations are document-length transforms, not point edits.** The `{retain, insert, delete}` form — Quill's
Delta shape — describes a walk over the whole document, so it composes, it inverts (needed for undo, §7.6),
and it carries formatting as attributes on retains. Point edits do none of those. **`clientSeq` makes
submission idempotent**: a client retries an unacknowledged operation after reconnecting, and the server,
seeing a `clientSeq` already committed for that session, replays the original ack instead of applying it
twice, which makes Chapter 03's at-least-once delivery harmless. Cursor updates share the socket but are
*not* operations — not acknowledged, not logged, not retried (§7.3).

---

## 5. Data model

```
documents   doc_id UUID pk | owner_id | title | current_revision | snapshot_revision

operations                                        # the source of truth
  doc_id UUID partition key | revision BIGINT clustering key ascending
  author_id | session_id | client_seq | payload BLOB | created_at

snapshots                                         # derived, regenerable
  doc_id UUID partition key | revision BIGINT clustering desc | content_ref | created_at

acl doc_id UUID partition key | principal_id TEXT clustering key  # user, group, or link token
    role ENUM(owner, editor, commenter, viewer)   | inherited_from UUID NULL

sessions                                          # Redis, TTL 60 s, never durable
  doc_id -> { session_id -> {user_id, color, anchor, head, revision, last_seen} }
```

**The operation log is the document.** `snapshots` is a cache of a fold over the log; losing every snapshot
costs recomputation, not data — the inverse of Chapter 20, where the log was disposable and the materialized
view was what users read. Operations are partitioned by document and clustered by revision, so every hot-path
read is a single-partition range scan, with no skew problem of Chapter 20's kind. **Presence lives in Redis
with a TTL and never enters the log**: it is high-volume, worthless after a second, and a session that dies
without a goodbye must expire on its own (Chapter 33).

---

## 6. Architecture, derived

### Attempt 1: whole-document save, last writer wins

The client autosaves the full text every few seconds; the server stores it. With two editors typing at 3.3
characters per second and a five-second autosave, each save overwrites everything the other typed since the
last one — `2 × 3.3 × 5 ≈ 16 characters` discarded per cycle, a word lost every five seconds, silently.
**This fails at two users**, the number of users the product is for: not a scale failure but an immediate
one, and the design of most naive save buttons.

### Attempt 2: broadcast positional patches, apply verbatim

Send `{insert:"x", at:42}`; everyone applies it. The counterexample takes three characters. The document is
`ABC`; concurrently A inserts `X` at 1 and B deletes the character at 1:

```
A:  ABC → AXBC   then applies B's delete@1  → ABC     (the X is gone)
B:  ABC → AC     then applies A's insert@1  → AXC
```

Permanent divergence from four operations, and neither user did anything unusual. From §3 the expected number
of operations concurrent with any given one is about five, so **essentially every operation is concurrent
with something** and this fires within seconds. It is not a rare race; it is the normal case.

### Attempt 3: lock the document, or lock a paragraph

Correct by construction, and unusable. A lock costs a round trip — 80 to 100 ms — while keystrokes arrive
every 150 to 300 ms, so you lock once per word and double the latency of typing; and paragraph granularity
fails exactly where two people edit one paragraph, which is the case the product exists for.

### Attempt 4: operational transformation with a central sequencer

Keep attempt 2's local echo and fix divergence with a **transformation function**:

```
T(a, b) → (a', b')   such that   apply(apply(doc, a), b') ≡ apply(apply(doc, b), a')
```

Given two operations written against the same state, `T` adjusts them so either order yields the same result.
Above: A's `insert@1` transformed against B's `delete@1` stays at 1; B's `delete@1` becomes `delete@2`. Both
replicas reach `AXC`, and A's `X` survives — which is also the intention-preserving answer. Now impose an
order: **one server owns the document and assigns revision numbers.** This is the Jupiter algorithm (Nichols
et al., 1995), and it is what Google Docs runs.

```
CLIENT  applies its own op locally, immediately  (zero-latency local echo)
        sends it tagged with the revision it was based on
        holds at most ONE unacknowledged op, composing further edits into a pending buffer
        on a remote op: transforms it against the outstanding op and buffer, applies it,
          and transforms its own outstanding op forward

SERVER  receives op tagged revision R; current revision is C
        transforms it against operations R+1 .. C     (§3: about five of them)
        appends the result at C+1, durably; acks the author; broadcasts to other sessions
```

Two properties make this tractable, and both come from the central server. **The server's history is
linear**, so the transformation functions need only **TP1** — a transformed pair converges — and never
**TP2**, the far harder property that transformations of three or more concurrent operations converge
regardless of composition order, which is where the published convergence bugs live (§7.1). And **each client
keeps one operation in flight**, so its relationship with the server is a two-party state space with a single
dimension of concurrency.

### Attempt 5: durable log, derived document, document affinity

The server must not ack before the operation is durable, because the client discards retry state on ack — so
group-commit per shard as in §3, paying half a batch interval (10 ms) against an 80 ms RTT. Reading then
means replaying hundreds of thousands of operations, so snapshot periodically and open as "newest snapshot
plus a bounded tail" (§7.2).

A document must have exactly one sequencer, so all its sessions must land on one server. Route by `doc_id`
through a consistent hash (Chapter 04), with the assignment held as a **lease** so that two servers never
both believe they own a document — a split sequencer produces two linear histories, the one failure this
architecture cannot recover from gracefully. On failure another server loads the newest snapshot, replays the
tail, and accepts reconnections; clients resubmit, deduplicated by `clientSeq`. The document is unavailable
for the lease timeout and no acknowledged edit is lost.

### Final architecture

```
                        ┌────────────────────────────────────────────┐
Browser ──WebSocket──►  │  Session server (owns doc D by lease)      │
   │ local echo, one    │  ACL check at connect; transform R+1..C;   │
   │ op in flight       │  assign revision; group-commit; broadcast  │
   │                    └──┬─────────────┬──────────────┬────────────┘
   │                       ▼             ▼              ▼
   │                Operation log   Snapshot store   Redis: presence
   │                (sharded by     (object store,   (TTL 60 s,
   │                 doc_id) ──┐     every ~1k revs)  cursors only)
   │                           └──► async compactor ──► revision history
   └──HTTP──► Bootstrap API ──► newest snapshot + tail replay ──► {revision, content}
                  └──► ACL service ──► folder-inherited permissions (cached, 60 s)

   Lease service (etcd / ZooKeeper): doc_id → owning session server, with fencing token
```

---

## 7. Deep dives

### 7.1 Operational transformation versus CRDT

Most material gets the factual premise wrong, so start with the fact:

> **Google Docs uses operational transformation — specifically the Jupiter algorithm — not CRDTs.** So did
> Google Wave. CRDTs are used by Figma, VS Code Live Share, and the Yjs and Automerge ecosystems. Both
> approaches are in production at scale; neither has won.

**OT** treats an operation as positional and context-dependent, and achieves convergence by *changing the
operation*, transforming it forward through operations it did not see; a replica's state is exactly the text.
**A sequence CRDT** instead gives every inserted character a globally unique immutable identifier and a place
in a dense total order ("insert after the character with identifier X," in RGA and its descendants), and
deletion sets a **tombstone** rather than removing anything, because later operations may still reference the
identifier. Operations become *commutative by construction*, so no transformation and no central order are
required — at the cost of a replica state that is the text plus all of that metadata.

| | OT (Jupiter) | Sequence CRDT (RGA / YATA) |
|---|---|---|
| Convergence mechanism | Transform ops against concurrent ops | Commutative ops over unique identifiers |
| Central server | **Required** — a sequencer imposes the order | Not required; merges peer-to-peer |
| Replica state | The text | Text plus per-character metadata and tombstones |
| Correctness risk | Transformation functions; op-type pairs | Metadata growth; interleaving anomalies |
| Cost that grows | Nothing with history (window is latency-bounded) | Metadata grows with **edit history** |
| Adding an op type | O(n²) transform pairs to write and test | Usually local to the new type |

**Why CRDTs were developed, honestly.** Not because OT is slow, but because several published OT algorithms
were *wrong*: the literature from the late 1990s onward is a chain of papers showing that published
transformation functions fail to converge under three-way concurrency — the TP2 property — followed by
corrections themselves shown to fail. A provably convergent peer-to-peer OT function set is genuinely hard,
and getting it wrong yields silent, unreproducible corruption; CRDTs eliminate the class by making
convergence structural rather than a proof obligation over case analysis.

**Why that criticism does not land on Google Docs.** A central sequencer makes concurrency pairwise from any
replica's viewpoint, so TP2 is never required: the hard part of OT is precisely the part a central server
deletes — and Docs was always going to have a central server, for permissions, persistence, sharing, and
history. **OT's central-server requirement costs zero in a product that has one for four other reasons.**
CRDTs win where there is no server or it is off the critical path — peer-to-peer sync, local-first apps,
merging after weeks offline — and where reconnection must be cheap, since §7.5 shows OT's merge cost is
quadratic in the gap while a CRDT's is linear.

**Where the metadata cost actually bites.** The usual figure — 20–50× overhead from per-character identifiers
and permanent tombstones — has largely been defeated: Yjs, Automerge 2, and diamond-types run-length-encode
contiguous insertions by one author, collapsing ordinary prose to a small constant overhead. The residual
cost is narrow but real: heavily interleaved multi-author editing defeats the encoding, and tombstone
collection is only safe when you can bound which replicas are still offline — which, in a local-first
product, you cannot.

**The decision, and its price.** For a hosted editor with a server, permissions, and history: **OT with a
central sequencer.** The price is engineering, not runtime. The transformation functions are the hardest code
in the system, they need property-based testing against a reference model over randomly generated
interleavings, and each new operation type multiplies the work because transformation is pairwise — tables,
range-anchored comments, and tracked changes each add a row and a column to the transform matrix. Teams
starting an editor today often choose a CRDT for that reason; that is defensible, and calling it wrong would
itself be wrong.

### 7.2 The operation log and periodic snapshots

**You cannot store only the current text**, for four reasons, the first of which people miss. Transformation
needs history: a client at revision R submits an operation that must be transformed against R+1..C, and the
text at C cannot supply that — the log is not an audit trail, it is an operand. Reconnecting and offline
clients need the gap. Revision history is a product requirement. And undo needs the operation to invert it.

**Snapshot interval, derived.** Opening costs (load snapshot) + (replay tail), and replaying one
retain/insert/delete against an in-memory rope is roughly a microsecond.

```
every 1,000 revisions   → replay ≈ 1 ms      (invisible)
every 100,000 revisions → replay ≈ 100 ms    (the entire latency budget)
200k-revision doc, 20 KB text, snapshot every 1,000 → 200 × 20 KB = 4 MB vs ~12 MB of log
```

A thousand is comfortable for latency and expensive for storage: snapshots at that frequency cost as much as
the log they shortcut. So **keep only the newest snapshot plus sparse milestones** — one per day of editing,
plus every named version — since history browsing tolerates a few hundred milliseconds of replay. And
**compaction is bounded by your oldest possible client**: operations before R may be discarded only if no
client will ever submit against something older, so offline clients set the horizon directly (§7.5).

### 7.3 Presence and remote cursors

A remote cursor is drawn at a character offset, which makes it **the same kind of object as an operation's
position — and it decays for the same reason.** If Alice's cursor sits at 100 and Bob inserts five characters
at 50, it must render at 105 on Bob's screen; an untransformed cursor drifts with every remote edit and
within a minute points at the wrong word, which users read as the feature being broken. Either **transform
the position** through the same machinery used for operations — nearly free, since a cursor is a degenerate
operation with no content — or **anchor to a character identity**, the natural CRDT representation, which
survives arbitrary gaps without replay but under OT means inventing stable identifiers. Either way the
position must be transformed against operations *the sender had not yet seen*, so a cursor update must carry
its base revision; omitting it is the most common bug in presence implementations.

**Volume, and why presence must not use the operation path.** Cursors move on every keystroke and click:

```
naive:     50 editors × 10 updates/sec × 49 recipients = 24,500 msg/sec for ONE document
coalesced: 50 recipients × 10 snapshots/sec            =    500 msg/sec
```

Throttle at the sender to about 10 Hz and coalesce at the server into one presence snapshot per document per
100 ms — a fiftyfold reduction where each message is a complete picture, so a dropped one is self-healing.
Then enforce the boundary: presence is **not acknowledged, not logged, not retried, not durable**, and
sessions expire on a heartbeat TTL (Chapter 33). Reusing the operation plumbing for cursors because it is
already there fills the durable log with noise and makes compaction impossible.

### 7.4 Access control and sharing

**Resolve the effective role before accepting the WebSocket.** The role gates the socket's capabilities: a
viewer session receives `apply` and `presence`, and any `submit` it sends is rejected — do not rely on the
client hiding the editing UI, because the client is not a security boundary. Effective role is a walk up the
folder tree, most permissive grant winning; at 6.3M peak sessions averaging ten minutes that is roughly
10,000 resolutions per second, so cache resolved `(principal, doc) → role` with a short TTL. **That TTL is a
revocation-latency decision and must be stated as one.** Sixty seconds means a revoked
collaborator keeps access for up to a minute — and for an already-open session the TTL does not help at all,
because the check happened at connect and the socket lives for hours. Revocation must therefore **push**: an
ACL change publishes an invalidation the owning session server consumes, dropping affected sockets with
`revoked`. Without it, "remove access" is a lie for the life of the session — a security bug, not staleness.
Link sharing needs the same care: "anyone with the link can edit" makes the URL a bearer token, and bearer
tokens leak through referrers, screenshots, and browser sync, so the token must be a distinct high-entropy
identifier — never the document id — independently revocable and rotatable.

### 7.5 Offline editing and the reconnection merge

An offline client accumulates operations against a stale revision R while the server advances to C. On
reconnect it submits them all and the server transforms each forward. **The cost is a product, not a sum**:
transforming `m` client operations against `n` server operations takes `m × n` transformations, because each
client operation must cross the whole gap and each transformation shifts the ones behind it.

```
5 min offline, quiet doc:   m=300,   n=50      →  15,000 transforms    ≈ 15 ms      fine
1 day offline, busy doc:    m=5,000, n=100,000 →  5 × 10^8 transforms  ≈ 8 minutes  fatal
```

Eight minutes during which the sequencer is blocked or interleaving this with live editing. This is where
§3's "the window is bounded by latency" stops holding, and it is the strongest single argument for CRDTs in a
local-first product: a CRDT merge is linear in operations, because there is nothing to transform.

Four mitigations, in order of value. **Compose before sending**: five thousand keystroke operations against a
document the client alone was editing collapse into a far smaller set, attacking `m`, the term you control.
**Bound the offline window** to a few days, equal to §7.2's compaction horizon. **Fork instead of merging**
past the bound — what most systems do, because a merge that takes eight minutes and produces text nobody
intended is worse than a second document. And **transform off the sequencer**, running the batch against a
log snapshot on a worker. The client must also persist pending operations locally with their base revision,
or a browser restart destroys work that was never acknowledged.

### 7.6 Undo

**Undo must be selective.** Ctrl-Z undoes *my* last edit, not the document's. Global undo — pop the last
operation off the shared log and invert it — is trivial and wrong: it removes a collaborator's work without
consent, and products that shipped it removed it. To undo my operation `o`, committed at revision `k` with
the document now at `C`, build `o⁻¹` and **transform it against every operation from `k+1` to `C`** — §7.5's
machinery run backwards, which is why the operation form has to be invertible.

Where it gets hard, in three escalating steps:

1. **The inverse may no longer be meaningful.** I delete a sentence; you rewrite half of what replaced it. My
   undo reinserts a sentence into a context that no longer exists. It converges — every replica shows the
   same text — but the text may be text nobody wanted. Convergence and intention come apart most visibly here.
2. **Undos are operations**, so they enter the log and are themselves transformed, including against other
   people's undos. Redo is the inverse of an undo, and "undo of an undo of a concurrent edit" is precisely
   where published OT undo schemes have historically broken. Test with randomly generated interleavings
   against a reference model; hand-written cases do not find these.
3. **The undo stack is per user and must survive reconnection**, and it interacts with §7.5, because
   composed operations have lost the boundaries Ctrl-Z is expected to respect: compose by typing burst, not
   by time window, or undo deletes a paragraph where the user expected a word.

Selective undo is not solved in the general case. Implementations are heuristic; keep granularity small, test
inverse transformation hard, and accept that some interleavings will surprise someone. Saying that is
stronger than claiming the problem is closed.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Session server holding a document dies | Document unavailable for the lease timeout; unacked ops in flight | New owner loads snapshot, replays tail; clients resubmit, deduped by `clientSeq` |
| Split sequencer (two owners) | Two linear histories; permanent divergence — the unrecoverable failure | Lease with fencing token (Chapter 04); reject stale-token writes; page on any occurrence |
| Transformation bug | Silent, unreproducible divergence between replicas | Content hash in every `ack`; client compares and forces full resync on mismatch |
| Presence Redis loss | Cursors and collaborator list vanish; editing unaffected | Presence is derived and TTL'd; refills within one heartbeat |
| Offline client returns with a huge batch | Sequencer blocked for minutes | Off-sequencer two-phase catch-up; hard bound with fork fallback (§7.5) |
| ACL revoked while a session is open | Removed collaborator keeps editing | Push invalidation to the owning server; drop the socket with `revoked` |

The content-hash row deserves emphasis. **Every ack should carry a checksum of the server's state at that
revision**, and every client should verify it. Divergence is the one bug users cannot see happening and
cannot recover from, so detect it cheaply and resolve it bluntly: a forced resync costs a cursor position,
undetected divergence costs the document.

**Monitoring:** divergence-detection rate (nonzero is a page), transform depth per operation (rising means
clients are falling behind), ack p99, log commit latency, presence message rate per document, reconnect rate,
offline-merge duration distribution, lease churn.

---

## 9. Common mistakes

1. **Saying Google Docs uses CRDTs.** It uses operational transformation, specifically Jupiter — the most
   common factual error on this topic, repeated confidently in a great deal of material. Figma and VS Code
   Live Share use CRDTs, which is where the association comes from.
2. **Declaring either approach strictly better.** "OT needs a central server" costs nothing in a hosted
   product that has one anyway, and it removes the TP2 obligation. "CRDT metadata is huge" was largely closed
   by run-length encoding; tombstones and heavily interleaved editing are the narrower claim to make.
3. **Broadcasting positional patches with no transformation**, which diverges within seconds.
4. **Locking paragraphs.** Fails exactly in the case the product exists for, and adds a round trip per word.
5. **Blocking local echo on the server acknowledgment**, which makes typing feel broken at any latency and
   discards the one property both algorithms exist to provide.
6. **Storing only the current text**, which breaks transformation, reconnection, history, and undo.
7. **Forgetting that cursors must be transformed**, and that a cursor update must carry its base revision.
8. **Putting presence in the durable log**, which fills it with noise and makes compaction impossible; or
   **implementing global undo**, so Ctrl-Z deletes a collaborator's sentence.

---

## 10. Variants

**Figma.** A design canvas is a tree of objects with independent properties, not a shared character sequence,
so the hard problem largely disappears: edits to different properties never conflict, and same-property edits
resolve last-writer-wins in a server-assigned order. Figma's published account calls the approach
CRDT-inspired while noting that a central server permits something much simpler than a general CRDT — the
same observation as §7.1, reaching the opposite algorithm because the data model differs.

**Notion and block editors.** A list of blocks, each edited independently, so most concurrent edits touch
different blocks and never interact; the sequence problem is confined to one paragraph, and reordering blocks
becomes a separate list-shaped problem. A real simplification, not a cheat.

**Collaborative code editing (VS Code Live Share, Replit).** A text sequence, so the full problem — but with
a CRDT, because participants are peer-oriented and one machine hosts the workspace. Adds language-server
state, which must be attributed to a revision or the diagnostics describe text that no longer exists.

---

## 11. Further reading

- Chapter 02 §8 for the transport; Chapter 33 for presence and TTL state; Chapter 41 for offline sync and
  conflict resolution in a file-shaped rather than text-shaped system
- Ellis and Gibbs, "Concurrency Control in Groupware Systems" (SIGMOD 1989) — the original OT paper
- Nichols, Curtis, Dixon, and Lamping, "High-Latency, Low-Bandwidth Windowing in the Jupiter Collaboration
  System" (UIST 1995) — the Jupiter algorithm this chapter recommends
- Sun and Ellis, "Operational Transformation in Real-Time Group Editors" (CSCW 1998) — the TP1/TP2 framing
- "Google Wave Operational Transformation" (Google, 2010) — the production protocol Docs inherits
- Shapiro, Preguiça, Baquero, and Zawirski, "A Comprehensive Study of Convergent and Commutative Replicated
  Data Types" (INRIA Research Report 7506, 2011) — the CRDT foundation
- Kleppmann and Beresford, "A Conflict-Free Replicated JSON Datatype" (IEEE TPDS, 2017), and Kleppmann's talk
  "CRDTs: The Hard Parts" — the honest account of metadata and interleaving costs
- Evan Wallace, "How Figma's Multiplayer Technology Works" (Figma blog, 2019)
- Joseph Gentle, "5000x Faster CRDTs: An Adventure in Optimization"
