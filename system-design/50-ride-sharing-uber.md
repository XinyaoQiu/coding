# Chapter 50 — Ride Sharing (Uber)

> **Prerequisites:** Chapters 01 (partitioning), 02 (WebSocket), 03 (logs, event time), 04 §4 (geospatial indexing), 04 §2 (locks and fencing)
> **Patterns:** write-dominated ingest, in-memory derived state, hexagonal spatial index, two-stage guard (fast path + authoritative constraint)

---

## 1. The problem

A rider opens the app, sees nearby cars, and requests a ride. A driver close by receives an offer,
accepts, drives to the pickup point, and takes the rider to the destination. Both parties watch the other
move on a map the whole time. This arrives dressed as Uber, Lyft, DoorDash, or a field-service dispatch
system; the costume changes the vocabulary and nothing else. Almost everyone walks in prepared to discuss
the geospatial index, which is the easiest part of the problem and is essentially solved by Chapter 04 §4.
What distinguishes a good answer is noticing what the traffic looks like.

**The property that makes it hard:** this system is **write-dominated**, which is nearly unique among the
systems in this book. Ten million drivers reporting position every few seconds produce roughly two million
writes per second; ride requests arrive at a few hundred per second. The ratio runs about a thousand to
one in the *opposite* direction from every read-heavy system in Chapters 10 and 20 — so every instinct
those chapters trained (cache the hot object, precompute the read, let the write path be slow) is wrong
here, because the expensive thing is the write and the data it writes is obsolete five seconds later. Say
that out loud in the first two minutes; it reframes everything after it.

---

## 2. Requirements

### Functional

1. A driver's app publishes its current location continuously while the driver is online.
2. A rider requests a ride and receives a fare estimate and an ETA.
3. The system matches the request to a nearby available driver, who accepts or declines.
4. Both parties see the other's live position for the trip, which then completes, is priced, and recorded.

Defer, but name: payments (Chapter 81), driver onboarding, pooled rides, ratings, and the routing engine
itself — that last because road-graph routing is a separate specialty consumed as a service (§7.7).

### Non-functional

- **Location ingest** — absorb ~2 × 10^6 writes/sec sustained, peaking in a handful of metros at their own
  local rush hours. This is the requirement the architecture is really for.
- **Matching latency** — request to first offer under 2 s at p99. The rider is watching a spinner.
- **Location freshness** — a few seconds of staleness is acceptable. State this explicitly: it licenses
  keeping location in a volatile store with no durability guarantee.
- **No double-booking** — the *only* place in the system needing a hard consistency guarantee. Trip state,
  by contrast, must be **durable**: losing a ping is free, losing a trip is a support incident.
- **Availability** — degraded matching (a longer wait, a farther driver) beats no matching.
- **Geographic locality** — a ride in São Paulo involves no data in Berlin. The workload partitions by
  space almost perfectly, and the design should exploit that rather than pretend the problem is global.

### Explicitly out of scope

Fraud detection, driver pay computation, insurance and regulatory reporting, and the payment rails.

---

## 3. Estimation

Assume 10 million drivers online at peak and 20 million rides per day.

**Location writes — the constraining number**

```
10,000,000 online drivers, one update every 5 s
10e6 / 5  =  2,000,000 location writes/sec
```

Nothing else here is within three orders of magnitude. Hold it against the demand side:

```
20e6 rides/day / 86,400  ≈   230 requests/sec average; peak (3×) ≈ 700/sec
~3 offers per request    ≈ 2,100 offer writes/sec at peak
2,000,000 ÷ 700          ≈ 2,900 : 1  writes to matching queries
```

The matching query — the thing the product is for — is a rounding error in the traffic. Everything
expensive is maintenance of an index that mostly nobody reads. **What durable persistence would cost:**

```
per ping: driver_id 8 B + lat/lng 16 + ts 8 + heading/speed/accuracy 8   ≈ 40 B
with row and index overhead in an OLTP store                             ≈ 64 B
2e6/sec × 64 B  =  128 MB/sec  =  11.1 TB/day  =  4.0 PB/year
```

And that is before amplification: LSM compaction rewrites it repeatedly and a B-tree with a spatial index
pays several random page writes per update. Four petabytes a year of data with a five-second useful life —
the whole argument for §7.1. Against that, the state actually needed:

```
per driver in memory: id 8 + position 16 + h3 index 8 + heading/speed 8
                      + state 1 + version 8 + timestamps 8 + overhead  ≈ 100 B
10e6 × 100 B = 1.0 GB    +    cell membership @ ~24 B  ≈  0.24 GB
```

**The live position of every driver on Earth is about one gigabyte** — the second most important number
here. It fits in one machine's RAM, and once sharded for throughput it fits many times over. The problem
was never the volume of data; it was the rate of change.

**Concurrent trips, and why tracking is cheap**

```
20e6 rides/day × 15 min = 20e6 × 0.25 h / 24 h ≈ 208,000 concurrent trips
208,000 × 1 update / 4 s                       ≈  52,000 pushes/sec ≈ 2 MB/sec
```

Active trips are **2%** of the fleet, so the live position stream — the feature that sounds expensive — is
2.6% of ingest. Ten million persistent connections at 50,000 per gateway node is 200 nodes at 10,000
messages/sec each: ordinary.

---

## 4. API

```
WS   /driver/stream                                    # persistent, bidirectional
     client → {type:"location", lat, lng, heading, speed, accuracy, ts, seq}
     server → {type:"offer", offerId, rideId, pickup, dropoff, fare, expiresAt}
     server → {type:"config", updateIntervalMs}        # adaptive frequency, §7.3

POST /rides/estimate {pickup, dropoff, productType}
     -> 200 {estimateId, fareRange, etaSeconds, surgeMultiplier}
POST /rides {pickup, dropoff, productType, estimateId}   Idempotency-Key: <uuid>
     -> 202 {rideId, state:"matching"}
POST /offers/{offerId}/accept  -> 200 {rideId, pickup, riderName} | 409 taken/expired
WS   /rides/{rideId}/stream                            # rider watches the driver move
GET  /rides/{rideId}   ·   POST /rides/{rideId}/cancel
```

**Location goes over a persistent connection, not `POST /location`.** At one request per five seconds per
driver, HTTP means two million TLS request/response cycles per second, each with headers larger than the
payload. A WebSocket (Chapter 02 §8) amortizes the handshake over the session and reduces each update to a
few dozen bytes — one of the rare cases where transport is forced by arithmetic, not taste.

**`POST /rides` returns 202, not 201.** Matching may involve several sequential offers as drivers decline;
the request enqueues an intent and the outcome arrives over the rider's stream, where a synchronous call
would couple an HTTP timeout to human reaction time. Correspondingly the driver receives an *offer*, not
an assignment, and `accept` can legitimately return 409 — designing the API so the failure is expressible
is what makes §7.4 implementable.

**Every ping carries a client sequence number and its own timestamp.** Mobile networks reorder and replay;
without one the server cannot distinguish a delayed ping from a new one and the driver visibly jumps
backwards on the rider's map. Discard any ping whose sequence is below the last seen for that driver — a
per-driver fencing token, exactly Chapter 04 §2.

---

## 5. Data model

Three stores with sharply different requirements; presenting them as one storage layer is the most common
structural mistake in this question. **Live driver state — in memory, sharded by geography, no durability:**

```
driver_state[driver_id] = { lat, lng float64 · h3_r8 uint64 · heading, speed float32
    state enum         # offline|available|offered|assigned|on_trip
    state_version uint64    # monotonic, for the conditional transition (§7.4)
    last_seen_at timestamp · current_ride_id uuid|null }
cell_index[h3_r8] = set<driver_id>          # available drivers only
```

Overwritten in place on every ping: no history, no append. A ping is `O(1)` — write the struct, and if the
cell changed, move the driver between sets. Usually the cell does not change, since at 5-second intervals
and urban speeds a driver crosses a resolution-8 cell (~460 m across) only every 30–60 seconds, so the
membership update happens on roughly one ping in ten. Only `available` drivers are indexed, which shrinks
candidate sets and removes a class of filtering from the query path.

**Trips and offers — durable, relational, sharded by city**

```
rides   ride_id UUID PK · rider_id · driver_id NULL · state ENUM · fare_cents NULL
        pickup, dropoff POINT · requested_at/assigned_at/started_at/ended_at · city_id
offers  offer_id UUID PK · ride_id · driver_id · expires_at
        outcome ENUM   # pending|accepted|declined|expired|superseded
```

A few thousand writes per second globally: a small, ordinary transactional workload belonging in an
ordinary relational database. Resist putting it in the same exotic store as the location data; they share
nothing but the word "driver." **Location history**, finally, goes to Kafka (Chapter 03 §2) partitioned by
`driver_id` so one trajectory stays ordered on one partition, then is compacted into columnar files for
analytics, ETA training, disputes, and retention — the only place raw pings are durable, and deliberately
off the serving path. Sizing in §7.1.

---

## 6. Architecture, derived

### Attempt 1: a table with a spatial index

```sql
UPDATE drivers SET lat=?, lng=?, updated_at=now() WHERE driver_id=?;   -- every ping
SELECT driver_id FROM drivers
 WHERE state='available' AND ST_DWithin(location, ST_Point(?,?), 3000)
 ORDER BY location <-> ST_Point(?,?) LIMIT 20;                          -- matching
```

Correct, and fine for a demo city. **It breaks at roughly 20,000 writes per second per node** — a generous
figure for updates that each maintain a GiST index over a moving point, since the index entry is deleted
and reinserted every time and the page churn defeats the buffer pool. Two million writes per second
therefore needs on the order of a hundred database nodes doing nothing but rewriting a spatial index,
while producing §3's 4 PB/year; the read side would use about 1% of that cluster. Not a scaling problem to
solve with more nodes — it is paying transactional storage costs for data worthless within seconds.

### Attempt 2: keep current location in memory, overwrite in place

Drop the durable write. Keep a hash map from `driver_id` to current position and overwrite on every ping —
no insert, no append, no index maintenance beyond set membership, no disk. The dataset is 1.3 GB (§3), and
two million writes/sec across a few dozen sharded processes is 50,000–100,000 ops/sec each: unremarkable.

**What this gives up is history**, which has real consumers: disputes, ETA training, analytics, insurance.
So the raw ping is *also* published to Kafka and the historical path served from there (§7.1) — the move
Chapter 10 §7.3 makes for click analytics, except here it is worth 4 PB a year. **What it does not yet
give** is a proximity query: a map keyed by driver ID cannot answer "who is near this point."

### Attempt 3: bucket by H3 cell

Apply Chapter 04 §4. On write, compute the driver's H3 index and maintain `cell → set of available
drivers`; a query computes the origin cell, takes its k-ring, unions the sets, and filters by distance.

```
find_nearby(lat, lng, k):
    cells = h3.grid_disk(h3.latlng_to_cell(lat, lng, RES), k)   # 1+3k(k+1) cells
    return [d for d in union(cell_index[c] for c in cells)
              if haversine(d, (lat,lng)) <= radius]
```

Resolution 8 cells average ~0.74 km², so k = 1 covers 7 cells ≈ 5.2 km², an effective radius of ~1.3 km.
In a dense city with 8,000 online drivers over 250 km² — 32 drivers/km² — one cell holds ~24 drivers and
k = 1 returns ~165 candidates before distance filtering: enough to rank meaningfully, cheap enough to
rank. §7.2 covers resolution choice and the suburbs, where the same query returns two drivers.

### Attempt 4: shard by geography, and make assignment conditional

Location data partitions almost perfectly by space: a rider in one city is never matched to a driver in
another. Assign each shard a set of coarse H3 cells (resolution 4, ~1,770 km², or 5, ~253 km²) and route
both pings and queries by the cell of the coordinate. A proximity query becomes single-shard in the
overwhelming majority of cases — a scatter-gather would put the slowest shard's tail into every match —
and load follows the sun, so capacity is sized regionally. Boundaries are §7.6.

Nothing so far prevents two concurrent requests from selecting the same driver: two riders, one car. The
fix has two layers, and the interesting part is which is load-bearing — a **short-TTL lock** that makes
conflicts rare, and an **atomic conditional transition** (`available → offered → assigned`) that makes
them impossible, the two-stage guard of Chapter 04 §2, argued in §7.4.

### Final architecture

```
 10M drivers   ┌──────────────────────────────────────────────────┐
 (WebSocket) ──► Location service (sharded by coarse H3 cell)     │
      │        │   driver_state[id]  — overwrite in place         │
      │        │   cell_index[h3_r8] — set<available driver_id>   │
      │        └────────┬───────────────────────────▲─────────────┘
      │                 │ raw pings                 │ find_nearby(k-ring)
      │                 ▼                           │
      │          Kafka: driver_pings (by driver_id) │
      │      ┌──────────┼────────────────┐          │
      │      ▼          ▼                ▼          │
      │ Trip tracker  Surge aggregator  Columnar sink (Parquet)
      │      │        (windowed/cell, Ch 70)        │
      │      ▼          │                           │
      │ Rider streams   └── surge[cell] ──┐         │
Rider ┴─► Gateway ─► Matching service ◄───┴─────────┘
                       │ ├─► ETA service (road graph, contraction hierarchies)
                       │ └─► Redis lock:driver:{id}  (short TTL — optimization)
                       ▼
               Trip store (relational, by city) — durable, authoritative
```

Each boundary is drawn by a different requirement: location because 2M writes/sec needs memory and
geographic sharding; the trip store because trip state must be durable and transactional; Kafka because
history is valuable but must not be on the serving path; ETA because road-graph routing is a different
specialty with a different scaling profile; matching because it is the only thing that needs all four.

---

## 7. Deep dives

### 7.1 Do not durably persist every ping

The naive instinct is that a location update is a write and writes go to a database. Follow it and you own
128 MB/sec, 11 TB/day, 4 PB/year (§3) with a five-second useful life. Three observations dismantle it.

**The serving path needs exactly one value per driver.** "Where is driver 12345 right now" is answered by
the latest ping; every earlier one is dead weight. So the serving representation is a mutable cell, not a
log — storage is `O(drivers)` rather than `O(drivers × time)`, one gigabyte instead of four petabytes a
year, a factor of about 10^6.

**Freshness tolerance licenses volatility.** Because a few seconds of staleness is acceptable (§2), losing
the entire in-memory index costs at most one update interval: every online driver re-reports within five
seconds and it rebuilds itself. This is the strongest form of Chapter 20 §7.5's "derived cache" argument —
the source is not a database that must be scanned, it is ten million clients about to tell you again
anyway. **A store continuously overwritten by its producers does not need durability.**

**History has real consumers, none of them latency-sensitive.** Disputes, ETA training, analytics, and
retention tolerate minutes or hours, so publish the raw ping to Kafka and let that path batch freely:

```
Kafka:     128 MB/sec raw; batched + compressed ≈ 40 MB/sec on the wire
           × 3 replicas ≈ 120 MB/sec network; 7-day retention ≈ 25 TB
Columnar:  trajectories delta-encode well — successive positions differ by metres,
           timestamps by near-constant intervals ≈ 8–10 B/ping ≈ 1.7 TB/day
```

1.7 TB/day of Parquet is an ordinary data-lake bill; 11 TB/day of indexed OLTP rows is not, and the gap is
entirely a consequence of matching the storage shape to a workload that is append-only, immutable, read in
bulk, and never read by primary key. **The cost:** a ping lives only in memory until the Kafka publish, so
a process loss drops a second or two of history — irrelevant for analytics, but if the trajectory is
legally load-bearing, publish first and update memory from the consumer instead.

### 7.2 The geospatial index: applying H3

Chapter 04 §4 establishes what H3 is — hexagons across 16 resolutions, each cell about one-seventh the
area of the next coarser one, addressed by a single 64-bit integer. Three consequences make it right for
*this* workload rather than merely fashionable.

**Bucketing is free.** The write path computes `h3.latlng_to_cell` — fixed-cost arithmetic, no tree
descent, no rebalancing — and uses the result as a hash key, which matters when it runs two million times
a second. A quadtree requires a descent and, worse, structural mutation as points move between nodes,
meaning locks under a workload that is nothing but movement. The hexagonal grid is *static*: cells never
split, merge, or rebalance, so the only mutable state is set membership. **The index is fixed and the data
moves through it** — the property that makes 2M writes/sec tractable.

**Ring searches are uniform.** Every hexagon has six neighbors, all equidistant from its center. A square
grid has four edge neighbors at distance `s` and four corner neighbors at `s√2`, so a k-ring is not a disk
but a square whose corners reach 41% farther than its edges — a distortion that appears directly as
candidate sets biased along the diagonals. With hexagons `grid_disk(origin, k)` genuinely approximates a
disk, so increasing k is a clean isotropic expansion of the radius, exactly what this query wants.

**Resolution has an arithmetic answer.** Pick it so a typical query needs one or two rings:

| Resolution | Avg cell area | k=1 (7 cells) | k=2 (19 cells) | Candidates at 32/km², k=1 |
|---|---|---|---|---|
| 7 | ~5.16 km² | 36 km² (r ≈ 3.4 km) | 98 km² | ~1,150 — too many |
| **8** | **~0.74 km²** | **5.2 km² (r ≈ 1.3 km)** | **14 km² (r ≈ 2.1 km)** | **~165** |
| 9 | ~0.105 km² | 0.74 km² (r ≈ 0.5 km) | 2.0 km² | ~24 — too few |

Resolution 8 is the right urban default, but density varies by three orders of magnitude between a
downtown core and a rural highway: at 0.5 drivers/km², k = 1 returns 2.6 candidates and only k = 3 (37
cells, 27 km²) reaches ~14. So **k expands adaptively** — start at 1, increment while fewer than N
candidates survive the distance filter, stop at the maximum tolerable pickup distance. Expansion is cheap
because each ring is a handful of hash lookups against sets that are small in exactly the places expansion
is needed. **The alternative worth arguing** is a geohash index in Redis (`GEOADD`/`GEOSEARCH`): no extra
library and simpler, at the cost of Chapter 04 §4's two flaws plus a write that is a `ZADD` into a shared
sorted set — `O(log n)`, mutating shared structure — rather than an `O(1)` membership change, which at 2M
writes/sec is not academic. Choose H3 when movement dominates, geohash when the index must live in a
database you already have (Chapter 51).

### 7.3 Adaptive update frequency

A fixed five-second interval treats a driver parked at an airport identically to one merging onto a
freeway — the polling mistake, sampling at the worst case's rate for everyone. Let the server dictate the
interval per driver, pushed down the existing WebSocket as a `config` message:

| Driver state | Fleet | Interval | Writes/sec |
|---|---|---|---|
| On an active trip | 2.0 M | 4 s | 500,000 |
| Available and moving | 3.0 M | 5 s | 600,000 |
| Available, stationary > 60 s | 5.0 M | 30 s | 167,000 |
| **Total** | **10 M** | | **~1,267,000 — a 37% reduction** |

Nearly forty percent of the constraining number removed by noticing that half the fleet is not moving. The
saving compounds: the ingest fleet shrinks, Kafka volume drops by the same fraction, the columnar sink
falls from 1.7 to 1.1 TB/day, and the phone's GPS and radio wake a sixth as often — which matters to
someone whose battery must survive a ten-hour shift.

Three refinements make it robust. **The client may report early**: the server sets an upper bound, and the
client reports immediately on moving past a distance threshold, which keeps a 30-second interval from
becoming a 30-second blind spot. **Stationarity is measured, not assumed**: the threshold must exceed the
GPS noise floor (~10 m) or urban-canyon jitter silently disables the optimization in exactly the dense
areas where it helps most. **The interval is a hint the server can withdraw**: during a demand spike it
tightens intervals in that cell. The principle is that **update frequency should be proportional to the
value of the update**, and that value is not uniform across the fleet.

**The cost:** a stationary driver's position can be 30 s stale, so one who pulls away unnoticed may be
offered a ride from a position they have left — but the offer is a round trip, so the acceptance carries a
fresh position to re-validate against, degrading to a worse match rather than a wrong one.

### 7.4 Matching without double-booking

At peak, 700 requests per second select from candidate pools that overlap heavily, because supply and
demand concentrate in the same few cells. Two requests a millisecond apart downtown frequently pick the
same top-ranked driver, and the naive implementation contains the classic race:

```
best = rank(find_nearby(pickup))[0]
if best.state == "available":         # read
    best.state = "offered"            # write — another matcher interleaves here
    send_offer(best)
```

Both matchers read `available`, both write `offered`, both riders are told a car is coming. This is
Chapter 80's problem with drivers in place of seats: **a scarce resource, concurrent claimants, and a
check separated from the act.**

**Layer 1 — a short-TTL lock, which is an optimization.** Before offering, `SET lock:driver:{id} NX PX
15000` for roughly the offer window; concurrent matchers fail to acquire and move on, wasting neither an
offer nor the driver's attention. Without it, in a hot cell a meaningful fraction of offers are collisions
and the median rider waits an extra round. But it guarantees nothing — a matcher paused past its TTL by a
GC stall or a partition wakes believing it still holds the lock (Chapter 04 §2).

**Layer 2 — an atomic conditional transition, which is the guarantee.**

```sql
UPDATE drivers
   SET state='assigned', current_ride_id=:ride, state_version=state_version+1
 WHERE driver_id=:driver AND state='offered'
   AND current_ride_id=:ride AND state_version=:expected_version;
-- assigned if and only if rows_affected = 1
```

One row, one partition, one atomic operation. The `state_version` predicate is a fencing token: a delayed
writer carrying a stale version is refused *by the resource itself*, the only place a refusal can be
trustworthy. Zero rows means someone else won — return 409 and move on. There is no window in which two
rides both see `rows_affected = 1`, because the database serializes writes to a single row.

**The relationship between the layers is the answer.** The lock makes conflicts rare; the conditional
write makes them impossible. Delete the lock and the system is correct and slower; delete the conditional
write and it is fast and wrong. Presenting the lock as the mechanism that prevents double-booking inverts
which layer is load-bearing — the single most reliable discriminator in this question.

**Where that state lives is a real trade-off.** In memory the transition is fast and local, but the
guarantee then sits in a store with no durability and a restart loses assignments; in the trip database it
is durable at the cost of a round trip, affordable at 2,100 offers/sec. **Put it in the durable store** —
the rate that forced everything into memory is the *location* rate, not the *assignment* rate, and
conflating them is how designs end up with their correctness guarantee in a cache.

### 7.5 The driver state machine

The index gets the attention; this is where the bugs are. A driver's lifecycle is a state machine with
concurrent inputs from three independent sources — the driver's app, the rider's app, and server-side
timeouts — and every transition must be legal, idempotent, and total.

```
            go online                    decline / expire / rider cancels
  offline ─────────────► available ◄──────────────────┐   ◄──────────────┐
     ▲                       │                        │                  │
     │ offline / conn lost   └──► offered ────────────┘                  │
     │ (with grace, §7.7)            │                                   │
     └───────────────────────────────┤ accept (conditional write)        │
                                     ▼                    trip completed │
                                 assigned ──► arrived ──► on_trip ───────┘
                                     └────────────┴─────────┘ cancellation paths
```

**Every transition is guarded by the expected source state** — not "set state to assigned" but "set state
to assigned *if it is currently offered*." That turns every concurrent-input race into a losing update
returning zero rows rather than a corruption, and makes retries free: a duplicated accept, inevitable on a
mobile network, finds the state already `assigned` with the same ride and returns success (Chapter 03 §4).

**Every state that can be entered has a timeout out of it.** `offered` expires after ~15 s back to
`available`; `assigned` without an `arrived` escalates; `arrived` without `on_trip` eventually cancels
with a no-show fee. Without these, one dropped message strands a driver in a state the system's own inputs
can never leave, silently removing them from supply — a leak that surfaces weeks later as inexplicably
shrinking supply in one city.

**Cancellation is legal from more states than feels comfortable, and each differs**: free in `matching`,
fee-bearing in `assigned`, and in `on_trip` not a cancellation at all but an early completion with a
partial fare. Modeling it as one transition is a frequent source of revenue bugs and support tickets.

**The claim worth making out loud:** correctness here matters more than the choice of geospatial index. A
suboptimal index makes matches slightly worse; an illegal transition double-books a car, strands a driver,
or bills a rider for a trip that did not happen.

### 7.6 Geographic sharding, and the city that straddles a boundary

**A query near a shard boundary** has part of its k-ring on the neighboring shard. Query both and merge:
exact, cheap in proportion to how rare the case is, and unlike replicating a boundary band it introduces
no second copy of mutable state. Detect it by checking whether `grid_disk(origin, k)` spans two shards.

**The city that spans a boundary** is where naive geographic sharding fails badly. A metropolitan area
does not respect a hexagonal grid: draw resolution-4 cells over any large city and the densest,
highest-demand square kilometres sit near a cell edge often enough to matter. One shard then holds a
downtown core at 32 drivers/km² while its neighbor holds farmland, so uniform assignment sizes the fleet
for the worst shard and idles the rest; that busiest shard is also the one whose loss removes the most
supply, and it fails at rush hour. Worst, boundary queries stop being rare — if the boundary runs through
downtown, a large fraction of that city's queries are two-shard, and a small cost on rare events becomes a
permanent doubling of the busiest workload.

The fix is that **shard assignment must be by measured load, not by area, and must keep metros intact.**
Define shards over a *set* of coarse cells rather than a contiguous region, assign cells by observed ping
rate, and constrain the assignment so all cells within a metropolitan boundary land in one shard group. A
metro that outgrows a shard is split internally at finer resolution along a chosen low-traffic line — a
river, a rail corridor — not wherever the grid falls. **The cost:** that assignment is dynamic state every
gateway must agree on, and a stale copy routes pings where nobody queries, silently reducing supply.
Version the mapping, include the version in routing, and alert on gateway skew.

### 7.7 The active trip: live position, lost signal, ETA, and surge

**The live stream is affordable because it is scoped.** Only ~2% of the fleet is on a trip (§3), so
forwarding the assigned driver's position to the matched rider costs ~52,000 messages/sec. The scoping
*is* the optimization: streaming every driver's position to every nearby rider — what "show cars moving on
the map" naively suggests — would be a broadcast fanout problem of Chapter 31's kind, avoided by noticing
that the pre-match map does not need real positions.

**A driver's phone loses signal mid-trip** — a tunnel, a garage, a rural gap — daily, not exceptionally:

- **Do not change trip state on disconnection.** The trip lives in the durable store and its state machine
  is driven by explicit events, not connection liveness. A dropped socket pauses the position stream; it
  does not end the trip. Conflating transport liveness with business state is the mistake Chapter 33 warns
  about for presence.
- **Buffer on the client, replay on reconnect,** with original timestamps and sequence numbers; the server
  reconstructs the trajectory in event time (Chapter 03 §7), so distance and fare are correct despite late
  arrival, and the sequence numbers make reordered messages trivially deduplicable.
- **Tell the rider something true.** After ~15 s show "reconnecting" rather than a car frozen at a stale
  position, which reads as a hung app; dead-reckon briefly at low confidence, then stop.
- **Grace before removing supply.** An `available` driver who disappears leaves `cell_index` after 30–60 s
  — long enough to survive a tunnel, short enough that phantom supply does not accumulate. An `on_trip`
  driver who disappears is removed from nothing.

**ETA is a separate service, and naming it as one is part of the answer.** Straight-line distance is not a
usable proxy for arrival time: rivers, one-way streets, and highway access routinely make the nearest
driver by Euclidean distance the third-fastest by road. Ranking by predicted ETA means routing over a road
graph — contraction hierarchies plus a live traffic model — and the load is real: matching at 700/sec ×
~20 candidates is 14,000 calls/sec, tracking 208,000 trips every 30 s adds 7,000, and fare estimates
another 4,000, for ~25,000/sec. That is its own system with its own map-data pipeline and release cadence.
Behind an interface, matching falls back to straight-line distance when it is degraded — worse matches,
working product, which is §2's availability posture. It also caches well: cell-pair ETAs (`h3_r8 origin →
h3_r8 destination`, bucketed by time of day) absorb most of the matching load, since 20 candidates usually
occupy a handful of cells.

**Surge pricing is a windowed aggregation, not a pricing engine.** Per cell and per short tumbling window,
count demand (requests, and app-opens with a pickup in the cell) and supply (distinct available drivers),
and derive a multiplier from the ratio — Chapter 70's machinery unchanged, keyed by cell. Three
application-specific points. **Smooth the output** across adjacent windows and cells: a one-minute window
over a cell with three drivers yields a multiplier flickering between 1.0 and 2.4, which riders experience
as dishonesty. **The surge cell is coarser than the matching cell** — resolution 8 has too few
observations per window, so aggregate at 6 (~36 km²) or 7. **A quoted multiplier must be honored**: pin it
to the `estimateId` with a short expiry, which is why `POST /rides` carries that ID rather than
recomputing.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Location shard dies | Its drivers vanish from the index; no matches there | Every online driver re-reports within one interval, so the index self-heals in ≤ 5 s; warm standby, route on health-check failure |
| Kafka unavailable | Historical path stops; serving unaffected | Non-blocking publish with a bounded local buffer; drop pings rather than backpressure ingest (Ch 03 §6) |
| Redis lock store down | Offer collisions become common; no double-booking | Fail open — the conditional write is the guarantee (§7.4). The payoff for putting correctness in the right layer |
| Driver connection lost while available | Phantom supply, wasted offers, riders wait | Remove from `cell_index` after a 30–60 s grace period; do not touch trip state |
| Driver connection lost on trip | Rider's map freezes | Client buffers and replays with original timestamps; UI shows "reconnecting"; trip state untouched |
| ETA service degraded | Ranking loses road-awareness | Fall back to haversine; matches worse, product works |
| Demand spike in one cell | Shard CPU saturates; local matching latency spikes | Surge damps demand; per-cell admission control (Ch 90); widen k rather than queue |
| GPS spoofing | Fraudulent proximity, gamed incentives | Plausibility checks — implied speed between pings, teleportation across a k-ring — scored offline, not on the hot path |
| Stale shard mapping | Pings routed where nobody queries; supply silently vanishes in a region | Version the mapping, include it in routing, alert on gateway version skew |

**Monitoring:** location writes/sec per shard (the leading indicator for everything); the ratio of online
drivers to drivers in `cell_index` (a slow divergence means the grace-period logic is leaking supply);
matching p99; offers per successful assignment (a rise means collisions, declines, or bad ranking);
conditional-write conflict rate; k-ring expansions by region; and Kafka lag, alerted only when it stops
draining.

---

## 9. Common mistakes

1. **Not noticing the system is write-dominated,** and reaching for read-path machinery — caches, read
   replicas, materialized views — in a system whose traffic is 2,900:1 writes.
2. **Persisting every location update to a database.** Four petabytes a year with a five-second useful
   life; the right shape is a mutable cell overwritten in place, plus a log for real history.
3. **Treating the geospatial index as the hard part.** It is largely a library call. Contention in
   matching, the driver state machine, and the ingest rate are where the design is won or lost.
4. **Believing a distributed lock prevents double-booking.** It makes collisions rare; the conditional
   transition in the authoritative store makes them impossible. Inverting the two is the clearest sign of
   a memorized pattern.
5. **A fixed update interval for every driver.** Half the fleet is parked; adaptive frequency removes ~37%
   of the constraining number and improves battery life, for a page of logic.
6. **Streaming live positions to unmatched riders**, turning a 52,000/sec problem into a broadcast fanout
   problem for nothing — or **ranking by straight-line distance**, when ETA is the real signal.
7. **Sharding geography by area rather than load,** splitting a metro across a boundary and converting a
   rare cheap two-shard query into the permanent state of the busiest region.
8. **Coupling trip state to connection state.** A driver in a tunnel has not ended their trip; business
   state is driven by explicit events, and the connection is a transport detail.

---

## 10. Variants

**Food and grocery delivery (DoorDash, Instacart).** The same structure, with every difference in matching
rather than the index. Supply is scarcer, so batching — one courier carrying several orders — becomes a
routing optimization rather than nearest-driver selection; the restaurant is a third party with its own
state machine and unreliable timing, so the match is made against a *predicted* readiness time; and
couriers on bicycles or on foot make the road graph and ETA model mode-dependent.

**Proximity search over static points (Chapter 51).** The exact inverse: objects do not move, the index is
precomputed and cached, and the workload is read-dominated — reading 50 and 51 back to back is the
cleanest demonstration that the geospatial index is not the design, the traffic shape is. **Matching over
semi-static users (Chapter 52)** sits between them: positions change over minutes, so the ingest problem
disappears and a different contention problem, mutual-match detection, takes over.

**Fleet and asset tracking.** Same ingest shape, no matching stage, and a much stronger history
requirement — the trajectory *is* the product, so §7.1's trade-off reverses and the durable log becomes
the primary store. **Emergency dispatch** keeps the index and changes the contention model entirely: units
are far scarcer, assignment is centralized and priority-driven rather than offered, preemption is legal,
and the availability bar is qualitatively higher.

---

## 11. Further reading

- Chapter 04 §4 for the geospatial primitives applied here; Chapter 80 for §7.4's contention pattern;
  Chapter 70 for the aggregation behind surge; Chapters 51 and 52 for the counterparts
- [Uber Engineering — H3: A Hexagonal Hierarchical Spatial Index](https://www.uber.com/us/en/blog/h3/) · [uber/h3](https://github.com/uber/h3)
- [HelloInterview — Design Uber](https://www.hellointerview.com/learn/system-design/problem-breakdowns/uber)
- Robert Geisberger et al., "Contraction Hierarchies: Faster and Simpler Hierarchical Routing in Road
  Networks" (2008) — the standard precomputation behind a production ETA service
