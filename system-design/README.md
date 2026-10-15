# System Design — A Textbook

This is not a cheat sheet. Each chapter is written to be *read*, not skimmed: it develops a system from
a naive first attempt, shows precisely what breaks and at what scale, and derives the real architecture
from that failure. Where a decision is contested, both sides are argued before one is chosen.

## How this book is organized

**Part I — Foundations** (chapters 00–04) builds the vocabulary. Every system chapter assumes it. Read
these in order, once, properly. They are dense; that is deliberate. Nothing in them is repeated later.

**Part II — Systems** (chapters 10–A4) is the body. Each chapter is self-contained and follows the same
structure, so that the tenth chapter is easier to read than the first:

1. **The problem** — what the product is, and the one property that makes it hard
2. **Requirements** — functional and non-functional, with the reasoning that produces them
3. **Estimation** — worked arithmetic, with the number that constrains the design called out
4. **API** — the interface, and why it is shaped that way
5. **Data model** — schema, store selection, partitioning
6. **Architecture, derived** — naive design → what breaks → the fix, repeated until it holds
7. **Deep dives** — the two to four decisions that actually distinguish a good design
8. **Failure modes and operations** — what happens at 3 a.m.
9. **Common mistakes** — including the ones that sound sophisticated
10. **Variants** — how the design bends for adjacent products
11. **Further reading**

A chapter is finished when you can reproduce section 6 on a whiteboard from memory and defend every
arrow in it.

## Reading orders

- **Cover to cover** — Part I, then Part II in numeric order. Difficulty is roughly monotonic.
- **Interview in two weeks** — Part I fully, then chapters 10, 20, 30, 32, 40, 41, 50, 70, 71, 80, 81,
  90, 91. These thirteen cover the great majority of what gets asked, and every remaining chapter is a
  recombination of ideas from them.
- **By theme** — follow the pattern index below; a system you have never seen is usually two patterns
  you already know, glued together.

## Part I — Foundations

| # | Chapter | Covers |
|---|---|---|
| 00 | [The interview itself](00-interview-framework.md) | The six-step framework, time budgets, how requirements are elicited, how deep dives are chosen, scoring |
| 01 | [Data and storage](01-foundations-data-and-storage.md) | Storage engines (B-tree vs LSM), relational vs wide-column vs document, indexing, partitioning, replication, consistency models, CAP and PACELC, transactions and isolation |
| 02 | [Caching and delivery](02-foundations-caching-and-delivery.md) | Cache patterns, invalidation, stampede/penetration/avalanche, eviction algorithms, CDNs, load balancing, client-push protocols (polling, SSE, WebSocket, native push) |
| 03 | [Asynchrony and streaming](03-foundations-async-and-streaming.md) | Queues and logs, delivery semantics, idempotency, backpressure, dead-letter queues, stream processing, event time and watermarks, exactly-once as a mechanism |
| 04 | [Primitives](04-foundations-primitives.md) | Distributed IDs, distributed locks and fencing, rate-limiting algorithms, geospatial indexes, probabilistic data structures, consistent hashing, observability |

## Part II — Systems

### Links and basic storage
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 10 | [URL shortener](10-url-shortener.md) | Read-heavy KV, key generation, collision handling | P0 |
| 11 | [Distributed ID generator](11-id-generator.md) | Coordination-free uniqueness, clock skew | P1 |

### Feed and social
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 20 | [Social feed (Twitter/X timeline)](20-social-feed.md) | Hybrid fanout, write amplification, skew | P0 |
| 21 | [Instagram](21-instagram.md) | Feed + media pipeline + ephemeral content | P0 |
| 22 | [Ranked news feed](22-news-feed-ranking.md) | Retrieval/ranking separation, feature serving | P1 |
| 23 | [Reddit / message board](23-reddit.md) | Per-community precomputation, comment trees, decay ranking | P1 |

### Realtime messaging
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 30 | [Chat (WhatsApp)](30-chat-whatsapp.md) | Persistent connections, ordering, offline delivery | P0 |
| 31 | [Live comments](31-live-comments.md) | Broadcast fanout, ephemerality, sampling | P1 |
| 32 | [Notification system](32-notification-system.md) | Fanout pipeline, provider semantics, idempotency | P0 |
| 33 | [Presence](33-presence.md) | Heartbeats, TTL state, deliberate staleness | P2 |

### Large blobs and media
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 40 | [Video platform (YouTube)](40-video-platform-youtube.md) | Chunked upload, DAG transcode, ABR, CDN | P0 |
| 41 | [File sync (Dropbox)](41-file-sync-dropbox.md) | Content-addressed blocks, delta sync, conflict resolution | P0 |
| 42 | [Video streaming (Netflix)](42-video-streaming-netflix.md) | Offline encode, CDN placement, ABR ladders | P1 |

### Location
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 50 | [Ride sharing (Uber)](50-ride-sharing-uber.md) | Geospatial index, write-dominated ingest, matching | P0 |
| 51 | [Proximity search (Yelp)](51-proximity-search-yelp.md) | Static geo index, geo + text ranking | P1 |
| 52 | [Matching (Tinder)](52-matching-tinder.md) | Candidate retrieval, mutual-match detection, seen-sets | P1 |

### Crawl and search
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 60 | [Web crawler](60-web-crawler.md) | URL frontier, politeness, dedup at scale, traps | P1 |
| 61 | [Search and the inverted index](61-search-inverted-index.md) | Index sharding, real-time indexing, retrieval + ranking | P1 |
| 62 | [Autocomplete](62-autocomplete.md) | Tries, precomputed top-K, offline rebuild | P2 |

### Counting and aggregation
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 70 | [Ad click aggregator](70-ad-click-aggregator.md) | Stream processing, event time, exactly-once, reconciliation | P0 |
| 71 | [Top-K and trending](71-top-k-trending.md) | Count-Min Sketch, min-heap, windowing, merge | P0 |
| 72 | [Leaderboard](72-leaderboard.md) | Sorted sets, sharded ranking, approximate rank | P1 |
| 73 | [Metrics and monitoring](73-metrics-monitoring.md) | Time series, cardinality, rollups, alert evaluation | P1 |

### Contention and money
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 80 | [Ticket booking](80-ticket-booking.md) | Holds, optimistic concurrency, admission control | P0 |
| 81 | [Payment system](81-payment-system.md) | Idempotency, double-entry ledger, sagas, reconciliation | P0 |
| 82 | [Online auction](82-online-auction.md) | Bid ordering, sniping, live price fanout | P1 |
| 83 | [Trading system](83-trading-system.md) | Deterministic matching engine, event-log replication | P2 |

### Infrastructure components
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| 90 | [Rate limiter](90-rate-limiter.md) | Algorithm selection, atomic distributed state, failure policy | P0 |
| 91 | [Distributed cache](91-distributed-cache.md) | Consistent hashing, O(1) eviction, replication | P0 |
| 92 | [Job scheduler](92-job-scheduler.md) | Timing wheels, leases, catch-up semantics | P1 |
| 93 | [Pub/sub log (Kafka)](93-pubsub-kafka.md) | Partitioned log, offsets, ISR, rebalancing | P1 |
| 94 | [Distributed key-value store](94-key-value-store.md) | Quorums, anti-entropy, gossip, conflict resolution | P2 |

### Collaboration and AI
| # | Chapter | Core pattern | Priority |
|---|---|---|---|
| A0 | [Collaborative editor (Google Docs)](a0-collaborative-editor.md) | Operational transformation vs CRDT | P2 |
| A1 | [Online multiplayer game](a1-online-game.md) | Server-authoritative state, matchmaking, clocks | P1 |
| A2 | [Code judge](a2-code-judge.md) | Sandboxing, worker pools, burst load | P1 |
| A3 | [LLM inference service](a3-llm-inference.md) | Continuous batching, KV cache, SLO scheduling, streaming | P1 |
| A4 | [Recommendation system](a4-recommendation-system.md) | Candidate generation, ranking, feature stores, feedback loops | P1 |

## Pattern index

Look up a pattern to find every chapter that exercises it.

| Pattern | Chapters |
|---|---|
| Fanout on write vs on read | 20, 21, 22, 23, 30, 32 |
| Write amplification and skew | 20, 30, 50, 70, 71 |
| Chunking and content addressing | 40, 41 |
| Async pipelines and DAG processing | 32, 40, 60, 70 |
| Contention on scarce resources | 50, 80, 81, 82, 83 |
| Idempotency and retry semantics | 30, 32, 70, 80, 81, 92 |
| Geospatial indexing | 50, 51, 52 |
| Approximate counting | 60, 70, 71, 72, 73 |
| Persistent client connections | 30, 31, 33, 50, 82, A1, A3 |
| Retrieval then ranking | 22, 51, 61, A4 |
| Consistent hashing | 60, 90, 91, 94 |
| Two-stage guard (fast path + authoritative constraint) | 50, 80, 81 |

## Conventions used throughout

- **p50 / p99** refer to latency percentiles. Unqualified "latency" means p99.
- **QPS** is queries per second, measured at average load; peak is called out separately when it matters.
- Storage arithmetic uses decimal units (1 GB = 10^9 bytes) because capacity planning does.
- Where a real company's published design is known, it is cited and distinguished from the
  interview-standard answer, which is sometimes different and occasionally wrong.
- Code is pseudocode unless a language is named. It exists to disambiguate prose, not to compile.

## Status

Complete. 5 foundation chapters, 38 system chapters.

| Part | Chapters | State |
|---|---|---|
| Part I — Foundations | 00–04 | Complete |
| Part II — Systems | 10–A4 | Complete |

Every chapter in Part II follows the eleven-section structure described above and ends with a
Further reading section pointing at real sources. Cross-references between chapters are by number and
have been verified to resolve.
