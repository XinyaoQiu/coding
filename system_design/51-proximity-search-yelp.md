# Chapter 51 — Proximity Search (Yelp)

> **Prerequisites:** Chapters 01 (indexing, store selection), 02 (caching, CDN), 04 (§4 geospatial indexes), 50 (the write-dominated inverse of this chapter)
> **Patterns:** static geo index, geo filter combined with text and facets, retrieval then ranking, cache keying on continuous values

---

## 1. The problem

A user opens an app, and sees restaurants near them — filtered by cuisine, price, and rating, ordered by
some blend of distance and quality, with photos and review counts.

Chapter 50 designed a system where the indexed objects move constantly and are queried rarely. This
chapter is its mirror image: **the indexed objects almost never move, and are queried constantly.** A
restaurant changes location roughly zero times in its life. Every trade-off that chapter made in favor of
write throughput, this chapter reverses in favor of read latency, and walking through the reversal is the
most instructive thing here — it demonstrates that "geospatial system" is not a design, it is a category
containing designs with opposite shapes.

**The property that makes it hard:** the geo predicate is never the only predicate. A pure nearest-neighbor
query is a solved problem with a well-known index. A query for *"open now, Thai, under $30, rated above
4.0, within 2 km, ranked by a blend of distance and quality, page 3"* has to intersect a spatial filter
with several categorical filters and a text match, then rank the survivors — and no single index serves
all of those. Choosing where the geo filter sits in that pipeline, and what to precompute, is the design.

---

## 2. Requirements

### Functional

1. Search businesses near a location, with a radius or a viewport.
2. Filter by category, price band, rating, and open-now.
3. Rank results and paginate them.

Defer, but name: reviews and photos as their own subsystem, reservations, business owner tooling,
personalized recommendations, and full free-text search over review bodies.

### Non-functional

- **Read latency** — p99 under 200 ms end to end. This sits in front of a map render the user is already
  waiting on, and it is re-issued on every pan and zoom.
- **Read:write ratio** — extreme, and this is the defining requirement. Business records change on the
  order of once a month; searches run continuously. Assume 10^5:1 or worse.
- **Freshness** — minutes is fine for business attributes; **open-now must be correct to the minute**,
  which is the one place freshness bites and is treated in §7.4.
- **Availability** — the read path must stay up. A stale result set is vastly better than an error.
- **Consistency** — eventual, everywhere. A newly added restaurant appearing in search five minutes later
  is invisible to users. Say this explicitly; it licenses the whole precomputed-index design.
- **Correctness of the geo predicate** — a result outside the requested radius is a visible bug, even
  though the index that finds it is approximate. See §7.1.

### Explicitly out of scope

Routing and turn-by-turn navigation, real-time wait times, and delivery logistics (which is Chapter 50's
shape, not this one).

---

## 3. Estimation

Assume 50 million businesses worldwide and 100 million daily searches.

**Read rate**

```
100M searches/day / 86,400  ≈ 1,160 searches/sec average
peak (3×, and lunch/dinner
 create sharp local peaks)   ≈ 3,500 searches/sec
```

Add map interaction: each pan or zoom reissues a query, so the *request* rate is several times the
*search-intent* rate. Call it 10,000 QPS at peak.

**Write rate**

```
50M businesses × ~12 attribute edits/year / 31.5M sec ≈ 19 writes/sec
```

Nineteen. This is the number that defines the chapter. Against 10,000 reads/second it is a ratio of
roughly 500:1 on requests, and far higher on rows touched. **You can afford to do essentially unbounded
work per write in exchange for less work per read** — precompute aggressively, rebuild indexes offline,
denormalize without hesitation, and never optimize the write path.

Contrast with Chapter 50, where the equivalent figure was two million writes per second and the entire
design was arranged around not persisting them.

**Storage**

```
50M businesses × ~2 KB (name, address, hours, categories, ratings, geo) ≈ 100 GB
```

One hundred gigabytes. The entire business corpus fits comfortably in the memory of a modest cluster,
which is worth saying out loud because it means the index does not have to be clever about spilling to
disk. Reviews and photos are far larger and are deliberately not in this dataset — they are fetched on
detail view, not on search.

**Index cardinality**

```
50M businesses ÷ 2^20 H3 cells at resolution 7 (~5 km² each)
```

Population is wildly non-uniform: Manhattan cells hold tens of thousands of businesses, rural cells hold
zero. **This skew, not the total, is the indexing problem** — a fixed resolution is either too coarse
downtown or absurdly fine in the desert. §7.1 deals with it.

---

## 4. API

```
GET /search
  ?lat=37.7749&lng=-122.4194
  &radius=2000                       # meters; OR bbox=minLat,minLng,maxLat,maxLng
  &q=thai                            # optional free text
  &categories=restaurants,thai
  &price=1,2
  &min_rating=4.0
  &open_at=2026-09-05T19:30:00Z      # absolute instant, not "now" — see below
  &sort=relevance|distance|rating
  &cursor=<opaque>&limit=20
  -> 200 {results:[{id,name,distance_m,rating,review_count,thumb_url,...}],
          nextCursor, total_estimate}

GET /businesses/{id}                 # detail view: hours, photos, reviews
POST /businesses/{id}/reviews        # the write path, deliberately separate
```

Four decisions worth defending:

**Both a radius and a bounding box.** A phone asking "near me" wants a radius; a map wants the rectangle
currently on screen. Supporting only one forces the client to approximate the other, and a circle
inscribed in a viewport discards the corners the user can see.

**`open_at` is an absolute timestamp, not the literal string "now".** Two reasons. It lets the user ask
about Friday evening rather than this instant, which is a real product need. And it makes the request
**cacheable**, because "now" is a different value every millisecond and would give every request a unique
cache key. The client sends a coarsened timestamp; §7.5 explains why the coarsening matters more than it
looks.

**`total_estimate`, not `total`.** An exact count requires evaluating every match; an estimate comes free
from the index. Promising an exact total in the API contract commits you to work the product does not
need.

**Cursor pagination.** Offset pagination over a ranked, filtered result set forces the engine to compute
and discard the first N on every page. A cursor encoding `(sort_key, tiebreak_id)` resumes in place. This
matters more here than in a feed, because the result set is expensive to produce rather than merely
expensive to store.

---

## 5. Data model

```
businesses                        -- source of truth, relational
  business_id     BIGINT PRIMARY KEY
  name            TEXT
  lat, lng        DOUBLE
  address         JSONB
  categories      TEXT[]
  price_band      SMALLINT         -- 1..4
  status          SMALLINT         -- active / closed / unverified
  updated_at      TIMESTAMP

business_hours                    -- separate: multiple rows per business
  business_id     BIGINT
  day_of_week     SMALLINT
  open_min, close_min  SMALLINT    -- minutes from local midnight
  PRIMARY KEY (business_id, day_of_week, open_min)

business_stats                    -- derived, updated asynchronously
  business_id     BIGINT PRIMARY KEY
  rating_avg      REAL
  review_count    INT
  popularity      REAL             -- decayed engagement signal
```

And the thing that actually serves searches — a **denormalized search document**, one per business, held
in the search engine:

```
{ business_id, name, name_ngrams, categories[], price_band,
  location: {lat, lng},           -- indexed as a geo_point
  h3_r7, h3_r8, h3_r9,            -- precomputed cell ids at three resolutions
  rating_avg, review_count, popularity,
  hours_bitmap,                   -- 7×24×2 bits: is this business open in this half-hour?
  timezone }
```

Three deliberate decisions:

**The search document is a derived artifact, not the source of truth.** It is rebuilt from the relational
tables, and it can be dropped and rebuilt in full — at 50M documents, a full rebuild is hours, which is an
acceptable disaster-recovery story precisely because writes are rare. This is the freedom that a
19-writes-per-second workload buys, and it is why the design can denormalize without fear of the
consistency problems that would plague a write-heavy system.

**Hours are stored twice**: normalized in `business_hours` for editing and display, and as a
`hours_bitmap` in the search document for filtering. The bitmap is 336 bits — one per half-hour of the
week — so "open at this instant" becomes a single bit test rather than an interval comparison against a
variable number of rows. Precomputing it costs nothing at 19 writes/second and removes the most awkward
predicate from the hot path. §7.4 covers what the bitmap cannot express.

**H3 cells are precomputed at several resolutions** and stored as plain keyword fields. The index does not
need to understand geometry to filter on them — a cell ID is just a term, and term filters are the thing
an inverted index is fastest at. This is the move that lets the geo predicate participate in the same
index as every other filter, rather than living in a separate system that must be intersected afterward.

---

## 6. Architecture, derived

### Attempt 1: relational table with a bounding-box query

```sql
SELECT * FROM businesses
WHERE lat BETWEEN :minLat AND :maxLat
  AND lng BETWEEN :minLng AND :maxLng
  AND price_band = ANY(:prices)
  AND rating_avg >= :minRating
ORDER BY <something>
LIMIT 20
```

This is where most people start, and it fails for a reason that is worth understanding precisely rather
than dismissing.

A B-tree index on `(lat, lng)` is ordered on `lat` first. The query can use the index to narrow to the
latitude band — but a 2 km latitude band spans the entire globe in longitude, so at 50M businesses that
band contains on the order of

```
50M × (4 km / 20,000 km of latitude range) ≈ 10,000 rows
```

before the longitude predicate is applied, and those rows are scattered across the index. The engine reads
all of them and discards ~99%. Adding a second index on `(lng, lat)` does not help; the planner picks one.
This is the fundamental limitation from Chapter 01, §3: **a B-tree orders on one dimension, and a
proximity query constrains two.**

The measured result is tens of milliseconds at best, degrading badly in dense areas, before any category
filter or ranking has run. Against a 200 ms end-to-end budget that includes ranking and hydration, it does
not leave enough room.

### Attempt 2: a spatial index

Replace the B-tree with something that understands two dimensions: PostGIS with a GiST/R-tree index, or
precomputed geohash/H3 cells with a prefix or term match (Chapter 04, §4).

This fixes the geo predicate. A radius query now touches the cells that intersect the query circle and
reads only businesses in them — hundreds of rows, not tens of thousands.

It does not fix the query. The categorical filters and the text match are still evaluated as a post-filter
over the spatial result:

```
spatial index → 800 candidates in radius
  → filter: category = thai        → 40
  → filter: price in (1,2)         → 25
  → filter: rating >= 4.0          → 11
  → filter: open now               → 6
  → rank, return 6
```

In a sparse area this is fine. In Manhattan, a 2 km radius contains 30,000 businesses and the spatial
index hands all of them to a post-filter that discards 99.98% of them. The index has solved the easy half
and left the hard half untouched, and the failure is worst exactly where traffic is highest.

The inverse ordering fails symmetrically: filter by category first (there are 400,000 Thai restaurants
worldwide) and then apply the geo predicate, and you have made the same mistake in the other direction.

### Attempt 3: put every predicate in one inverted index

Stop treating "spatial" as a separate kind of query. **A cell ID is a term.** If the search document
carries its H3 cell at several resolutions as a keyword field, then the geo predicate becomes
`h3_r8 IN (<the ~20 cells covering the query circle>)` — an ordinary terms filter, intersected by the
engine with `categories:thai`, `price:(1 OR 2)`, `rating >= 4.0`, and the hours bit, using the same posting
lists and the same skip-list intersection machinery.

```
h3_r8 IN (…20 cells…)   ∩   categories:thai   ∩   price:(1,2)   ∩   rating≥4.0
       ↓
   40 candidates, intersected in the index, never materialized as rows
       ↓
   score and rank
       ↓
   top 20
```

The engine's query planner intersects the *most selective* posting list first. In Manhattan, `thai` is
more selective than the cell set; in a small town, the cell set is more selective. **You do not have to
choose the order — the index does, per query.** That adaptivity is the reason this attempt succeeds where
both fixed orderings failed, and it is the core insight of the chapter.

Elasticsearch does this natively (its `geo_distance` filter, or a terms filter on precomputed cells, both
compose with the rest of the boolean query). So does any Lucene-derived engine. Building it by hand is
possible and rarely justified.

### Attempt 4: separate retrieval from ranking, and cache

Retrieval produces a few hundred candidates cheaply. Ranking — which blends distance, rating, review
count, popularity, text relevance, and sponsored placement — is expensive per candidate and must not run
over the full match set. Keep the stages separate, exactly as in Chapters 22 and 61.

Then cache. With writes at 19/second and reads at 10,000/second, the cache hit rate determines the size of
the entire serving fleet. §7.5 is about the one thing that makes caching hard here: the query contains a
continuous coordinate, so the naive cache key is unique per request and the hit rate is zero.

### Final architecture

```
  WRITE PATH (19/sec — deliberately unoptimized)

  Business edits ──► Postgres (source of truth)
  Reviews ─────────► Postgres ──► async aggregation ──► business_stats
                                          │
                                          ▼
                              indexer (CDC or periodic)
                                          │
                        builds denormalized search documents
                        (H3 cells at r7/r8/r9, hours bitmap)
                                          │
                                          ▼
  READ PATH (10k/sec)              Elasticsearch (50M docs, sharded by geo region)
                                          ▲
  Client ──► LB ──► Search service ───────┤
                      │  │                │
                      │  └─► Redis: cached result sets, keyed on
                      │        (snapped cell, quantized radius, filters, time bucket)
                      │
                      └─► rank ──► hydrate from Redis: business detail cache
                                          │
                                          ▼
                                    response (20 results)
```

Note what is *not* in the read path: the relational database. It is the source of truth and the input to
the indexer, and a search never touches it. That separation is what makes the read path survivable when
Postgres is down for maintenance — searches keep working against a slightly stale index, which is the
correct degradation for this product.

---

## 7. Deep dives

### 7.1 Cell resolution under extreme population skew

A single H3 resolution cannot serve both Manhattan and Montana. At resolution 8 (~0.7 km² per cell), a
Manhattan cell holds thousands of businesses and a Wyoming cell holds none; covering a 5 km rural radius at
r8 requires hundreds of cell terms in the query.

Three approaches:

**Fixed resolution, tuned to the median.** Simple, and it fails at both tails simultaneously — that is what
"median" means when the distribution is a power law.

**Multi-resolution indexing (the choice).** Store the cell ID at three resolutions in each document
(`h3_r7`, `h3_r8`, `h3_r9`) and let the *query* choose which field to filter on, based on the requested
radius. A 500 m radius uses r9 and needs ~7 cells; a 10 km radius uses r7 and needs ~10. The number of
terms in the query stays roughly constant regardless of radius, which is the property you want. The cost
is three keyword fields per document instead of one — trivial, given 19 writes/second.

**Adaptive subdivision (a quadtree).** Subdivide only where density demands it, so every leaf holds a
bounded number of businesses. Theoretically the best fit for skew. In practice it means maintaining a
mutable tree structure alongside an inverted index that does not understand it, reintroducing the
two-systems problem that attempt 3 just solved. Not worth it here; it *is* worth it in Chapter 50, where
the objects move and the index is bespoke anyway.

**The correctness caveat.** Cells are an approximation of a circle. The cell set covering a query circle
includes area outside it, so the candidate set contains businesses beyond the requested radius. **Always
apply an exact haversine distance filter after retrieval**, before ranking. Skipping it produces results
that are visibly outside the radius the user asked for — a bug users notice immediately and report as
"this app is broken", even though the index is behaving exactly as designed. Under-covering is the worse
error in the other direction: if the cell set fails to cover the circle's edge, you silently omit valid
results, which nobody reports and nobody notices. Prefer over-covering plus an exact filter.

### 7.2 Why this chapter inverts Chapter 50

The comparison is worth making explicitly, because it is the clearest demonstration that "use a
geospatial index" is not an answer to anything by itself.

| Dimension | Chapter 50 (Uber) | This chapter (Yelp) |
|---|---|---|
| Dominant load | ~2M writes/sec | ~10k reads/sec, 19 writes/sec |
| Object motion | Continuous | Effectively never |
| Index location | In-memory, bespoke, mutable | Inverted index, rebuildable, near-immutable |
| Durability of location | Discarded; only current position matters | Permanent; it is a business attribute |
| Query shape | "k nearest available, right now" | "matching many predicates, ranked, paginated" |
| Correct staleness | Sub-second | Minutes |
| Cache value | Near zero — the answer changes every second | Decisive — it sizes the fleet |
| What you optimize | Absorbing writes without persisting them | Precomputing so reads do almost nothing |
| Failure of the naive design | Write amplification | Post-filter selectivity |

The single sentence version: **when objects move, you cannot precompute; when objects do not move, you
must.** A candidate who has internalized this can be handed either problem and derive the right shape
rather than reaching for the geospatial index they happen to remember.

### 7.3 Ranking, where distance is only one signal

Sorting purely by distance produces bad results, and this is a product fact worth stating in an interview
because it justifies the whole retrieval/ranking split. The nearest restaurant is frequently a closed
storefront with two reviews. Users asking for "restaurants near me" want *good* restaurants that are
*near*, and the weighting between those is the product.

A serviceable scoring function over the retrieved candidates:

```
score = w1 · distance_decay(d)          -- exp(-d / d0), not 1/d: bounded, no singularity at d→0
      + w2 · quality(rating, count)     -- Bayesian-smoothed, see below
      + w3 · text_relevance             -- BM25 from the engine, if q was supplied
      + w4 · popularity                 -- time-decayed engagement
      + w5 · sponsored_boost
```

Two details that matter more than the weights:

**Rating must be smoothed toward a prior.** A single 5.0 review is not better than four hundred reviews
averaging 4.6, and a raw average says it is. Use `(C·m + Σr) / (C + n)` where `m` is the global mean and
`C` is a pseudo-count on the order of the median review count. Without this, the top of every result set
is one-review businesses, which is the classic and immediately visible failure.

**Distance decay, not distance inverse.** `1/d` diverges as distance approaches zero, making the ranking
dominated by whichever result happens to be twenty meters closer. An exponential or logistic decay with a
characteristic distance `d0` chosen per query context (walking radius downtown, driving radius in a
suburb) behaves sanely at both ends.

Sponsored placement belongs in the ranker rather than injected afterward, so that a paid result still has
to clear a relevance bar — but it must be visually labeled, which is a product and legal requirement, not
an engineering one.

### 7.4 Open-now, the one predicate freshness actually bites

Everything else in this system tolerates minutes of staleness. "Open now" does not: showing a closed
restaurant is one of the most-complained-about failures in this product category.

The `hours_bitmap` (§5) makes the common case a single bit test — 336 bits covering each half-hour of a
regular week, evaluated in the business's **local timezone**, which must be stored per business rather
than derived from coordinates at query time.

What the bitmap cannot express, and how each is handled:

- **Half-hour granularity.** A business closing at 9:45 is represented as closing at 9:30 or 10:00. Round
  *conservatively* — mark the half-hour closed — so the error is omitting a briefly-open business rather
  than sending someone to a locked door. Choosing which way to round is a judgment about which error the
  user forgives, and saying so is the point.
- **Holidays and temporary closures.** These are exceptions, not weekly patterns. Keep a small
  `hours_exceptions` table, and apply it as a post-retrieval filter over the ~20 results being returned
  rather than as an index predicate. Twenty lookups against a cache is nothing; indexing exceptions would
  mean re-indexing every affected business every time a holiday approaches.
- **Overnight hours.** A bar open 6 p.m. to 2 a.m. spans midnight and, on Sunday, spans the week boundary.
  A bitmap handles this naturally *because it is a flat array of half-hours rather than intervals* — the
  wraparound is just the array wrapping. This is a quiet argument in favor of the bitmap over storing
  intervals, and worth mentioning.
- **Timezones and DST.** The bitmap is in local time; the query timestamp is absolute. Convert once, in
  the query, using the business's stored timezone. Because DST transitions change the mapping, a query
  during the transition hour can be ambiguous — accept it, since the blast radius is one hour twice a
  year and the correct answer is undefined anyway.

### 7.5 Cache keying on a continuous coordinate

This is the deep dive most candidates miss, and it decides whether the cache is worth having.

The obvious cache key includes `lat` and `lng` at full precision. Two users standing next to each other
send coordinates differing in the seventh decimal place, produce different keys, and share nothing. GPS
jitter alone means *the same stationary user* generates a new key every request. **The hit rate is
approximately zero**, and a cache with a zero hit rate is a latency tax.

The fix is to **quantize the key, not the query**:

```
cache_key = hash(
    h3_cell(lat, lng, resolution=9),     -- ~0.1 km² — snap the position
    quantize(radius, [500, 1000, 2000, 5000, 10000]),
    sorted(categories), price_bands, min_rating, sort_mode,
    floor(open_at / 15 minutes),          -- time bucket
    page_cursor
)
```

Every user in a ~300 m cell, asking for the same filters within the same fifteen-minute window, now shares
one entry. In a dense area that collapses thousands of distinct requests into one.

Three consequences to state, because each is a real cost:

- **Results are computed for the cell center, not the user's exact position.** Distances shown may be off
  by up to the cell radius. Recompute the *displayed* distance client-side or in the response layer from
  the user's true coordinates, while keeping the cached *result set* keyed on the cell. The set of results
  is stable across a 300 m move; the distance labels are not.
- **Quantizing the radius means honoring a slightly different radius than requested.** Round the buckets
  *up* and filter down exactly, so you never omit a valid result.
- **The time bucket bounds open-now staleness to 15 minutes,** which conflicts with §7.4's freshness
  requirement. Resolve it by excluding the hours predicate from the cached retrieval and applying it as a
  post-filter on the cached candidate set — the candidates are stable, their open/closed status is not.

The general principle transfers well beyond this chapter: **a cache key containing a continuous value must
be snapped to a lattice, and the error introduced by snapping must be corrected downstream rather than
tolerated.**

### 7.6 Sharding and the map-viewport read pattern

Shard the index **by geographic region**, not by business ID hash. Almost every query is confined to one
small area, so geographic sharding means a query touches one shard instead of scattering across all of
them — the opposite of the advice in Chapter 61, and for a good reason: there, every query genuinely spans
the corpus; here, the geo predicate is a natural partition.

The costs, both manageable:

- **Load skew.** A shard containing Manhattan serves orders of magnitude more traffic than one containing
  Wyoming. Size shards by *traffic*, not by area or document count — which means smaller geographic
  regions in dense areas. Rebalancing is easy because the index is rebuildable (§5).
- **Cross-shard queries at boundaries.** A radius near a shard edge spans two. Scatter to both and merge;
  at two shards rather than fifty, this is cheap. Replicating a boundary margin into both shards removes
  even that, at the cost of duplicate documents.

The viewport pattern deserves its own note: a user panning a map issues a stream of overlapping queries
that differ slightly. Beyond the cell-snapped cache, the effective optimizations are client-side —
debounce the pan, request a viewport somewhat larger than the screen so small movements need no new
request, and let the client filter locally. A meaningful fraction of the load reduction for this product
happens in the client, and mentioning that is not a dodge; it is where the win is.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Search cluster loss | Total outage of the core product | Multi-AZ replicas; the index is rebuildable from Postgres, but a rebuild is hours — replicas, not rebuild, are the availability answer |
| Indexer lag or stall | New and edited businesses do not appear | Alarm on indexer lag; at 19 writes/sec the backlog drains in seconds once fixed, so the alarm matters more than the capacity |
| Hot shard (a dense metro) | Elevated latency confined to one region | Traffic-weighted shard sizing; add replicas of the hot shard specifically |
| Cache stampede after deploy | Full read load hits the search cluster at once | TTL jitter and request coalescing (Chapter 02, §3); warm the top metro cells before taking traffic |
| Stale `business_stats` | Ranking uses old ratings | Tolerable for hours; alarm only on aggregation pipeline failure, not lag |
| Bad bulk import | Thousands of wrong locations, silently | Validate coordinates against the stated address at ingest; alarm on the daily count of businesses whose cell changed — this should be near zero, and a spike is always a bug |
| Timezone data update | Open-now wrong in an affected region | Treat the tz database as a deployed dependency with a rebuild of affected documents |

**Monitoring:** search p99 split by *result-set size* (a slow query is usually a dense-area query, and the
average hides it); cache hit rate by key component, so you can see which part of the key is fragmenting;
candidate-set size distribution at retrieval (a rise means selectivity is degrading before latency shows
it); indexer lag; zero-result rate by region (a spike means an index or filter bug, and it is the metric
that catches problems users experience but never report).

---

## 9. Common mistakes

1. **Designing the write path.** Nineteen writes per second needs no design. Time spent on it is time not
   spent on the read path, and it signals an inability to read the estimate you just produced.
2. **Treating the geo predicate as a separate stage** and post-filtering everything else against it. This
   is attempt 2, and it fails precisely in the dense areas that carry the traffic.
3. **Using a plain B-tree on `(lat, lng)`** without being able to say why it fails — the one-dimensional
   ordering argument is the substance, not the conclusion.
4. **Omitting the exact distance filter after cell retrieval.** Cells over-cover; results appear outside
   the requested radius; users see a bug.
5. **A single fixed cell resolution**, which fails simultaneously at both ends of a power-law density
   distribution.
6. **Caching on raw coordinates**, producing a hit rate of zero, and then reporting the cache as a design
   element as though it were doing something.
7. **Ranking by raw average rating**, filling the top of every page with one-review businesses.
8. **Ranking by pure distance**, which is what the user literally asked for and not what they want.
9. **Sharding by business ID hash**, turning every geographically local query into a scatter-gather across
   the entire fleet.

---

## 10. Variants

**Local delivery and store locators.** Same read-dominated shape, smaller corpus, and usually a
"deliverable-to" polygon rather than a radius — a point-in-polygon test rather than a distance test, which
argues for a true spatial index (PostGIS) over cell terms.

**Real estate search.** Identical structure, with the geo predicate almost always a drawn polygon or a
viewport rather than a radius, and far heavier numeric range filtering (price, square footage, bedrooms).
The retrieval/ranking split matters even more because the ranking signals are weaker.

**Hotel and travel search.** Adds availability over a date range, which is a genuinely different problem:
availability is high-churn inventory, so the read-dominated assumption of this chapter breaks for that one
field. The usual resolution is to keep the static index here and intersect with a separate availability
service at retrieval time, accepting the extra hop.

**"Find friends nearby."** Looks like this chapter and is actually Chapter 50 — the objects move. The
lesson of §7.2 is that the product wording tells you nothing; the write rate does.

**Store-within-map at country scale (a coverage map).** When the query returns thousands of pins rather
than twenty results, switch from returning records to returning **precomputed aggregates per cell** — a
count and a centroid — and only fetch records when the viewport is small enough. This is a different API,
and noticing that it needs one is the correct answer.

---

## 11. Further reading

- Chapter 50, for the write-dominated mirror image, and Chapter 04, §4, for the index primitives
- Chapter 61, for the inverted index machinery this chapter leans on, and Chapter 22, for the
  retrieval/ranking split in its general form
- [Uber Engineering — H3: A Hexagonal Hierarchical Spatial Index](https://www.uber.com/us/en/blog/h3/)
- Elasticsearch documentation on `geo_point`, `geo_distance`, and `geo_bounding_box` queries
- PostGIS documentation on GiST indexes and `ST_DWithin`, for the relational alternative
- Manning, Raghavan, and Schütze, *Introduction to Information Retrieval*, for posting-list intersection
  and query planning over inverted indexes
