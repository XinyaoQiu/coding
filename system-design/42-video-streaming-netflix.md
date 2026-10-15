# Chapter 42 — Video Streaming (Netflix)

> **Prerequisites:** Chapters 02 (CDNs §6, push vs pull, cache keys), 01 (store selection), 40 (bitrate ladders, ABR, GOP-aligned segments — assumed, not repeated)
> **Patterns:** offline optimization with an unlimited time budget, push CDN and pre-positioning, owned infrastructure economics, client-driven adaptation

---

## 1. The problem

A subscriber opens the app, browses rows of artwork, picks a title, and watches it in the best quality
their connection allows, on a phone or a television, anywhere in the world. There are tens of thousands of
titles, and they are the same tens of thousands of titles for everyone.

Chapter 40 covered a platform where anyone can upload. This looks like the same product and is a different
engineering problem, which is exactly why interviewers ask both.

**The property that makes it hard — and easy:** there is no user-generated content. The catalog is small,
known weeks in advance, and immutable once encoded. That single fact deletes the write path and inverts
almost every decision from Chapter 40: encoding time becomes effectively free, so quality per delivered
bit can be optimized exhaustively; demand becomes predictable, so content can be placed before anyone asks
for it; and what remains is pure delivery economics — hundreds of terabits per second of egress, where
every percent of bitrate saved is a very large amount of money.

---

## 2. Requirements

### Functional

1. Browse a personalized home page of rows.
2. Play any title, with seek, multiple audio tracks, and subtitles.
3. Resume across devices ("continue watching"), and download for offline playback.

Defer, but name: search, ratings, profiles and parental controls, billing (Chapter 81), and the
recommendation model itself (Chapter A4) — although §7.5 argues the home page is not a side quest.

### Non-functional

- **Time to first frame** — p99 under 1 second; on a television the perceived quality of the whole product
  is set by this number and by how fast the artwork grid renders.
- **Rebuffer ratio** — under 0.1% of playing time, stricter than Chapter 40's, because a subscriber
  watching a two-hour film tolerates less than someone scrolling free clips.
- **Sustained bitrate** — 4K HDR needs 15–25 Mbps for the length of a film, not as a burst.
- **Playback availability** — survive a cloud region failure, an ISP problem, and a CDN node failure by
  degrading quality rather than stopping.
- **Cost per delivered gigabyte** — first-class, and the requirement that drives §7.1 and §7.2.
- **Licensing correctness** — a title must not be playable in a region or on a date where rights do not
  exist: a hard constraint with legal consequences, enforced by contract rather than by physics.

### Explicitly out of scope

**User upload.** There is none, and saying so early is not a formality: it deletes the entire ingest and
transcode-scheduling architecture of Chapter 40 and is the premise for everything below. **Live events**
are a genuinely different system — no pre-positioning is possible, peak is a spike rather than a
predictable curve, and encoding is realtime against a deadline — so scope them out as Chapter 40 does.
Also out: recommendation internals, DRM cryptography, billing, and content acquisition.

---

## 3. Estimation

Assume 300 million subscribers watching 2 hours per day. **Demand, and the cost of renting it — the
constraining number:**

```
300M × 2 h            = 600M viewing-hours/day
600e6 / 24 h          = 25M concurrent streams average; regional evening peak ≈ 2.5× → ~60M
at 5 Mbps delivered   ≈ 125 Tbps average, ≈ 300 Tbps peak; bytes/day ≈ 1.35 EB = 1.35e9 GB
at $0.002/GB (aggressive committed CDN rate) → $2.7M/day ≈ $1.0B/year
at $0.005/GB (a more ordinary rate)          → $6.7M/day ≈ $2.5B/year
```

A billion dollars a year at the optimistic price, for a service you do not control. **That number is the
entire justification for building your own CDN** (§7.2), and it is why a 20% bitrate reduction from better
encoding (§7.1) is worth more than most companies' total engineering budget.

**The catalog — the enabling number.** Assume 20,000 titles averaging 1.5 hours: 30,000 hours. Summing
every rendition of every codec — an H.264 ladder around 20 Mbps aggregate, VP9/AV1 around 12, HEVC/HDR
around 15, plus a dozen audio tracks and subtitles at ~2 — gives roughly 50 Mbps stored per second of
content.

```
30,000 h × 3,600 s × 50e6/8 ≈ 675 TB ≈ 0.7 PB   entire catalog, all codecs, all languages
top 1,000 titles ≈ 1,500 h            ≈ 34 TB   the popular head
the subset one region actually needs  ≈ 10–15 TB
```

**Less than one petabyte** — against Chapter 40's 1.3 exabytes per *year* of new bytes, a factor of two
thousand. That is what makes everything else possible: one 2U server with a few hundred terabytes holds a
large fraction of everything ever made, and ten terabytes covers what a region actually watches. Hold that
thought until §7.2.

**Encoding compute.** New content arrives at perhaps 1,000 hours per month. Per-title optimization (§7.1)
encodes each title at ~100 trial operating points, and quality-first presets cost roughly 5 core-hours per
content-hour:

```
1,000 h × 100 trials × 5 core-hours = 500,000 core-hours/month ÷ 730 h ≈ 685 cores sustained
```

Seven hundred cores against Chapter 40's sixty thousand. **This is why an unlimited encoding time budget
is affordable**: being wasteful with CPU is far cheaper than being wasteful with bandwidth. Even a full
catalog re-encode for a new codec is ~15 million core-hours — 20,000 cores for a month, a project rather
than an impossibility, which is why a new codec can actually be adopted here.

---

## 4. API

```
GET  /home?profileId=                    # the personalized grid
  ->  200 {rows:[{rowId, title, items:[{titleId, artworkUrl, ...}]}], nextRowCursor}
GET  /titles/{titleId}
  ->  200 {metadata, seasons[], episodes[], availableUntil, maturity}

POST /playback/{videoId}/manifest        # a session, not a static file
  body: {deviceProfile, drmSystem, audioLangs[], subtitleLangs[], startPositionMs}
  ->  200 {manifest, cdnHosts:[ranked], sessionToken, expiresAt}
  ->  403 if not licensed in this region today

POST /drm/license                        # separate call, separate service
  body: {sessionToken, challenge}   ->  200 {license}
POST /playback/{videoId}/telemetry       # bookmark + QoE, batched
  body: {positionMs, bitrate, rebufferMs, cdnHost, errors[]}   ->  204
GET  /continue-watching?profileId=       ->  200 {items:[{titleId, positionMs}]}
```

**There are no upload endpoints.** Content enters through an internal workflow from studio masters on a
schedule. Naming that absence is the fastest way to signal you understand what kind of system this is.

**The manifest is generated per session, not served as a static file.** Four decisions land in it at once:
which renditions this device may play (codec, DRM system, screen size), which are licensed here today,
which CDN hosts to try in what order, and what token authorizes the segments. It is the only personalized
object on the playback path.

**Licensing is therefore enforced at manifest issuance**, not at the CDN, which holds bytes and knows
nothing about rights. A 403 here is a correctness feature. Telemetry, by contrast, is batched and
asynchronous — it carries the bookmark and the QoE measurements that feed §7.2's steering, and must never
sit on the critical path of playback.

---

## 5. Data model

```
titles / episodes                        # a catalog, not a feed: ~20k rows, near-static
  title_id, type, name, synopsis, runtime_ms, maturity, release_date
availability                             # the licensing constraint, in the schema
  title_id, region, starts_at, ends_at    # playable iff a row covers (region, now)
assets                                   # every encoded artifact
  title_id, asset_id                      # "h264_1080p", "av1_2160p_hdr", "audio_de_atmos"
  codec, height, bitrate_bps, language, quality_score   # VMAF at this operating point
  storage_uri, segment_count
bookmarks                                # the only real user write
  profile_id (partition key), title_id, position_ms, updated_at
home_rows                                # precomputed per profile; see §7.5
  profile_id (partition key), row_rank, row_type, title_ids[], computed_at
```

**The catalog is small enough to be uninteresting as a storage problem, and that is the point.** Twenty
thousand rows fits in memory on a laptop; it replicates to every region, caches at every layer, and serves
with a hit rate near one — contrast Chapter 40's metadata table, which grows forever. And **`availability`
is not a detail**: modeling rights as (title, region, window) rows checked at manifest issuance keeps a
legal constraint from becoming scattered special cases, and makes a title vanishing at midnight a data
change rather than a deploy.

---

## 6. Architecture, derived

### Attempt 1: serve segments from object storage in one region

```
Player ──► https://origin.example.com/... ──► object storage (us-east)
```

Two failures. **Throughput**: 300 Tbps at peak is orders of magnitude beyond any single-region origin.
**Latency and loss**: a viewer in Jakarta pulling from Virginia crosses ~200 ms of round trip on a lossy
intercontinental path, so TCP throughput collapses below the 15 Mbps a 4K stream needs no matter how much
capacity the origin has.

### Attempt 2: rent a commercial CDN

This works technically, and for most companies it is the right answer. It fails here on two grounds.

**Cost**: from §3, roughly $1B/year at aggressive rates, growing linearly with subscriber hours. And
**control**: you cannot decide what is cached, when it is filled, or how much of a shared point of
presence is yours the night a flagship season launches. A pull CDN fills on demand, so the first viewers
in every location miss precisely when concurrency is highest — the one moment a miss is most expensive —
and you are one tenant among many on hardware tuned for the average of all of them.

### Attempt 3: build your own CDN at internet exchange points

Own the hardware, place clusters at IXPs, peer directly with ISPs: capital replaces per-gigabyte rent and
you control caching and fill. Better, and still not sufficient — traffic from an IXP still crosses the
ISP's own backbone, which is their expensive resource; during evening peak it congests, and the symptom is
your rebuffer metric. You have moved the bytes closer, not out of the way.

### Attempt 4: put appliances inside ISP networks, and fill them before demand

Give the ISP a server that lives in their data center, behind their peering edge, and serve their
subscribers from it. **Fill it overnight, during their off-peak window, with content predicted to be
watched tomorrow.**

The economics work for both sides, which is what makes it possible: the ISP's transit and backbone costs
drop sharply because the bytes originate inside their network, and your delivery cost approaches the
amortized cost of hardware. It is *feasible* because of §3's enabling number — **the popular regional
working set is around 10 TB, so one machine holds it.** This would be impossible for Chapter 40's catalog,
which is the cleanest illustration of how the no-upload premise reaches all the way down to hardware.

### Attempt 5: attack the bytes themselves

Placement reduces cost per delivered byte; the other lever is delivering fewer bytes at the same perceived
quality. With a small catalog and free encoding time that lever is unusually long: a ladder optimized per
title (§7.1) cuts average bitrate materially, and every percent applies to all 1.35 EB per day, forever.
It is a compute-for-bandwidth trade, and §3 showed compute is two orders of magnitude the cheaper side.

### Final architecture

```
   CONTROL PLANE (cloud, multi-region)           OFFLINE PIPELINE
   ┌──────────────────────────────────────┐   ┌───────────────────────────┐
   │ catalog + availability rules          │   │ studio master (mezzanine) │
   │ home-page service ◄─ precomputed rows ◄───┤ per-title ladder search   │
   │ playback API ── manifest + steering   │   │  (trial encodes + VMAF)   │
   │ DRM license service                   │   │ final encodes → packaging │
   │ telemetry ─► Kafka ─► rec models      │   └───────────┬───────────────┘
   └────────┬─────────────────┬────────────┘               │ assets
            │ manifest        │ license                    ▼
            ▼                 ▼                    ┌────────────────┐
        ┌──────────┐                               │ origin storage │
        │  Player  │                               └───────┬────────┘
        └────┬─────┘                                       │ nightly fill
             │ segments                                    │ (off-peak)
   ┌─────────▼───────────────────────────────────┐         │
   │ OWNED CDN                                    │◄────────┘
   │  appliances inside ISP networks       (95%)  │
   │  IXP clusters (regional fallback)     (~5%)  │
   │  origin (last resort, cold long tail) (<1%)  │
   └─────────────────────────────────────────────┘
```

The two planes have opposite properties, and drawing them separately is the point: the control plane is
elastic, personalized, and small in bytes; the data plane is fixed-capacity, impersonal, and enormous. The
player is the only component talking to both, so a control-plane outage degrades browsing while playback
in flight continues.

### Where this inverts Chapter 40

| Decision | Chapter 40 (upload platform) | Chapter 42 (streaming service) | Why it inverts |
|---|---|---|---|
| Ingest | Presigned chunked upload, 8.3 GB/s | None; studio masters on a schedule | No user-generated content |
| Encode time budget | Minutes, parallelized across a fleet | Hours or days per title, offline | Publish latency is a product requirement there, not here |
| Ladder | Fixed table indexed by source resolution | Optimized per title, even per shot | 500 h/min of arrivals vs 1,000 h/month |
| Expensive codecs | Lazy, popularity-triggered | Eager, for everything | Every title is popular relative to a 30,000-hour catalog |
| Stored bytes | 1.3 EB/year, growing, tiered to archive | ~0.7 PB total, all hot | The catalog is bounded |
| Cache strategy | Pull CDN; long tail misses by design | Push; pre-positioned before demand | Demand is predictable when the catalog is known |
| CDN | Rented, multi-provider | Owned, embedded in ISP networks | Traffic is huge, steady, and concentrated |
| Hard problem | The transcode pipeline | Delivery economics and discovery | Follows from everything above |

The table is worth reproducing from memory: "what changes if there is no upload?" is the follow-up
question this chapter exists to answer.

---

## 7. Deep dives

### 7.1 Per-title encoding

The standard practice Chapter 40 adopts is a **fixed ladder** — 1080p at 4.5 Mbps, 720p at 2.5, applied to
everything — and it is right there, because 500 hours arrive every minute and there is no time to think
about any individual video.

It is also, on any individual title, wrong. A flat-shaded animated series at 1080p can be visually
indistinguishable from its source at 1.5 Mbps, so encoding it at 4.5 wastes two thirds of the bytes on
every stream ever delivered. A dark, grainy, fast-moving action film at 1080p may still show blocking at
8 Mbps, so the same rung ships a bad experience to every viewer with bandwidth for better. **One number
cannot be right for both, and the error runs in both directions at once.**

The method, given free encoding time:

1. Encode the title at many (resolution, bitrate) operating points — on the order of a hundred.
2. Score each with a **perceptual quality metric**, not bitrate and not PSNR; VMAF exists precisely
   because PSNR ranks encodes in an order human viewers disagree with.
3. Take the **convex hull** of quality against bitrate — everything below it is strictly dominated — and
   choose the ladder rungs along that hull at fixed *quality* targets rather than fixed bitrates.

That last step is the conceptual move: **a rung is a quality level, and its bitrate is whatever the title
needs to reach it** — 1.5 Mbps for the animated show's 1080p rung, 7 for the action film's, both looking
equally good.

**Per-shot goes further.** A static dialogue scene and a chase have different rate-quality curves, so
segment at shot boundaries — which are also GOP boundaries, composing with Chapter 40 §7.1 — and optimize
each shot independently. A smaller gain than per-title's, on every byte.

**The costs, argued honestly.** Encoding cost rises by the number of trial points, a hundredfold, which §3
showed is affordable only because the catalog is tiny — the technique is simply unavailable to Chapter 40.
Variable per-rung bitrates complicate the client: ABR algorithms assuming a stable rung bitrate must use
the manifest's declared segment sizes instead. And the pipeline now depends on the quality metric being
right; if it mis-scores a genre you systematically underencode a whole category and no server-side signal
will tell you.

### 7.2 Owning the CDN, and appliances inside ISPs

Three tiers, from §6's diagram:

- **Embedded appliances** inside ISP networks, serving that ISP's subscribers: the overwhelming majority of
  bytes, at fixed capacity, filled overnight.
- **IXP clusters**, peering with many networks and holding a larger slice of the catalog, absorbing what
  the appliances miss and serving ISPs too small for their own hardware.
- **Origin**, in cloud object storage, which should see essentially nothing.

**Why an ISP agrees to host your hardware.** Their subscribers' streaming is a large fraction of their
evening load, and served from an appliance in their facility those bytes never touch transit or the
long-haul backbone. They save real money and their customers see better quality; you get delivery at
hardware cost. That the deal has to be good for *both* parties is the part candidates miss.

**Own versus rent, argued both ways.** Renting gives elasticity, no capital expenditure, immediate global
reach, and someone else's operations team; for almost every company it is correct. Owning wins only when
three conditions hold at once: traffic is enormous (so per-gigabyte rent dominates), demand is predictable
(so fixed capacity is not stranded), and the working set is small (so cheap hardware achieves a high hit
rate). This service satisfies all three; Chapter 40's platform satisfies the first only, which is why it
rents and steers across providers.

**The cost you accept** is real: thousands of machines in facilities you do not control, with no
elasticity. Capacity planning becomes procurement and shipping with a lead time of months, and a forecast
wrong by 30% cannot be fixed by an autoscaler. A design presenting owned infrastructure as strictly better
is not being honest.

**Steering.** Because the manifest is per session (§4), the control plane hands each session a ranked host
list — local appliance, IXP cluster, origin — and telemetry feeds back which hosts deliver well, so
steering is closed-loop: Chapter 40's multi-CDN mechanism pointed at your own hardware.

### 7.3 Pre-positioning: prediction is cheap when the catalog is small

A pull CDN fills on first request, which is wrong here twice over: the miss happens at peak concurrency
(everyone presses play on a new season the same evening), and the appliance's uplink to origin is far
smaller than its downlink to subscribers, so a miss storm saturates exactly the link you were avoiding.

So push. Every night, during the ISP's off-peak window, each appliance downloads what a forecast says its
subscribers will want tomorrow.

**The forecast is easy, and it is worth being precise about why.** With 20,000 titles you compute a
popularity estimate for every title in every region directly — no sampling, no sketching, no approximate
top-K. Yesterday's regional view counts predict today's very well for catalog content, and for new content
the release schedule is known weeks ahead, so a launch is a scheduled fill rather than a prediction at
all. Contrast Chapter 71, where top-K over an unbounded key space is itself the problem; here the key
space is small enough that the problem evaporates.

**Sizing.** From §3 a region's popular working set is 10–15 TB against an appliance holding hundreds of
terabytes, so it holds the head and a deep tail, and hit rates in the high nineties are achievable rather
than aspirational. What it cannot hold is every codec, language, and rung, so the fill algorithm chooses
*assets*, not titles: the 1080p H.264 and AV1 rungs with local-language audio for a popular title, nothing
for a title with no local audience. **When it misses**, the appliance fetches from the IXP tier rather
than origin and caches the result — a latency and cost event, not a failure.

### 7.4 Client-side ABR and buffer management

The player picks a rung per segment. Three families of algorithm:

**Rate-based.** Estimate throughput from recent segment downloads and pick the highest rung below it.
Simple, and it oscillates badly: throughput estimates are noisy, competing home-network traffic appears
and vanishes, and the estimate is biased when the player idles between requests in steady state.

**Buffer-based.** Ignore throughput; choose on how many seconds are buffered. Buffer occupancy already
integrates throughput over time and is directly the thing you are protecting, so it is stable and needs no
bandwidth estimator — but at startup the buffer is empty, making a pure buffer rule maximally conservative
exactly when the viewer is judging the product.

**Hybrid.** Rate-based during startup, buffer-based in steady state, with hysteresis so a rung change must
clear a threshold rather than cross a line. This is what deployed players do, and it is the answer.

**The asymmetry that governs all of it:** a rebuffer costs far more subjective quality than a lower
resolution does — viewers report a stall as a broken product and a soft image as a mediocre connection. So
the algorithm is not "maximize bitrate subject to not stalling" but "avoid stalling, then maximize
bitrate": upshift reluctantly, downshift eagerly. The algorithm choice follows from that asymmetry.

**Startup deserves its own treatment**: time to first frame is a p99 requirement, so the player takes the
lowest sensible rung for a segment or two, renders, and ramps as the buffer fills. A second of a soft
image is invisible; three seconds of a spinner is not.

### 7.5 The home page is a recommendation problem

With 20,000 titles and a subscriber who wants to start watching in under a minute, **discovery is the
product**. The home page is not a list of content; it is a two-dimensional ranking problem — which rows,
in what order, with which titles inside each, and even which artwork per title per viewer.

The engineering constraint is latency: forty rows, each needing model inference over thousands of
candidates, cannot be computed inside a page load. So the pipeline splits as Chapter 22's ranked feed
does:

- **Offline**, per profile: candidate generation and scoring produce the row set, written to `home_rows`.
- **Online**: read those rows, then adjust the top of the page with a few fresh signals —
  continue-watching, new releases, and what this profile did in the last few minutes.

The full treatment is Chapter A4; what belongs here is the boundary. Note that this is where the only real
personalization write load lives — the telemetry stream of §4 feeds it. **The catalog is impersonal and
cacheable; the home page is personal and precomputed, and keeping those separate is what lets the delivery
plane stay dumb.**

### 7.6 Licensing windows and DRM

**Availability is a first-class constraint.** Rights are granted per region for a window, so a title is
playable in one country, absent in another, and gone on a date. Enforce it at manifest issuance (§4): that
is the one place knowing the viewer's region and the current time, and the CDN — where an attacker would
look for an end run — holds nothing but encrypted bytes and has no notion of who may play them.

**DRM sits on the playback path and must be as available as the CDN.** The player obtains a license before
the first frame, so if the license service is down nothing plays even though every segment is cached
locally. That makes it a first-order availability dependency: regional redundancy, client-side license
caching for the session, and license pre-fetch in parallel with the manifest rather than after it.
Downloads extend the same machinery with a device-stored license that must expire even if the device never
reconnects, and should be shaped away from peak hours since they are not latency-sensitive.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Appliance offline in one ISP | That ISP's subscribers fall back to the IXP tier | Steering demotes the host on telemetry; IXP headroom is planned for exactly this |
| Fill window missed before a launch | Launch night served from IXP/origin at peak — the worst case | Alert on fill completion per appliance per title; start flagship fills days early; throttle non-critical fills |
| ISP peering congestion | Rebuffers concentrated in one network | Detect via per-ISP QoE telemetry, not server metrics; shift steering; reduce ladder ceiling for that network |
| DRM license service down | **Nothing plays**, even fully cached content | Multi-region, aggressive client license caching, license pre-fetch in parallel with the manifest |
| Control-plane region failure | Browsing and new sessions fail; in-flight playback continues | Regional failover for the control plane; the split in §6 is what makes in-flight playback survive |
| Bad encode shipped | Systematic quality loss on one title, invisible server-side | VMAF gates in the pipeline; per-title client QoE; withdraw a rung from the manifest without re-encoding |
| Global release moment | Concurrency spike concentrated in hours | Pre-positioned by construction; the fill, not the serve, is what must have happened |

**Monitoring:** rebuffer ratio and time-to-first-frame sliced by ISP, device, and title — the slice is the
signal, since a global average hides every real incident; appliance hit ratio and fill completion before
each launch; delivered bitrate versus the ladder, which is how you learn §7.1 shipped something wrong;
license service latency; and origin egress bytes, which should be a rounding error and whose growth is the
leading indicator that pre-positioning is degrading.

---

## 9. Common mistakes

1. **Designing an upload path.** There is none, and building Chapter 40's ingest here wastes the time in
   which you were supposed to notice what actually changes.
2. **Using one fixed bitrate ladder.** Defensible when content arrives faster than you can think about it;
   indefensible when the catalog is 30,000 hours and encoding time is free.
3. **Renting a CDN without doing the arithmetic** — or proposing to build one for a service with a
   thousandth of this traffic. It is a crossover, and the interesting answer names the conditions (huge
   traffic, predictable demand, small working set) rather than the conclusion.
4. **Assuming a pull CDN is fine.** It fills at peak concurrency, exactly when you cannot afford misses;
   a known catalog licenses a push model.
5. **Putting ABR decisions on the server.** Only the client knows its buffer occupancy and measured
   throughput, and both change second to second.
6. **Optimizing for bitrate or PSNR instead of perceived quality.** The per-title argument collapses
   without a perceptual metric, because the ladder is chosen by quality, not by bytes.
7. **Ignoring licensing windows**, the one requirement here with legal rather than technical consequences.
8. **Treating the home page as an afterthought.** With a small catalog, discovery is the product surface,
   and its latency budget is stricter than playback's.

---

## 10. Variants

**Music streaming (Spotify).** Two orders of magnitude fewer bytes — a 256 kbps track for four minutes is
under 10 MB — so delivery is trivially cacheable and per-title encoding is not worth the trouble. What
gets harder is the catalog: tens of millions of tracks breaks the "everything fits in one appliance"
property and pushes the work into playlists, personalization, and offline sync.

**Smaller streaming services.** Same product, a tenth of the traffic. The arithmetic in §3 no longer
supports owning a CDN, so they rent from several providers with steering, and the correct design is
Attempt 2 rather than Attempt 4. Saying *at what traffic level the answer flips* is the point of having
derived it.

**Live sports.** Chapter 40's live carve-out at this chapter's scale, minus every advantage: no
pre-positioning, peak is a spike, encoding is realtime, and latency versus a viewer's phone notification
becomes a hard requirement. The hardest version of the problem, sharing only the player.

---

## 11. Further reading

- Chapter 40 for the encode pipeline assumed here; Chapter A4 for the recommender behind §7.5; Chapter 02
  §6 for push versus pull CDNs
- Aaron et al., "Per-Title Encode Optimization," Netflix Technology Blog, 2015 — the convex-hull method of
  §7.1; and "Optimized shot-based encodes," 2018, for the per-shot extension
- Li et al., "Toward A Practical Perceptual Video Quality Metric" (VMAF), Netflix Technology Blog, 2016
- Huang et al., "A Buffer-Based Approach to Rate Adaptation," SIGCOMM 2014 — the argument of §7.4; with
  Yin et al., SIGCOMM 2015, and Spiteri et al., "BOLA," INFOCOM 2016, for the formal versions
- Netflix Open Connect documentation (openconnect.netflix.com) — the appliance program of §7.2
- Gomez-Uribe and Hunt, "The Netflix Recommender System," ACM TMIS 2015
