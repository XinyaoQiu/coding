# Chapter 41 — File Sync (Dropbox / Google Drive)

> **Prerequisites:** Chapters 01 (partitioning, transactions), 02 (caching, stampede, client push), 03 (idempotency), 40 (chunked upload, object storage)
> **Patterns:** content-addressed blocks, delta sync, journal-and-cursor, optimistic concurrency, conflict preservation

---

## 1. The problem

A folder on a laptop is mirrored to a server and, through it, to every other device the user owns and to
every collaborator they have shared it with. Drop a file in; it appears everywhere. Edit it; the edit
appears everywhere. Work on a plane; on reconnect, everything reconciles.

The product looks like storage. It is not — storing bytes durably is Chapter 40's problem, largely solved
by handing them to an object store. What makes this hard is that the replicas run on machines you do not
own, that are offline half the time, that any process on the user's computer can modify behind your back,
and that return from a two-week absence carrying edits conflicting with edits made meanwhile. Two halves
follow: a file is the wrong unit to move over a network, because a one-word edit to a one-gigabyte file is
not a one-gigabyte event; and when replicas diverge, no cleverness on the server can decide which bytes the
user meant to keep.

**The property that makes it hard:** the client is a first-class, frequently-partitioned, independently
mutating replica, and correctness is defined by what happens when replicas disagree. Durability,
throughput and cost all have standard answers. Divergence does not — and the honest answer to divergence
is to refuse to resolve it automatically.

---

## 2. Requirements

### Functional

1. A local folder is kept in sync: creations, edits, deletes, renames, moves.
2. Changes on one device appear on the user's other devices, and in a web client, without user action.
3. A folder can be shared; members see each other's changes.
4. A device that has been offline reconciles on reconnect without losing data.
5. Version history for some retention window.

Defer, but name: selective sync, on-demand placeholder files, comments, full-text search, public links.

### Non-functional

- **Sync latency** — a committed change reaches an online peer at p50 under 3 s, p99 under 15 s. Users
  measure this interval directly and compare it to Slack.
- **Bandwidth efficiency is correctness-adjacent, not an optimization.** Bytes sent must be proportional
  to the size of the change, not the size of the file. §3 shows the whole-file design does not merely cost
  more — it fails to converge.
- **Durability** — eleven nines on blocks; synchronous replication on metadata.
- **No silent data loss, ever, including under conflict.** Separate from durability, and the requirement
  that decides §7.4: a conflict that discards an edit is data loss even though every byte was durably
  stored at some point.
- **Consistency** — a commit is atomic and linearizable *within a namespace*; across namespaces and
  devices, eventual.
- **Availability** — the metadata read path must be up. Block upload may fail and retry indefinitely,
  because the client holds the bytes and is patient.
- **Client resource budget** — the agent must not saturate the uplink, spin the CPU, or drain the battery.
  This sounds like polish; it is the top cause of uninstalls in this category, and §7.7 treats it as a
  design constraint.

### Explicitly out of scope

Real-time collaborative editing of document *contents* (Chapter A0 — a different data model, see §10),
server-side format conversion, and end-to-end encryption with client-held keys, which appears only where it
collides with deduplication in §7.5.

---

## 3. Estimation

A mature consumer product: 700 million registered users, 20 million paying, 50 million active daily across
roughly three devices each — 150 million devices.

```
20M × 200 GB + 680M × 2 GB        ≈ 5.4 EB logical, ≈ 2.2 EB after dedup+compression
average file ≈ 1 MB   → 5.4e18/1e6  ≈ 5.4 × 10^12 live files
                        2.2e18/4 MiB ≈ 5.2 × 10^11 stored blocks
metadata: 5.4e12 × 300 B ≈ 1.6 PB ÷ 4,096 shards ≈ 400 GB/shard
mutations: 50M DAU × 20/day = 1e9/day ≈ 11,600/s avg, 35,000/s peak
                                       ÷ 4,096 shards ≈ 9 write txns/sec/shard
```

Three observations. **Most files are smaller than one block** — nine files in ten fit in one 4 MiB block,
while nine bytes in ten live in files spanning many, so block size is chosen against the byte distribution,
and the small-file majority is why the *metadata* layer carries the request load. Metadata is enormous in
aggregate but trivially partitionable at 400 GB a shard. And nine write transactions per second per shard
means the commit path can afford a real ACID transaction — which licenses §5.

**Change detection — the constraining number.** All 150 million devices must learn about a change within
seconds; naively they ask:

```
poll every  5 s → 30,000,000 requests/sec
poll every 60 s →  2,500,000 requests/sec, with a 60 s latency floor
```

**Thirty million requests per second whose answer is almost always "nothing."** That number shapes the
architecture. It cannot touch a database, and even the sixty-second variant violates the 3-second p50,
so it forces three things at once: persistent or long-polled connections instead of polling, a tier that
answers entirely from memory, and a payload small enough that answering is nearly free.

**Delta sync — deriving the saving.** A video editor autosaving a 1 GB project every five minutes across an
eight-hour day, on a 20 Mbps uplink:

```
saves/day = 8 × 12 = 96
whole-file: 96 × 1 GB = 96 GB → 768,000 Mbit ÷ 20 Mbps = 38,400 s = 10.7 hours
blocks:     1 GB / 4 MiB = 239 blocks; an in-place edit touches 1
            96 × 4 MiB = 402 MB → 3,216 Mbit ÷ 20 Mbps =   161 s = 2.7 minutes
```

Ten point seven hours of upload for eight hours of work. **The whole-file design does not converge** — the
queue grows without bound and the file is permanently stale, a correctness failure dressed as a performance
one. Blocks finish in under three minutes on 0.42% of the bandwidth, a 239× reduction.

---

## 4. API

```
# --- metadata plane ---
GET  /namespaces/{nsId}/delta?cursor=<opaque>&limit=1000
  ->  200 {entries:[{path, fileId, rev, blocklist:[sha256], size, mtime,
                     isDir, deleted, modifiedBy}], cursor, hasMore}
POST /files/commit
  body: {nsId, path, blocklist:[sha256], size, mtime, parentRev, deviceId}
  ->  200 {fileId, rev}
  ->  409 {conflict:"rev_mismatch", serverRev, serverBlocklist}
  ->  412 {missingBlocks:[sha256, ...]}

# --- block plane ---
POST /blocks/probe   body: {hashes:[sha256, ...]}    # up to 1000
  ->  200 {missing:[sha256, ...], uploadUrls:{sha256: <presigned PUT>}}
PUT  <presigned url>  body: raw bytes (<= 4 MiB)     # to object storage, not to us
GET  /blocks/{sha256}/url  ->  200 {url: <presigned GET, CDN-fronted>}

# --- notification plane ---   long poll, 60 s
GET  /notify?subs=[{nsId, cursor}, ...]  ->  200 {changed:[nsId,...]} | {} on timeout
```

**`probe` then `PUT` is the deduplication protocol, and it is client-driven.** The client hashes first and
asks second; the server never receives bytes it already has, which is what makes "uploading" an existing
file instantaneous. It is also the side channel of §7.3, and you should raise that yourself.

**Block upload is idempotent by construction**, because the target name *is* the hash of the content: a
retry after an ambiguous timeout writes identical bytes to an identical key. No idempotency key, no dedup
table, no request-ID bookkeeping — contrast Chapter 03 §4, where all of that is necessary.

**`commit` carries `parentRev`, making it a compare-and-swap** — accepted only if the file is still at the
revision the client based its work on, else 409 with current state. That is optimistic concurrency
(Chapter 01 §8) and the *detection* half of conflict handling; §7.4 is the resolution half. The 412 exists
because the server must verify every hash before committing: without it, a client bug or a lost upload
produces a revision that lists correctly and 404s forever.

**The notification response contains no file data — not even a path**, only namespace IDs. That is what
lets the tier serve 150 million connections from memory with no authorization logic and no database access,
and it bounds what a just-removed member can learn to a single bit.

---

## 5. Data model

```
# ---- metadata: relational, sharded by ns_id ----
namespaces      ns_id PK/shard key, kind ('root'|'shared'), owner_id
memberships     user_id, ns_id, mount_path, role
files           PK(ns_id, path); file_id, latest_rev, is_dir, deleted
file_revisions  PK(ns_id, file_id, rev); parent_rev, blocklist BYTEA, size,
                mtime (the client's claim, never used for ordering), device_id,
                created_at (server clock, authoritative)
journal         PK(ns_id, journal_id); file_id, rev     # monotonic per namespace

# ---- blocks: object storage ----
blocks          key = hex(sha256(plaintext)), value <= 4 MiB, immutable
block_refs      (hash, ns_id)                            # GC and scoped dedup
```

**Metadata is relational and transactional; blocks are not.** A commit must atomically insert a revision,
update `files.latest_rev`, and append a journal entry, or the namespace is corrupt. **Metadata is small,
hot, and needs ACID; blocks are enormous, cold, and need nothing but durability and immutability.** Putting
file bytes in the database, or file metadata in the object store, is the most common way to get this wrong.

**The shard key is `ns_id`, not `user_id`.** A shared folder is its own namespace, mounted into each
member's tree at a path of their choosing. This makes the commit single-shard by construction — all four
tables it touches are keyed by `ns_id` — and records a shared folder's change **once**, not once per
member. §7.6 shows what that saves at 50,000 members.

**The journal is the synchronization protocol.** A per-namespace monotonic `journal_id` is the cursor the
client holds; `delta?cursor=N` returns entries above N, making a sync check proportional to the number of
*changes* rather than of *files*. Without it, a client with 7,700 files must transfer and diff a full
listing to discover that nothing happened.

**The blocklist is packed inline, not one row per block.** A 1 GB file is 239 hashes × 32 B = 7.6 KB, cheap
to read as a unit; one row per block would triple metadata volume and make every file read a range scan.
For very large files — a 1 TB image is an 8 MB blocklist — the blocklist itself is chunked and
content-addressed, the same trick one level up.

---

## 6. Architecture, derived

### Attempt 1: whole files, and poll for a listing

Upload each file to object storage under its path; to sync, fetch the listing and compare against local
state. Two independent failures. The upload path does not converge: §3's editor needs 10.7 hours of uplink
for 8 hours of work, and any user whose edit rate exceeds uplink ÷ file size falls permanently behind — a
100 MB file saved every two minutes on 20 Mbps is already past it. The listing path is worse in aggregate:

```
7,700 files × ~100 B/entry = 770 KB per listing
150M devices ÷ 60 s        = 2.5M listings/sec × 770e3 B = 1.9 TB/s
```

One point nine terabytes per second, sustained, to communicate that nothing has changed.

### Attempt 2: split files into content-addressed blocks

Chunk every file into fixed 4 MiB blocks and hash each with SHA-256. The file's identity becomes an
*ordered list of hashes*; the bytes live in object storage under those hashes. Upload is: chunk, hash,
`probe`, `PUT` what is missing, `commit` the list. Editing one sentence in a 1 GB file yields 239 hashes of
which 238 are unchanged and uploads one block — the 239× saving of §3, plus resumability, idempotency (§4),
and cross-user deduplication (§7.3). Bandwidth is solved; polling is untouched.

### Attempt 3: replace the listing with a journal and a cursor

Give each namespace a monotonic `journal_id`; the client holds a cursor and asks `delta?cursor=N`. An
empty response is ~200 B, so 2.5M polls/sec costs 500 MB/s — 3,800× less than attempt 1. Network is fine;
the other resource is not. Those 2.5 million requests each read one namespace's `journal_id` from a sharded
database to answer "no" — 610 wasted queries per second per shard, scaling with the *idle* population,
which is the worst kind of scaling. And the 60-second interval sets a 60-second floor against a 3-second
p50; shortening it to 3 s multiplies the load twentyfold, to 50 million queries/sec.

### Attempt 4: a notification tier that never touches the database

Extract change detection into a tier holding two things in memory: `ns_id → latest journal_id`, and per
`ns_id` the set of parked long-poll connections. A commit publishes `(ns_id, journal_id)` — one small
message, 35,000/sec at peak. A client long-polls with its cursors; if any `journal_id` exceeds a held
cursor, respond immediately, otherwise park for 60 seconds. The idle path is one hash lookup and one parked
socket, with no database involved at all, and `150M ÷ 50,000 per node ≈ 3,000 nodes` carries it.

Those nodes are stateless caches, rebuildable from the metadata store in seconds and costing their clients
only a reconnect when lost. Latency drops to the publish delay, well inside 3 seconds, because the response
is pushed when the commit lands rather than at the next poll — and the metadata store now sees traffic
proportional to *change*, instead of 50 million polls per second.

### Attempt 5: get block bytes out of the application tier

Blocks are 4 MiB and take seconds to move. Proxying them means every deploy severs in-flight transfers,
app servers are sized for connection-hours rather than requests, and every byte crosses the network twice.
Instead the block service issues **pre-signed URLs** and the client talks to object storage directly, with
downloads fronted by a CDN — immutable content-addressed objects are the ideal CDN payload: infinite TTL,
no invalidation, and a cache key that is already a strong checksum.

### Final architecture

```
 ┌───────────────────── Desktop / mobile client ──────────────────────┐
 │ FS watcher + periodic full rescan (the watcher is lossy — §7.1)     │
 │ local index (SQLite): path → size, mtime, inode, blocklist, base_rev│
 │ chunker (4 MiB) ──► SHA-256 ──► hash list                           │
 │ reconciler: three-way compare   local ⟂ base ⟂ remote               │
 └───┬─────────────────────┬────────────────────────┬──────────────────┘
     │ probe / commit      │ block bytes            │ long-poll
     ▼                     ▼                        ▼
 ┌────────────┐   ┌────────────────┐      ┌──────────────────────┐
 │ Metadata   │   │ Block service  │      │ Notification tier    │
 │ service    │   │ (presign only) │      │ 3,000 nodes, in-mem  │
 └─────┬──────┘   └───────┬────────┘      │ ns_id → journal_id   │
       │ txn              │ presign       │ ns_id → {conns}      │
       ▼                  ▼               └──────────▲───────────┘
 ┌────────────────┐  ┌──────────────┐                │ publish (ns_id,
 │ Sharded RDBMS  │  │ Object store │                │          journal_id)
 │ by ns_id ×4096 │  │ + CDN, 2.2EB │                │
 │ files/revisions│  │ immutable,   │                │
 │ journal/members│  │ presigned    │                │
 └────────┬───────┘  └──────────────┘                │
          └──────────────────────────────────────────┘
 Async: journal ──► block GC (refcount + grace) ──► version-history retention
```

Three planes — metadata, blocks, notification — with different data, different stores, different scaling
laws, and different failure semantics. If you draw nothing else, draw that separation and defend it.

---

## 7. Deep dives

### 7.1 The client is half the system

Interviewers grade the client. A candidate who describes only servers has designed a file store, not a
sync product. Four components, each with a specific failure mode.

**The filesystem watcher** — inotify, FSEvents, `ReadDirectoryChangesW` — is **lossy under load**. Linux's
default `max_queued_events` is 16,384; extracting a 50,000-file archive into the folder overflows it and
the kernel sets `IN_Q_OVERFLOW`, telling you that you missed events but not which. So the watcher can never
be the sole source of truth: the agent also runs a **periodic full rescan**, every few hours and after any
overflow or restart, comparing `(size, mtime, inode)` against the index. Designing around a lossy watcher
rather than trusting it is the detail that most distinguishes people who have built one of these.

**The local index** — SQLite in practice — maps each path to size, mtime, inode, blocklist, and the revision
it last matched, so "has this changed?" is a `stat` rather than a rehash: SHA-256 runs at ~1.5 GB/s with
hardware acceleration and 400 MB/s without, so rehashing a 200 GB folder at every startup costs two to
eight minutes of pegged CPU. The classic bug is one-second mtime granularity, which hides a file rewritten
twice in one second at the same length — hence the rescan also compares inode. Hashing is nonetheless free
relative to the network: a 4 MiB block hashes in 2.8 ms and uploads in 1.7 s on a 20 Mbps uplink.

**The reconciler** compares three states per path: `local` (on disk now), `base` (what the index says was
last in sync), `remote` (what `delta` reports).

```
local == base, remote != base  →  download and apply
local != base, remote == base  →  chunk, probe, upload, commit
local != base, remote != base  →  CONFLICT (§7.4)
```

This is the merge-base structure of version control, and saying so compresses ten minutes of explanation
into a sentence. `base` is why the index must be durable: lose it and every file becomes a potential
conflict, because an unchanged file is no longer distinguishable from a changed one.

Two consequences worth volunteering. **Never write into the user's file in place** — stage on the same
filesystem, `fsync`, then `rename()`, atomic within a filesystem and exactly why staging cannot live in
`/tmp`. And **suppress your own writes**, or a download fires the watcher, which looks like a local edit,
which triggers an upload, a notification, and another download.

### 7.2 Fixed-size versus content-defined chunking

Fixed offsets — 0, 4 MiB, 8 MiB — work beautifully for in-place edits and fail completely for one
operation. **Insert a single byte at the head of a 1 GB file.** Block *i* still covers
`[4Mi·i, 4Mi·(i+1))`, but the content in that range has shifted by one byte, so every block's content
differs and every hash differs: 239 blocks, 239 changed, 1 GB uploaded for a 1-byte insertion, 0% saving.
This is the shift problem, and it is not exotic — prepending a header to a log, inserting a row near the
top of a CSV, `sed -i` adding a line, an application rewriting a container whose header grew by four bytes.

**Content-defined chunking (CDC)** chooses boundaries from content rather than offset. Slide a window of
*w* bytes (typically 48), maintain a **rolling hash** — a Rabin fingerprint — updatable in constant time
per byte, and cut wherever `h ≡ 0 (mod 2^k)`:

```
h_{i+1} = ( (h_i − c_i · p^{w−1}) · p + c_{i+w} )  mod M      # k = 22 → ~4 MiB average
```

Because a boundary depends only on the 48 preceding bytes, inserting a byte at the head shifts the first
boundary but the second — determined by content 4 MiB later — lands where it did before. Boundaries
**resynchronize**: a 1-byte insert changes about two chunks, so ~8 MiB is uploaded instead of 1 GB.

The costs, which make this a real decision rather than an obvious upgrade. Variable sizes need clamping: a
run of identical bytes never triggers a boundary, so a maximum (say 8 MiB) is required, and a forced cut at
the maximum does not resynchronize, so pathological input degrades to fixed chunking anyway. Random access
becomes a prefix-sum over chunk lengths rather than a division. CPU roughly doubles — negligible against
the network, not against a phone battery. And **the parameters are frozen forever**: the scheme is part of
the storage format, so changing the chunk size makes every stored block useless for dedup against new
uploads, and you cannot migrate an exabyte.

**The choice.** For general-purpose file sync, fixed blocks are defensible and are what production systems
here ship, because the corpus is dominated by in-place rewrite (Office autosave, database files, image
editors), append (logs, video capture), and full replacement — the first two of which fixed chunking
handles perfectly and the third no worse than CDC. Boundary-shifting edits are a minority of a minority of
bytes, and rolling-hash CPU on every file plus permanent format complexity is a bad price for them. For a
**backup** product the answer flips — restic, borg and Data Domain all use CDC — because insertion is
normal in machine-generated archive streams, and because (§10) backup has no conflict problem to fund.
**Name the cost:** fixed blocks mean one class of edit costs `O(file)` instead of `O(edit)`, permanently.

### 7.3 Cross-user deduplication, and the side channel it opens

Blocks are named by the hash of their content, so two users who upload the same file store one copy;
neither knows about the other, and filenames and permissions are metadata and unaffected.

```
a 500 MB installer stored by 100,000 users
  no dedup:   100,000 × 500 MB                       = 50 TB
  with dedup: 500 MB + 100,000 blocklists (~4 KB ea) = 900 MB   →  ~55,000×
```

Across a consumer corpus the aggregate saving is more modest — 20–40% of raw bytes — but on 5.4 EB logical,
30% is 1.6 exabytes of hardware. The bandwidth saving is as valuable: "uploading" a 500 MB file someone
else already has takes as long as hashing it.

**The problem.** `probe` is an oracle that truthfully answers, for any 32-byte hash, whether that content
exists somewhere in the service. Three attacks follow, in increasing severity:

1. **Existence disclosure.** Hash a candidate — a leaked document, a specific build, a particular film —
   and learn whether any user has it, with no access beyond your own account.

2. **Content recovery on low-entropy files.** An employer distributes offer letters from a template
   differing only in a salary field; an attacker with the template enumerates candidate salaries, hashes
   each document, and probes — the one that deduplicates is the true content. That is a **read** of data
   never given out, and it works whenever the unknown part of a file is cheap to enumerate.
3. **A covert channel.** Two colluding accounts transmit one bit per probe by choosing whether to upload a
   pre-agreed block, defeating data-loss-prevention monitoring.

Removing the `probe` endpoint does not remove the channel: upload *speed* leaks the same bit. It is
inherent in client-side deduplication, not in the API shape.

**Mitigations, with costs.** **Proof of ownership** challenges the client to prove it holds the whole block
— a keyed hash over server-chosen byte ranges, or a Merkle proof over sub-block leaves — before granting
the shortcut. It stops hash-only bulk download but does **not** close the oracle, because the challenge is
issued only when the block exists, and does nothing about attack (2). **Server-side-only dedup** — always
accept the upload, deduplicate after receipt — closes the channel completely while **preserving the entire
storage saving**, at the cost of the entire bandwidth saving; that is the cleanest middle position and the
one to propose when pushed on privacy. **Scoped dedup** — match only within a namespace, team, or user — is
strongest: members of a shared folder can already read each other's files, so an oracle scoped there
discloses nothing new, while per-user scoping closes it outright and forfeits the 20–40% cross-user saving.
Cross-user dedup is worth a nine-figure hardware line and is a genuine confidentiality weakness; real
services moved toward scoping after this was published (Harnik et al., §11).

### 7.4 Conflicts: detect with version vectors, resolve by keeping both

A laptop and a desktop both hold `report.docx` at revision 7. Both go offline. Both edit. Both reconnect.

**Detection.** The `parentRev` check catches the simple case: the second commit claims parent 7, the server
is at 8, so 409. That suffices here because every commit funnels through one shard that totally orders the
namespace, so a revision DAG with parent pointers captures the causality that matters — a conflict is two
revisions sharing a parent. Version vectors (`{deviceA:3, deviceB:1}`, compared componentwise, "neither
dominates" meaning concurrent) are the general answer and become *necessary* the moment devices exchange
updates without a common server: LAN sync (§7.7), peer-to-peer topologies, multi-master regions. Carry
them, but be precise about why: **a version vector buys causality tracking between replicas that can talk
directly; a parent pointer suffices when every write is serialized by one authority.** Their cost is that
the vector grows with the devices that ever wrote the file, and pruning can turn a real conflict into a
false fast-forward.

**Resolution, the graded part.** Do not implement last-writer-wins.

- **LWW destroys user data silently.** One device's work vanishes with no dialog and no artifact; the user
  discovers it days later, if ever. For a product whose proposition is "your files are safe here," that is
  the one unacceptable outcome, which is why §2 promoted it to a stated requirement.
- **LWW is not even well-defined.** "Last" by which clock? mtimes come from client machines whose clocks
  are wrong or deliberately set backward, so the winner is whoever's clock runs fastest; ordering by server
  arrival is well-defined but arbitrary, rewarding whoever reconnected second.
- **Automatic merging is unavailable.** The server does not know the format, and a three-way textual merge
  of a `.docx`, a `.psd`, or a SQLite file produces something that opens in nothing.

**Preserve both.** The revision that commits first — or that dominates in the version vector — keeps the
path; the other is committed as `report (conflicted copy from Alex's MacBook 2026-09-03).docx`. Both
devices converge on a directory containing both, and the user resolves it, because the user is the only
party that knows which bytes matter.

**The costs, plainly:** conflicted copies are ugly, they proliferate (one misbehaving client in a shared
folder can generate hundreds), and they generate support tickets. All true, and still overwhelmingly the
right trade, because every one of those complaints is recoverable and a single silent deletion is not. Two
refinements remove most of the noise. **Suppress identical-content conflicts** by comparing the two
blocklists — many conflicts are an application rewriting a file with byte-identical content, and you
already have both hash lists, so this is free. **Resolve delete-versus-edit in favor of the edit.** The
general rule, which decides a dozen smaller cases: **when in doubt, keep data.**

### 7.5 Compression and encryption both defeat deduplication

**Encryption.** If the client encrypts each block under a per-user key, identical plaintext yields
different ciphertext, different hashes, and zero cross-user dedup. **Convergent encryption** restores it by
deriving the key from the content, `k = H(plaintext)`. It works and is used in production, but it
reintroduces attack (2) of §7.3 in stronger form: anyone who can guess a plaintext can encrypt it and
confirm the guess. **Deduplication and confidentiality against a guessing adversary are mathematically in
tension**, and no protocol resolves it — a product offering real end-to-end encryption should scope dedup
per-user and say so rather than claim both.

**Compression, and the ordering that matters.** The naive pipeline — compress the file, then chunk the
compressed stream — is a trap. DEFLATE and zstd emit back-references and entropy-coded symbols whose
encoding depends on everything preceding them, so **a one-byte change near the head of a file changes
essentially the entire compressed output** — §7.2's shift problem, reintroduced through the compressor and
now applying to in-place edits too. The correct ordering is:

```
plaintext ─► chunk (boundaries on plaintext) ─► compress each chunk ─► encrypt each chunk ─► store
             the dedup hash is taken over the PLAINTEXT chunk
```

Boundaries are decided on plaintext, so identical regions chunk identically regardless of any compressor,
and hashes are over plaintext, so dedup still matches; compression and encryption become per-block
transforms that do not affect identity. The cost is a worse ratio, because the dictionary resets at every
boundary — and the arithmetic is reassuring. DEFLATE's window is 32 KiB and zstd's is 1–8 MiB at high
levels, so a 4 MiB block is 128 times DEFLATE's window; resetting once per block costs low single-digit
percentage points. **The block is large enough that per-block compression is nearly free**, a second reason
to have chosen a multi-megabyte block. Most bytes in a consumer corpus are already compressed, so compress
speculatively, keep the smaller, and record a one-bit flag.

### 7.6 A shared folder with fifty thousand members

Consider a company-wide folder with 50,000 members averaging two devices each. If the schema were sharded
by user and a change had to be recorded per member, one commit would be 50,000 metadata writes —
Chapter 20's celebrity fanout, reproduced exactly, and at 100 commits per minute, 83,000 writes per second
from one folder against a store sized for nine per shard. **The namespace design already prevents this.** A
shared folder is one namespace with one journal, so a commit is one transaction on one shard regardless of
membership. The fanout moves to read time — Chapter 20's hybrid argument in a different costume, correct
for the same reason: the write cost is unbounded in membership, the read cost is not, and the read hits
trivially cacheable data.

What remains is a read-side thundering herd (Chapter 02 §3): one commit wakes 100,000 parked connections
which all call `delta` on the same namespace with the same cursor and then fetch the same new blocks. Four
responses, all necessary. **Cache the delta response** — for a given `(ns_id, cursor)` the answer is
byte-identical for every member and immutable once the journal advances, so one computation serves all
100,000 requests. **Jitter the client** by 0–5 seconds after a notification, smearing the spike to
20,000/sec well inside the 15-second p99. **Serve blocks from the CDN**, which is what 100,000 downloads of
one immutable object are for. **Rate-limit per namespace**: 1,000 commits/sec is essentially always a
misconfigured client, and since `ns_id` is the shard key you cannot split a hot namespace without giving up
the single-shard commit, so the answer is admission control plus relocation onto a dedicated shard.

A second consequence: a client's sync state is a *set* of `(ns_id, cursor)` pairs, which is why `subs` in
`GET /notify` is a list — a user in 200 shared folders must not open 200 connections. And because
notifications carry no content (§4), the worst case while a membership removal propagates is that an
ex-member learns a folder changed; `delta` is authorized at request time and fails.

### 7.7 Bandwidth, throttling, and LAN sync

An agent that saturates a domestic uplink makes video calls stutter, and the user blames the agent —
correctly. A single TCP stream cannot fill a high bandwidth-delay product: 100 Mbps at 100 ms RTT needs
1.25 MB in flight, which slow-start takes several round trips to reach. Four to eight parallel block
transfers fix that — and are also exactly how you build a standing queue in the user's router. The right
target is **scavenger-class** behavior: use spare capacity, yield instantly. A token bucket (Chapter 04 §3)
with a user-visible limit is the floor; the better answer is delay-based congestion control — LEDBAT
(RFC 6817) targets a fixed 100 ms queuing delay and backs off as one-way delay rises. Naming background
sync as a scavenger workload, and loss-based congestion control as the wrong tool, is sharper than "add a
throttle setting."

**LAN sync.** Fifty machines in a branch office all need the same new 2 GB folder, over a 100 Mbps link:

```
WAN-only:  50 × 2 GB = 100 GB = 800,000 Mbit ÷ 100 Mbps = 8,000 s = 2.2 hours
LAN sync:   1 × 2 GB          =  16,000 Mbit ÷ 100 Mbps =   160 s = 2.7 minutes
                              + peer-to-peer copies at 1 Gbps on the local switch
```

A 50× reduction in WAN transfer. Peers discover each other on the local segment (mDNS or UDP broadcast),
exchange the hash lists they hold, and transfer blocks directly. **Content addressing pays off a third time
here.** A LAN peer is not trusted — it may be a compromised machine on the same office network — and does
not need to be, because the receiver knows the SHA-256 of every block it wants and verifies each one. A
malicious peer can waste your bandwidth; it cannot give you wrong bytes. This is the property BitTorrent
relies on: **content addressing turns untrusted transfer into a performance question, not a security one.**

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Notification node loss | 50,000 clients stop learning about changes | Reconnect with backoff and jitter; correctness unaffected because the cursor is client-held and the tier is pure cache. A polling floor (one `delta` per 5 minutes regardless) bounds worst-case staleness at 5 minutes, not forever |
| Metadata shard down | One set of namespaces cannot commit or read deltas | Blast radius is one shard; clients queue local changes and retry indefinitely; block uploads continue on a different plane, so the backlog drains fast on recovery |
| Object storage region unavailable | Uploads and downloads fail there | Blocks are immutable, so cross-region replication has no conflicts to resolve — serve from a replica and re-point pre-signed URLs |
| Watcher event overflow | Local changes silently missed | Periodic full rescan, plus an unconditional rescan after any overflow flag. Never trust the watcher alone (§7.1) |
| Client clock wrong or moving backwards | Bogus mtimes; catastrophic under LWW | Ordering comes from the server journal, never from client mtime; mtime is a user-facing attribute only |
| Sync loop (agent re-detects its own writes) | Infinite upload cycle, quota burn, hot shard | Match watcher events against recent self-writes; circuit-break on N identical consecutive commits |
| Block referenced by a committed blocklist but missing | A file that lists correctly and 404s — silent data loss | The `412 missingBlocks` check at commit; refcounted GC with a grace period and a rule never to delete a block younger than N days; an offline auditor that walks blocklists |
| Bit rot or corrupt transfer | Wrong bytes delivered | Verify SHA-256 on every read, server and client. Content addressing gives end-to-end integrity for free — use it |
| Runaway client (build directory in the folder) | Thousands of commits/sec on one namespace | Per-namespace and per-device commit rate limits, server-enforced; default ignore patterns; surface it rather than absorbing it |

**Monitoring:** end-to-end sync latency from a canary (a probe writes a file, a probe client measures until
it appears — the only metric that reflects the user's experience); notification connection count and
reconnect rate, since a reconnect storm leads every tier incident; commit 409 rate, where a spike means a
misbehaving client or a clock problem rather than a surge of collaboration; block probe hit rate, the
direct measure of deduplication effectiveness; journal lag per shard; and top-N namespaces by commit rate,
which is how runaway clients are found.

---

## 9. Common mistakes

1. **Designing only the server.** The desktop agent — watcher, index, chunker, reconciler — is at least half
   the system and holds the genuinely hard state machine.
2. **Last-writer-wins on conflicts.** Simpler and wrong: it deletes user data silently, and "last" is
   undefined because the clocks belong to clients. Argue it explicitly rather than reaching for the
   conflicted-copy answer by rote.
3. **Putting file bytes in the metadata database, or metadata in the object store** — or routing block
   bytes through the application tier. They have opposite requirements: small, transactional, constantly
   read versus enormous, immutable, rarely read. The separation *is* the design.
4. **Polling a full file listing.** 1.9 TB/s to say "nothing changed." The journal-and-cursor is not an
   optimization; without it there is no product.
5. **Whole-file upload, or claiming "delta sync" without naming the unit of delta.** Fixed blocks,
   content-defined chunks, or byte-level diffs — pick one and defend it.
6. **Presenting fixed-size chunking as having no failure mode.** The one-byte prepend is the follow-up
   question, and having the CDC answer ready — including why you might still choose fixed blocks — is the
   difference between reasoning and reciting it.
7. **Presenting cross-user deduplication as free.** It is an existence oracle and, on low-entropy files, a
   content-recovery attack. Raise it yourself.
8. **Treating the client as trusted.** The server must verify blocks exist before committing, enforce quota
   and size limits, and — with cross-user dedup enabled — require proof of ownership.

---

## 10. Variants

**Google Drive.** Two products in one namespace: a PDF is a blob and syncs as described here; a Google Doc
is not a file but an operation log, and its sync is Chapter A0's problem. One folder listing must present
two storage models behind a uniform interface — and a Doc has no meaningful "conflicted copy," because
concurrent edits merge at the operation level.

**OneDrive Files On-Demand / iCloud Drive.** Placeholder files occupying a name but no bytes until opened.
The sync engine becomes a filesystem filter driver and the question shifts from "what do I transfer" to
"what do I materialize and when do I evict it" — an eviction problem (Chapter 02 §4) whose cache miss is a
blocking `open()` the user is watching.

**Backup products (Time Machine, restic, Borg, Data Domain).** Write-once, read-almost-never, and — the key
structural difference — **one writer per repository, so there are no conflicts at all.** The complexity
budget §7.4 spends on divergence goes to chunking and dedup instead, which is why these systems use CDC and
aggressive global dedup while sync products often do not. Noticing that the absence of one problem funds
the solution to another is what distinguishes a design discussion from a recital.

**Peer-to-peer sync (Syncthing, Resilio).** No central journal and no serialization point, so version
vectors become mandatory (§7.4) and there is no authority to break ties — correspondingly more conflicted
copies and a harder story for shared folders.

**Git, and container image distribution.** Git is the same three-way merge against a `base` with the same
content addressing, but with explicit synchronization, a full DAG instead of a linear journal, and
format-aware merging for text. Container layers are content-addressed blocks, and the probe-then-push
protocol and the peer-assisted distribution of §7.7 transfer unchanged.

---

## 11. Further reading

- Chapter 40, for chunked upload and the object-storage path; Chapter 02 §3, for the herd control in §7.6;
  Chapter A0, for the model Google Docs uses instead of file sync
- Muthitacharoen, Chen and Mazières, "A Low-Bandwidth Network File System" (SOSP 2001) — the canonical
  description of Rabin-fingerprint content-defined chunking
- Tridgell, "Efficient Algorithms for Sorting and Synchronization" (PhD thesis, ANU, 1999) — the rsync
  rolling checksum, ancestor of everything in §7.2
- Harnik, Pinkas and Shulman-Peleg, "Side Channels in Cloud Services: Deduplication in Cloud Storage"
  (IEEE Security & Privacy, 2010) — the source for §7.3
- Halevi, Harnik, Pinkas and Shulman-Peleg, "Proofs of Ownership in Remote Storage Systems" (ACM CCS 2011)
- Zhu, Li and Patterson, "Avoiding the Disk Bottleneck in the Data Domain Deduplication File System"
  (USENIX FAST 2008) — production-scale deduplication indexing
- Drago et al., "Inside Dropbox: Understanding Personal Cloud Storage Services" (ACM IMC 2012)
- Dropbox Engineering, "Rewriting the heart of our sync engine" (2020) and "Inside the Magic Pocket" (2016)
- RFC 6817, "Low Extra Delay Background Transport (LEDBAT)" — the scavenger congestion control of §7.7
