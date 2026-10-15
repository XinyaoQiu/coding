# Chapter 03 — Asynchrony and Streaming

Almost every scalable design in Part II works by taking something off the request path. This chapter is
about what happens once you do: the queue you put it in, what "delivered" means, how retries stop being
dangerous, and how a stream of events becomes a number someone can query.

---

## 1. Why asynchrony

Three distinct reasons, and being clear about which one you are invoking is part of justifying the queue.

**Latency.** The user's request returns as soon as the durable record exists; the expensive work happens
after. Uploading a video returns in milliseconds; transcoding takes minutes.

**Load smoothing.** Producers burst; consumers process at a steady rate. The queue absorbs the difference.
Without it, a traffic spike must be absorbed by capacity that is idle the rest of the day.

**Decoupling.** The producer does not know who consumes, and a consumer being down is not a producer
outage. This is what makes it possible to add a fourth consumer of an event stream without touching the
service that emits it.

The cost, always: **you have replaced a synchronous failure with an eventual one.** The user gets a 200
and the work may still fail later, which means you now owe them a status mechanism, and you owe yourself
a dead-letter queue and an alert. Designs that add a queue without acknowledging this are incomplete.

---

## 2. Queues and logs

Two different things, often conflated.

### Message queue (RabbitMQ, SQS)

A message is delivered to one consumer and then **removed**. Consumers compete for work. The queue is a
work distribution mechanism; its natural depth is zero.

- Good at: task distribution, per-message acknowledgment, priority, delayed delivery.
- Weak at: replay (the message is gone), multiple independent consumers of the same message, ordering.

### Log (Kafka, Pulsar, Kinesis)

An **append-only, partitioned, retained** sequence. Consumers track their own offset; reading does not
remove anything. Multiple independent consumer groups read the same data at their own pace.

- Good at: multiple consumers, replay and reprocessing, high throughput, ordering within a partition.
- Weak at: per-message acknowledgment, priority, per-message delay, very large messages.

**Ordering in a log is per-partition only.** All messages that must be ordered relative to each other must
share a partition key. This is the constraint that shapes many designs: in a chat system, keying by
`conversation_id` guarantees a conversation's messages are ordered, and deliberately gives up any global
ordering — which nobody needed.

**Retention is a feature.** Kafka's ability to replay a week of events is what makes reprocessing after a
bug possible, and it is the foundation of the reconciliation pattern in Chapter 70. When you choose a log
over a queue, this is usually the reason; say it.

### Which to name

If the interviewer's system has one consumer doing work items, a queue is honest and simpler. If it has
several independent consumers, or you will ever want to replay, it is a log. Saying "a queue — Kafka"
without noticing the difference is a small tell.

---

## 3. Delivery semantics

**At-most-once.** Send, don't retry. Messages are lost on failure. Acceptable for telemetry where a
dropped sample is invisible; unacceptable for almost everything else.

**At-least-once.** Retry until acknowledged. Nothing is lost; **duplicates are guaranteed** — not
possible, guaranteed, because the acknowledgment can be lost after the work was done. This is the
default and correct choice for nearly every system.

**Exactly-once.** What everyone wants and what does not exist as a network property. Two distinct things
travel under this name:

1. **Exactly-once processing within a closed system.** Real. Flink with Kafka achieves it: operator state
   is checkpointed, the sink's output is committed transactionally at checkpoint boundaries via two-phase
   commit, and the consumed offsets stored in the checkpoint are kept in lockstep with the committed
   output. Restarting from a checkpoint therefore neither loses nor duplicates. This works because the
   framework controls both the state and the output commit.

2. **Exactly-once delivery across an arbitrary service boundary.** Not real. The moment your consumer
   calls an external API, a lost response is indistinguishable from a failed call.

**The universal answer is therefore: at-least-once delivery plus an idempotent consumer.** Say it in those
words. It is correct, it is what production systems do, and it demonstrates you know why "exactly-once"
in a vendor's marketing is scoped more narrowly than it sounds.

---

## 4. Idempotency

An operation is idempotent when performing it twice has the same effect as performing it once. This is
the property that makes retries safe, and retries are what make distributed systems work.

### Idempotency keys

The general mechanism:

1. The **client** generates a unique key per logical operation — not per attempt. A retry reuses the key.
2. The **server**, before doing the work, attempts to claim the key in a store with a TTL.
3. If the claim fails, the operation has been seen: return the **stored result of the first attempt**,
   not just a success code. Returning a bare 200 to a retried payment loses the payment ID the client
   needed.
4. On completion, store the result against the key.

Where keys come from in practice: a client-generated UUID (payments, message sends), or a deterministic
hash of the event's identity — `SHA256(userId + eventId + type)` is the standard construction for
notification deduplication (Chapter 32), because it makes the key derivable rather than transmitted.

TTL is a judgment call: long enough to cover any plausible retry window (24 hours is common), short
enough that the store does not grow forever.

### Making operations naturally idempotent

Better than bolting on a key:

- **Absolute rather than relative.** `SET status = 'shipped'` is idempotent; `INCREMENT retry_count` is
  not. Prefer setting a value over adjusting one wherever the semantics allow.
- **Conditional writes.** `UPDATE ... WHERE version = 7` succeeds once, no matter how many times it runs.
- **Unique constraints.** A uniqueness constraint on `(order_id)` makes duplicate inserts fail loudly and
  harmlessly, and — importantly — the database enforces it regardless of which application instance is
  confused.
- **Deterministic identifiers.** If the ID of the thing being created is derived from the request rather
  than generated, a duplicate request produces a duplicate primary key rather than a second row.

---

## 5. Retries

Retries are the other half of at-least-once, and naive retries cause outages.

**Exponential backoff with jitter.** Backoff alone leaves all failed clients retrying in lockstep, which
reproduces the original overload on a schedule. Jitter — randomizing the delay — decorrelates them. Full
jitter (`sleep = random(0, min(cap, base * 2^attempt))`) is the standard.

**Retry only what is retryable.** A 400 will be a 400 forever; retrying it wastes capacity and delays the
inevitable error. Classify: 4xx client errors are terminal (except 429 and 408); 5xx and network timeouts
are retryable. Chapter 32 has the concrete version of this for push providers, where a 410 means the
device token is dead and retrying it forever is a real production bug.

**Retry budgets and circuit breakers.** Under a broad failure, retries multiply load on an already-failing
dependency and turn a partial outage into a total one. A circuit breaker trips after a failure threshold,
fails fast for a cooling period, then allows a trickle of probe requests. A retry budget caps retries as
a fraction of total requests, so a system-wide failure cannot produce a system-wide retry storm.

**Beware layered retries.** Three layers each retrying three times is twenty-seven requests. Decide which
layer owns retrying and make the others pass failures through.

### Dead-letter queues

Messages that exhaust their retries go to a DLQ rather than being dropped or blocking the queue. A DLQ
without an alert and a documented replay procedure is a place data goes to die quietly; say both when you
propose one.

**Poison messages** — a message that deterministically crashes its consumer — will otherwise be redelivered
forever and stall the partition behind it. The DLQ is what breaks that loop.

---

## 6. Backpressure

When producers outpace consumers, something must give. The options, in order of preference:

1. **Buffer** — the queue absorbs it. Bounded; know the bound and what happens at it.
2. **Slow the producer** — the correct answer when the producer is your own service. TCP does this
   natively; application protocols usually need it built in.
3. **Shed load** — reject requests explicitly, ideally the least valuable ones. A fast, clear rejection is
   far better than an unbounded queue that turns into unbounded latency.
4. **Scale consumers** — right, and not instantaneous; autoscaling has a lag measured in minutes, which
   is longer than most spikes.

The failure to avoid is the **unbounded queue**, which converts an overload into a latency problem that
grows without limit and is invisible until every request is timing out. Bound your queues. When the bound
is hit, drop or reject *and emit a metric*.

---

## 7. Stream processing

Turning an unbounded event stream into aggregates that can be queried. Chapters 70, 71, and 73 are
applications of this section.

### Event time versus processing time

**Processing time** is when your system saw the event. **Event time** is when it actually happened.

They differ because of network delay, mobile clients that were offline, retries, and pipeline lag. Any
aggregation that windows on processing time produces **wrong numbers whenever the pipeline is behind** —
and the pipeline will be behind, usually at exactly the moment the numbers matter.

Always window on **event time**. The composite key for a per-minute aggregation is
`(entity_id, truncate(event_timestamp, 1 minute))`.

### Watermarks

If you window on event time, you need to decide when a window is complete, given that late events may
still arrive. A **watermark** is the framework's assertion that no event with a timestamp earlier than T
will arrive from now on. When the watermark passes the end of a window, the window is closed and emitted.

Watermarks are heuristics. What happens to an event that arrives after the watermark is a policy you must
choose:

- **Drop it.** Simple, loses data, fine for approximate dashboards.
- **Allowed lateness.** Keep the window state for an extra interval and emit an updated result.
- **Side output.** Route late events elsewhere for separate handling or reconciliation.

Being able to say "I'd window on event time with a watermark, allow lateness of five minutes, and route
anything later than that to a reconciliation path" is a strong, specific answer.

### Window types

- **Tumbling** — fixed, non-overlapping. Each event in exactly one window. The default.
- **Sliding** — fixed size, overlapping. "Last 5 minutes, updated every minute."
- **Session** — bounded by a gap in activity. Variable length; useful for user sessions.

### Stateful operators and checkpointing

Aggregation requires state, and state must survive failure. Frameworks checkpoint operator state
periodically to durable storage; recovery restores the state and rewinds the input to the matching
offsets. Checkpoint interval is the trade-off between recovery time and steady-state overhead.

### Lambda and Kappa

**Lambda architecture** runs a fast approximate streaming path alongside a slow exact batch path over the
same raw data, and the batch results overwrite the streaming results as they become available.

**Kappa architecture** keeps only the streaming path, and handles reprocessing by replaying the retained
log through a new version of the job.

Kappa is simpler and usually right. But for anything involving **money**, keep a batch reconciliation job
that recomputes yesterday's numbers from raw events and corrects the serving store. Fraud invalidation,
late events beyond your lateness bound, and bugs in the streaming job all produce divergence that only a
recomputation catches. Saying this in Chapter 70's interview separates people who have operated a billing
pipeline from people who have read about one.

---

## 8. Change data capture

Reading a database's replication log and publishing the changes as an event stream (Debezium is the
common implementation). Useful for keeping a search index, a cache, or a downstream service in sync
without the writing service knowing they exist.

Compared with the outbox pattern (Chapter 01, §9): CDC requires no application change but exposes the
database schema as an interface, which couples consumers to internal structure. The outbox is explicit
and gives you a designed event shape at the cost of application code. For a greenfield service, the
outbox is usually the better answer; for retrofitting onto a system you cannot change, CDC is.

---

## 9. Checklist for an async decision

1. What does the user see between the acknowledgment and the completion? (There must be an answer.)
2. What is the delivery semantic, and where is the idempotency enforced?
3. What is the partition key, and what ordering does it therefore guarantee?
4. What is the queue's bound, and what happens when it is reached?
5. Where do permanently failing messages go, and who is paged?
6. If the consumer is down for an hour, does the system recover on its own?

Question 6 is the one that justifies retention, and question 1 is the one candidates most often have no
answer to.

---

## Further reading

- Martin Kleppmann, *Designing Data-Intensive Applications*, chapters 11 and 12
- [Uber Engineering — Real-Time Exactly-Once Ad Event Processing with Apache Flink and Kafka](https://www.uber.com/us/en/blog/real-time-exactly-once-ad-event-processing/)
- Apache Flink documentation on event time, watermarks, and checkpointing
- Marc Brooker, "Exponential Backoff And Jitter" (AWS Architecture Blog)
