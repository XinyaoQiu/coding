# Chapter 40 — Video Platform (YouTube)

> **Prerequisites:** Chapters 01 (object storage, store selection, the outbox pattern), 02 (CDNs §6, cache keys and TTLs), 03 (queues, at-least-once, idempotency), 04 (observability)
> **Patterns:** direct-to-blob chunked upload, resumable transfer, DAG batch processing, fan-out/fan-in parallelism, adaptive bitrate delivery, tiered storage

---

## 1. The problem

A user uploads a video file. Some minutes later, anyone in the world can watch it — on a phone over a
degraded cellular link or on a television over fiber — at whatever quality their connection supports at
that instant, starting within a second of pressing play, seeking anywhere without a stall.

The question is asked constantly because it is the cleanest available test of whether a candidate can
design a *pipeline* rather than a request handler. Nothing in it is subtle the way Chapter 20's celebrity
fanout is subtle; everything in it is large.

**The property that makes it hard:** the unit of work is enormous at both ends, and enormous in two
different ways. On the write side a single logical operation moves gigabytes and then spends hours of CPU
turning them into a dozen derived copies — so the write path cannot be a request, and cannot even be one
background job. On the read side the same object is delivered billions of times, and bytes delivered
exceed bytes ingested by roughly three orders of magnitude, which makes egress the dominant cost of the
business. The design is therefore two decompositions: **chunk the upload so no server ever holds the
file, and chunk the transcode so no worker ever encodes the whole video.**

---

## 2. Requirements

### Functional

1. Upload a video with metadata, resumably, from a flaky client.
2. Transcode it into multiple resolutions and codecs and make it playable.
3. Stream it with adaptive quality and arbitrary seeking.
4. Count views.

Defer, but name: search (Chapter 61), recommendations (Chapter A4), comments (Chapter 23), the
subscriptions feed (Chapter 20), monetization, playlists, DRM.

Ask one clarifying question early: **is upload-to-playable latency a product requirement?** The answer
decides whether the pipeline needs §7.1's parallel design or whether one worker per video would do. For a
consumer platform it always is — creators refresh the page — but making the interviewer say it converts
§7.1 from a flourish into a derived requirement.

### Non-functional

- **Playback join time** — p99 under 1 second to first frame, which forces the manifest and first segment
  to sit at a CDN edge and the lowest ladder rung to be genuinely small.
- **Rebuffer ratio** — under 0.5% of playing time; it correlates with abandonment far more strongly than
  absolute quality does, and it is what the client's ABR algorithm optimizes.
- **Durability and resumability** — once `complete` returns, the source must never be lost (use what the
  object store gives you), and a client switching from Wi-Fi to cellular mid-upload continues, not restarts.
- **Publish latency** — a few minutes for a ten-minute video, and *also* a few minutes for a two-hour one.
  That the requirement is independent of length is the entire reason for §7.1.
- **Read availability** — playback survives loss of the upload path, the transcode fleet, the metadata
  database, and a region. It is static file delivery; it should be the most available thing you own.
- **Cost** — first-class here. A 20% bitrate or storage reduction is a nine-figure line item, and several
  decisions (§7.4, §7.5) exist only to serve it.

### Explicitly out of scope

**Live streaming is a different problem and should be scoped out loudly.** It has no batch transcode
stage, because there is no file to split; it encodes a continuous stream against a hard deadline, targets
2–5 seconds glass-to-glass with low-latency HLS or sub-second with WebRTC, cannot pre-position anything,
and loses a dropped segment forever rather than retrying it. A shared player and CDN are the extent of the
overlap, and one architecture serving both serves neither. Also out: recommendation ranking, copyright
disputes, ad insertion, codec internals.

---

## 3. Estimation

Assume the commonly quoted **500 hours uploaded per minute** and a raw source averaging **1 GB per hour**
(≈2.2 Mbps — a blend of short phone clips and long high-bitrate uploads).

**Ingest bandwidth**

```
500 hours/min × 1 GB/hour = 500 GB/min
500 GB / 60 s             ≈ 8.3 GB/s ≈ 67 Gbps sustained
720,000 hours/day → 720 TB/day → ≈ 263 PB/year of source
```

Eight gigabytes per second, continuously, in objects averaging a gigabyte. No stateless application tier
absorbs and forwards that, and no per-request timeout survives contact with it.

**Stored bytes — the ladder multiplier.** A 1080p source produces an H.264 ladder of roughly:

```
240p 0.3 + 360p 0.6 + 480p 1.1 + 720p 2.5 + 1080p 4.5  ≈ 9.0 Mbps = 4.05 GB per source hour
```

Against a 2.2 Mbps source that is **4×** before anything else. Keeping the original (mezzanine) adds 1×.
A more efficient codec — VP9 or AV1 at 50–60% of H.264's bitrate for equal quality — would add 2.4× more
if generated for everything, which is why §7.4 generates it lazily. Call the realized multiplier **5×**;
the usual 3–5× range is entirely a function of how much of the ladder you build eagerly. That gives
`263 PB/year × 5 ≈ 1.3 EB/year` of new stored bytes.

**Transcode compute.** The full H.264 ladder costs roughly 2 core-hours per source hour: the top rung is
near 1× realtime on a core, lower rungs are cheap, decode is shared.

```
720,000 source-hours/day × 2 = 1.44M core-hours/day ÷ 24 = 60,000 cores, sustained
```

About a thousand large machines running continuously just to keep pace with ingest. AV1 costs 10–50×
H.264 for the same output, so AV1 for everything would need 600,000–3,000,000 cores — that one comparison
is the whole argument of §7.4, derived rather than asserted.

**Egress — the constraining number.** Assume a billion watch-hours per day at 3 Mbps delivered.

```
1e9 watch-hours/day ÷ 24 h    ≈ 41.7M concurrent streams (average)
× 3 Mbps                      ≈ 125 Tbps average; peak (~2.5×) ≈ 310 Tbps
bytes/day: 1e9 × 3600 × 3e6/8 ≈ 1.35 EB/day
```

Compare the sides: **1.35 EB delivered per day against 0.72 PB ingested — a byte ratio near 1,900:1.**
That ratio is the most important fact in the chapter: the origin must serve essentially none of the read
traffic (a 99.9% CDN offload target is arithmetic, not ambition), the shape of the bitrate ladder
multiplies the largest cost in the business, and any decision trading read bytes for write work is almost
certainly correct. Two constraining numbers, one per path: **8.3 GB/s of ingest that must never touch an
application server**, and **310 Tbps of peak egress that must never touch an origin.**

**Metadata.** At a 10-minute average, 500 hours/min is 3,000 videos/min ≈ **50 videos/sec** — a rounding
error, and sharding it elaborately is this question's version of the Chapter 10 trap.

---

## 4. API

```
POST /videos
  body: {title, description, visibility, filename, totalBytes, contentType}
  ->    201 {videoId, uploadId, partSize,
             parts: [{partNumber, url, byteRangeStart, byteRangeEnd}, ...], expiresAt}

PUT  <presigned part url>                    # DIRECTLY to blob storage, never to us
  ->    200 {etag}
GET  /videos/{videoId}/upload
  ->    200 {receivedParts:[1,2,5], missingParts:[3,4,6],
             parts:[{partNumber, url, ...}]}  # fresh presigned urls for what is missing

POST /videos/{videoId}/upload/complete
  body: {uploadId, parts:[{partNumber, etag}, ...]}
  ->    202 {videoId, state:"processing"}     # 409 if a part is missing or an etag mismatches
GET  /videos/{videoId}
  ->    200 {state:"processing"|"ready"|"failed", manifestUrl, thumbnails[], duration}

GET  https://cdn.../v/{videoId}/master.m3u8              # served by the CDN, never by us
GET  https://cdn.../v/{videoId}/h264_1080p/seg_00042.m4s
```

**The client uploads to blob storage, not to us.** `POST /videos` returns presigned URLs — time- and
scope-limited credentials to write one byte range of one object — and the application tier never sees a
video byte. It is the only way to satisfy 8.3 GB/s without a fleet whose sole job is copying bytes between
sockets, and it removes the largest source of tail latency from the service serving metadata reads.

**`complete` is a separate call and it is the commit point.** It verifies every part's presence, size, and
etag, has the store assemble the multipart object, writes the `videos` row, and emits the transcode event.
Everything before it is uncommitted scratch. **It returns 202, not 200** — the video is durable but not
playable, and exposing `processing → ready` in the read model instead of blocking is what lets the client
render progress rather than a spinner of unbounded duration.

---

## 5. Data model

```
videos                                  # relational; ~50 writes/sec, tiny
  video_id      UUID  PK
  user_id       BIGINT
  title, description, visibility
  state         ENUM(uploading, uploaded, processing, ready, failed, blocked)
  duration_ms   INT NULL                # unknown until the preprocessor probes
  source_uri    TEXT                    # object-store key of the mezzanine
  created_at, published_at

uploads
  upload_id UUID PK, video_id, total_bytes, part_size
  state ENUM(open, completed, aborted), expires_at    # abandoned uploads must be reaped
renditions
  video_id      UUID  partition key
  rendition_id  TEXT  clustering key    # "h264_1080p", "av1_720p"
  codec, height, bitrate_bps, storage_uri
  storage_class ENUM(hot, infrequent, archive)
  state         ENUM(pending, encoding, ready, failed)

transcode_tasks                         # the DAG's runtime state; transient
  job_id UUID partition key
  task_id TEXT clustering key           # "encode:h264_720p:seg_00042"
  stage, segment_no, depends_on[], state, attempt, worker_id, output_uri
```

**The metadata store is relational and unsharded**: fifty writes per second with read-your-writes on the
creator's dashboard is one Postgres primary with room to spare, and Chapter 01's checklist points at that
boring answer, so take it. **Segments are not rows.** A two-hour video at 3-second segments across five renditions is 12,000 objects,
and that holds for every video ever uploaded — a table four orders of magnitude larger than the catalog,
serving a query nobody runs. The authoritative list of segments is the **manifest**, itself a small file
in object storage served by the CDN. `transcode_tasks` is the justified exception, because the scheduler
must know which of twelve thousand tasks failed; it is transient, TTL'd on job completion, so its size
tracks in-flight work rather than the catalog.

---

## 6. Architecture, derived

### Attempt 1: upload through the app server, transcode in the request

```
Client ──POST (1 GB body)──► App server ──► ffmpeg ──► blob storage ──► 200 when done
```

Fails on the first number. A gigabyte body cannot be held in a request handler, every load balancer
between client and server has a body limit and an idle timeout measured in tens of seconds, and encoding
one source hour is two core-hours — a one-hour video holds the connection open for roughly two hours.

### Attempt 2: async transcode, upload still through the app tier

Accept the upload, write it to object storage, publish `video_uploaded`, return, let workers transcode.
The transcode half is now right in shape. The upload half breaks at **8.3 GB/s**: sustaining 67 Gbps
through a stateless tier means dozens of machines shuttling bytes, each buffering gigabyte requests, each
a point at which a reset destroys a whole upload — fatal independent of throughput:

```
1 GB over a 5 Mbps mobile uplink = 1e9 × 8 / 5e6 ≈ 1,600 s ≈ 27 minutes
```

Twenty-seven minutes of uninterrupted mobile connectivity is not a thing that reliably happens, and an
all-or-nothing upload fails often enough at 90% that users conclude the product is broken.
**Resumability is not a feature request; it is what happens when transfer time exceeds the mean time
between network events.**

### Attempt 3: presigned, chunked, resumable, direct-to-blob upload

Split the file client-side into parts — 5–10 MB, small enough that losing one is cheap, large enough that
per-request overhead is negligible. The client PUTs each part directly to the store with a presigned URL,
retrying parts individually; on resume it asks which are missing and sends only those.

```
Client ──► API: POST /videos ──────────► presigned part urls
   ├── PUT part 1..N ──────────────────► Blob storage   (no app server in the byte path)
   └──► API: POST .../complete ──► verify ──► videos row + outbox ──► transcode event
```

The application tier now handles three small JSON requests per video — roughly 150 requests/second in
total — while 8.3 GB/s flows past it. Byte path and control path are separated and scale independently.

Two details carry weight. **Presigned URLs** delegate authority: the API tier, which knows whether the
caller may upload, mints a signed URL for one part of one object for a short window, and the store
validates it — authorization stays in your service while bytes never enter it. Scope it to a key, never a
prefix, or a leak becomes storage-wide write access. And **part size is a trade-off**: a 100 MB part lost
at 99% wastes 100 MB of uplink, tiny parts multiply request overhead, and at 5–10 MB a failed part costs
about fifteen seconds of a 5 Mbps uplink. Return it from the server so it is tunable without a release.

**Transcoding must not begin before the commit.** Start encoding when the first parts land and you encode
a truncated file; if a retry rewrites a part you encode a file that no longer exists; if the upload is
abandoned you have burned core-hours on garbage. Worst, a partially transcoded video can reach `ready` and
publish — a silent corruption no monitoring catches, because every individual component succeeded.

The commit must also be atomic with the event emission, or you get videos stuck in `processing` forever
(row, no job) or orphan jobs (job, no row). Write the row and an outbox record in one transaction, publish
from the outbox (Chapter 01, §9), and make it idempotent on `video_id` (Chapter 03, §4).

### Attempt 4: one worker transcodes the whole video

Download the mezzanine, run ffmpeg once per rung, upload, write the manifest. This is correct, and it is
what you should build first. It breaks three ways.

**Wall-clock is proportional to duration.** At 2 core-hours per source hour, even with rungs in parallel
on a 16-core machine, a two-hour video takes on the order of an hour — violating publish latency by
construction.

**Failure is catastrophic.** A worker dying at 95% has produced nothing reusable, and the retry redoes the
whole hour; as job duration grows, the probability of finishing without a preemption or a bad disk falls,
and past some length the expected number of attempts stops being close to one.

**The fleet head-of-line blocks.** Long videos occupy workers for hours, starving short videos behind them
and stretching the queue's latency tail to the longest video anyone uploads — unbounded.

### Attempt 5: split the source into independently decodable segments

Cut at Group-of-Pictures boundaries into 2–5 second segments. Each is decodable in isolation, so each can
be encoded by a different worker, concurrently, and the outputs concatenated. **Transcode wall-clock stops
being a function of video length and becomes a function of the time to encode one segment**, plus the
serial preprocess and assemble passes (§7.1).

### Attempt 6: make it a DAG, not a pipeline

Segmented work has dependencies — probe before split, split before encode, all encodes of a rendition
before its concatenation, all renditions before the manifest — and, orthogonally, thumbnails, captions,
fingerprinting, and moderation, each depending on the source or one rendition and nothing else. Hard-coded
sequencing means every new capability edits the pipeline; a **DAG generated from per-video configuration**
makes it a new node (§7.2), and a video needing no captions simply has a smaller graph.

### Final architecture

```
                     ┌──────────── Blob storage (mezzanine) ◄── PUT parts (direct from client)
                     │                     │
Client ──► API tier ─┤ POST /complete      │
            │        └─ videos row+outbox  │
            ▼                 │            │
    Postgres (videos,         ▼            │
    uploads, renditions)  Kafka: video_uploaded
                                  │        │
        ┌─────────────────────────▼────────▼──────────────────────┐
        │  Preprocessor ── probe (codec, res, fps, GOP) ── split   │
        │  DAG scheduler ── graph from config; deps; retries;      │
        │                   speculation                            │
        │  Resource manager ── worker allocation, priority         │
        │  Task workers  [seg 1][seg 2]…[seg N] × each rendition   │
        │                 (stateless, parallel across the fleet)   │
        │  Assemble ── concat per rendition ── package (fMP4/CMAF) │
        │  Manifest writer ──► master.m3u8 / .mpd ──► publish      │
        │  side branches: thumbnails │ captions │ content-ID │     │
        │                 moderation (gates publish, not encode)   │
        └────────────────────────┬─────────────────────────────────┘
                                 ▼
                Blob storage (segments, manifests) ──► origin shield
                                 │
                                 ▼
                        CDN edge ──► Player ──► view events ──► Kafka ──► Ch. 70
```

Read it as three systems sharing only object storage: a small control plane, a large batch compute plane,
and an enormous read-only delivery plane — each able to fail without the others noticing, which is exactly
the availability property §2 asked for.

---

## 7. Deep dives

### 7.1 GOP-aligned chunking — the idea that makes the pipeline work

Video is compressed by predicting frames from other frames. A **Group of Pictures** starts with an I-frame
(a complete, independently decodable image) and continues with P- and B-frames encoding only differences.
A P-frame is meaningless without the frames it references.

That is why you cannot split a video at an arbitrary byte offset and hand the halves to two workers: the
second half begins mid-GOP, referencing frames in the first half, and cannot be decoded at all. **Split at
GOP boundaries** — at I-frames, with the GOP *closed* so nothing in it references outside it — and each
segment is a complete, self-contained little video, encodable with no knowledge of its neighbors.

Here is what that buys. Take a two-hour source at 3-second segments:

```
7,200 s / 3 s               = 2,400 segments
full ladder                 ≈ 2 core-hours per source hour
per segment: 3 s × 2        = 6 core-seconds
total work: 2,400 × 6 s     = 4 core-hours       (unchanged — only schedulability changed)
with 500 workers: 2,400/500 ≈ 5 waves × 6 s ≈ 30 seconds of encoding wall-clock
```

Compare a ten-minute video: `600/3 = 200` segments, one wave on 200 workers, **≈6 seconds**. Both finish
in well under a minute of encode time. **A two-hour video and a ten-minute video publish in roughly the
same wall-clock**, which is the §2 requirement and is unachievable any other way.

**The serial ends are what actually bound you**, and saying so separates understanding the idea from
reciting it. Preprocessing reads the whole source and assembly writes the whole (4× larger) output; both
are linear in file size. Amdahl then caps the speedup: going from 500 workers to 5,000 takes the encode
phase from 30 seconds to 6 and leaves a job that still takes a couple of minutes.

**Why 2–5 seconds and not 0.5 or 30.** *Compression*: every segment begins with an I-frame, several times
the size of a P-frame, so short segments cost bitrate at equal quality — half-second segments can cost
10% or more, enormous against §3's egress. *Request count*: at 2 s a two-hour video is 3,600 segment
requests per viewer per rendition; at 10 s, 720. *ABR responsiveness*, pushing the other way: quality can
only change at a boundary, so segment duration is the reaction time of the whole adaptive system, and
ten-second segments mean ten seconds of stuttering before the player can step down. *Granularity*: there
must be more segments than workers or the fan-out does nothing.

**Alignment must hold across renditions.** If 720p has I-frames at 0/3/6 s and 1080p at 0/2.5/5, the
player cannot switch between them at a boundary without a glitch or a decoder reset. Every encode must be
told to force keyframes at identical timestamps (`-force_key_frames`, fixed closed GOPs, no scene-cut
insertion). This is the most common way a hand-rolled ladder produces a player that stutters on every
quality change, and the cause is invisible unless you know to look.

### 7.2 The DAG, and why the pipeline is shaped as one

**The preprocessor** probes the source (container, codecs, resolution, frame rate, color space, audio
tracks, GOP structure), decides the ladder — you do not generate 1080p from a 480p phone video, and saying
so aloud saves a large fraction of the storage bill — and performs the split.

**The DAG scheduler** builds the task graph from configuration and tracks it: independent stages in
parallel (all segments of all renditions, all thumbnails), dependent stages in sequence (encode → concat →
package → manifest → publish), with retries, dead-lettering, and speculation policy for stragglers — safe
because a segment encode is deterministic, so a duplicate write is harmless.

**The resource manager** allocates workers to runnable tasks, and is separate because its concerns differ:
bin-packing heterogeneous work onto heterogeneous hardware (AV1 wants different machines than thumbnail
extraction), enforcing priority (a short video from a large channel ahead of a two-hour upload from a new
account), and reclaiming capacity when ingest spikes. One component doing both dependency tracking and
bin-packing becomes unmaintainable fast.

**Now the argument.** Once this exists, a new capability costs one node and no changes elsewhere.
Thumbnails depend on the source alone and gate nothing but publish. Captions depend on the audio track the
preprocessor already demuxed, are slow and unreliable, and hang off the graph as a node whose failure is
non-fatal — the video publishes without them and the node retries later. Content-ID fingerprinting depends
on a low rung the ladder already produced, so it reuses work rather than adding any. Moderation runs over
sampled frames and gates *publish* rather than encoding, so it adds nothing to wall-clock unless it fails.
Each is a node with declared inputs, a declared output, and a declared failure policy. **That is the
payoff of a DAG over a pipeline**, and it generalizes: when the set of things you may want to do to an
object is open-ended and mostly independent, encode the dependencies as data and let a scheduler run them
rather than writing control flow that must be edited for every addition.

### 7.3 Adaptive bitrate: the ladder, the manifest, the codecs

A single encode cannot serve both a 4K television on fiber and a phone on a congested cell: at 15 Mbps the
phone stalls permanently, at 800 kbps the television shows mush. So encode the same content at several
(resolution, bitrate) points, segment each at identical timestamps, and let the *client* choose per
segment.

**The manifest** — `.m3u8` for HLS (RFC 8216), `.mpd` for DASH (ISO/IEC 23009-1) — is a small text file
listing renditions and their segment URLs. The player fetches it, picks a rung, measures the throughput
and duration of each segment fetch, estimates buffer occupancy, and picks the rung for the next segment;
because switches happen at timestamp-aligned boundaries, the change is seamless. Two consequences: the
*player* decides, because only it knows its buffer and measured throughput, and quality is per-segment, so
it tracks bandwidth rather than being fixed at press-play.

**Codec choice.** H.264 is the compatibility floor — hardware-decodable on essentially everything ever
shipped — and you must have it. VP9 and AV1 give equal perceptual quality at 50–60% of the bitrate, a
direct 40–50% cut in the largest cost in §3, but cost 10–50× more CPU and only decode on newer hardware.
The answer is both: H.264 always, an efficient ladder for clients that advertise support, with the *when*
decided by popularity (§7.4). **Package once, serve both** manifest formats by emitting fragmented MP4
(CMAF); MPEG-TS for HLS plus fMP4 for DASH doubles segment storage for zero quality benefit.

**A fixed ladder is right here and wrong in Chapter 42.** With 500 hours arriving per minute you cannot
search for an optimal ladder per video; you index a fixed table by source resolution. When the catalog is
small and encode time free, you optimize per title — the inversion of Chapter 42, §7.1.

### 7.4 Storage and cost: the ladder multiplier and the long tail

§3 produced 1.3 EB of new stored bytes per year. Two facts make it attackable. Views are power-law
distributed to an extreme degree, so *stored* and *served* bytes barely overlap — you hold petabytes that
will never be read again after their first week. And the ladder is generated eagerly but consumed
selectively: nobody watches the 1080p rendition of a video with eleven views.

**Tier the storage.** Move renditions of dormant videos to infrequent-access after 30 days and archival
after a year — roughly an order of magnitude cheaper. The trade is retrieval latency and fees, so the
policy must be *per-rendition*: keep low and mid rungs hot, since they are small and are what a long-tail
viewer is actually served, and archive the 1080p and 4K rungs, where the bytes are. If someone plays an
archived video, serve a hot rung immediately and restore the rest in the background — degraded quality is
invisible, a two-minute stall is not.

**Do not generate expensive codecs eagerly.** From §3, AV1 for everything is 600,000–3,000,000 cores; AV1
for the top 1% by views is 6,000–30,000, comparable to the whole H.264 budget and therefore affordable.
Encode H.264 at upload and **trigger an AV1 re-encode when a video crosses a view threshold** — the
economics are exact, since bitrate savings scale with views while encoding cost does not. The mechanism is
a stream job over the view counters (Chapter 70) emitting a low-priority `re-encode` event into the same
DAG, on reclaimable capacity.

**Cap the ladder by source** — never upscale; a 480p source gets three rungs, not five. And **keep the
mezzanine** in archival storage: it costs 1× of everything ever uploaded and buys re-encoding when a better
codec arrives, which given H.264 → VP9 → AV1 → next is a bet that keeps paying. An option purchased, not
an accident.

### 7.5 Delivery: why the CDN carries essentially all of it

Peak egress is ~310 Tbps and the read:write byte ratio is ~1,900:1. No origin serves that, so the offload
target is not 90% but something like 99.9%. Fortunately video segments are the most cacheable objects that
exist: they are **immutable** — `seg_00042.m4s` for a given rendition never changes — so TTLs can be
effectively infinite, there is no invalidation problem, and versioned URLs make purges unnecessary
(Chapter 02, §6). Long TTL on segments, short TTL on the master manifest, which is the only thing that
changes when a rendition is added or a video is taken down.

Cache behavior splits along the same power law as storage. **Head content** is hot at every edge, hit
rates near 100%, one origin fill per edge per video; this is the traffic, and it is nearly free.
**Long-tail content** misses — eleven viewers near eleven different edges — and the fix is an **origin
shield**, a mid-tier cache between edges and object storage, so a cold object is fetched once per region
rather than once per point of presence. Finally, **multi-CDN**: segment hostnames are chosen per session
in the generated manifest, which is possible only because the manifest is per-request while the segments
are static. Chapter 42 takes that much further.

### 7.6 View counts are not a database increment

`UPDATE videos SET views = views + 1` is wrong for three independent reasons, and the reasons matter more
than the conclusion.

**Rate.** A billion watch-hours per day at a ten-minute average is on the order of six billion view events
per day — ~70,000/sec average, several times that at peak, before the heartbeats a real player sends every
few seconds. The metadata database of §5, sized for fifty writes per second, is not built for it.

**Contention.** Views follow the same power law, so a trending video concentrates a large share of all
increments on a **single row**. Row locking serializes them; the row becomes the hottest write in the
system and its latency scales with the video's popularity. Sharding the counter fixes contention but not
rate, and adds a scatter-gather to every read.

**Semantics.** A view is not an HTTP request. It needs a watch-time threshold, deduplication, bot
filtering, and the ability to be *retracted* hours later when fraud is found — none of which is
expressible as an increment, all of which is a stream computation over an event log.

So the player emits view and heartbeat events to Kafka; a stream job windows, deduplicates, filters, and
aggregates into per-video counters read through a cache. The count is eventually consistent and slightly
delayed, which is both acceptable and universally observed. **Chapter 70 is this pipeline in full.** Note
the second use of the same stream: it drives §7.4's popularity-triggered re-encode and the tiering
decision — building it as an event log rather than a counter is what makes those possible at all.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Client network drops mid-upload | Partial multipart object; no user-visible loss | Resumable parts; `GET /upload` returns what is missing; lifecycle rule aborts abandoned uploads |
| `complete` succeeds, event lost | Video stuck in `processing` forever | Transactional outbox (Ch. 01 §9) plus a reaper that re-emits for rows past an SLA |
| Task worker dies mid-segment | One segment missing | Scheduler retries that task; cost is seconds, not the video |
| Straggler worker | Job wall-clock set by one task | Speculative duplicate past a multiple of the stage median |
| Transcode fleet saturated | Publish latency climbs for everyone | Priority by length and channel tier; shed low-priority re-encodes first; a backlog is a queue, not an outage |
| CDN edge or region loss | Playback failures in a geography | Multi-CDN steering in the per-session manifest; player retries the next host |
| Metadata DB down | No uploads or new manifests; **playback continues** | Playback depends only on CDN plus object storage — a designed property, not luck |

**Monitoring:** upload completion rate by client platform and connection type (the best early signal that
something in the part flow regressed); publish latency p50/p99 **bucketed by source duration** — the whole
point of §7.1 is that the buckets look alike, so divergence is the alarm; transcode queue depth and
per-stage failure rate; CDN offload ratio and origin fill bytes; and client-side join time, rebuffer ratio,
and delivered bitrate, which are what the viewer experiences and are invisible from the server.

---

## 9. Common mistakes

1. **Uploading through the application tier.** 8.3 GB/s does not flow through a stateless service that is
   also serving your API. Presigned, direct-to-blob, always.
2. **Treating the upload as one request.** A 27-minute mobile transfer that must not be interrupted is a
   broken product — and the *server* must be the authority on which parts are missing.
3. **Starting transcoding before the commit.** Produces truncated renditions that pass every health check
   and publish as good; `complete` exists to be the ordering barrier.
4. **Transcoding the whole video in one worker.** Publish latency and retries both proportional to length,
   plus head-of-line blocking of the fleet.
5. **Splitting at arbitrary offsets rather than GOP boundaries**, or splitting each rendition at different
   keyframes. The segments are then not independently decodable, or not interchangeable, and the design
   silently does not work — an invisible bug with a visible symptom.
6. **Storing a row per segment.** Trillions of rows serving a query nobody makes; the manifest is the
   index, and it lives in object storage.
7. **Incrementing a view counter in the database.** Wrong on rate, on contention, and on semantics.
8. **Ignoring cost.** Generating the full ladder plus AV1 for every upload and keeping it all hot is
   technically correct and economically absurd; "we only encode AV1 for videos that get traction" is one
   of the highest-signal sentences available here.

---

## 10. Variants

**Short-form video (TikTok, Reels, Shorts).** Clips of 15–60 seconds make the segmented pipeline's payoff
mostly disappear, and the interesting problems move: publish latency becomes seconds because creators
expect immediacy, the feed **prefetches the next several videos** while the current one plays (coupling
playback to ranking), and per-video overhead dominates because the videos are many and tiny.

**Live streaming.** Genuinely different, as §2 said: realtime encode against a deadline rather than a
batch DAG, low-latency HLS or WebRTC, no pre-positioning, and an origin that is a live packager rather
than an object store. The one shared idea is the ladder.

**Podcast and audio platforms.** This chapter with the video removed: two orders of magnitude less data, a
one- or two-rung ladder, no segmented transcode at all, and the same CDN-fronted immutable objects on the
read path. The case where you deliberately *not* build the pipeline — as is enterprise e-learning video,
where Attempt 4 plus a rented CDN is correct and the engineering belongs in authorization.

---

## 11. Further reading

- Chapter 02 §6 for CDN mechanics; Chapter 70 for the view-count pipeline; Chapter 42 for the same
  delivery problem with no upload path
- Huang et al., "SVE: Distributed Video Processing at Facebook Scale," SOSP 2017 — the clearest published
  description of a DAG-based parallel video pipeline, including GOP-aligned chunking
- Ranganathan et al., "Warehouse-scale video acceleration," ASPLOS 2021 — YouTube's video coding units
- Aaron et al., "High Quality Video Encoding at Scale," Netflix Technology Blog, 2015 — chunked encoding
- RFC 8216, "HTTP Live Streaming" (Pantos & May), and ISO/IEC 23009-1 (MPEG-DASH)
- Apple, "HLS Authoring Specification for Apple Devices," and the AWS S3 documentation on multipart
  upload and presigned URLs — concrete ladders, keyframe alignment, and the semantics behind Attempt 3
