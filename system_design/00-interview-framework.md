# Chapter 00 — The Interview Itself

A system design interview is not a test of whether you know what Kafka is. It is a test of whether you
can take an underspecified problem, impose structure on it, make defensible decisions under time
pressure, and communicate while doing so. Candidates who know more than their interviewer routinely
fail; candidates with a disciplined process and modest knowledge routinely pass.

This chapter is about the process. It is the most valuable chapter in the book, because a content gap
is recoverable inside an interview and a process failure is not.

---

## 1. What is actually being measured

Interviewers are scoring, usually on a rubric with something close to these axes:

**Problem structuring.** Did you convert a vague prompt into a bounded problem? Did you decide what
matters before designing? A candidate who starts drawing boxes ninety seconds in has already lost
points that cannot be recovered later, no matter how good the boxes are.

**Technical depth.** Can you go two levels below the buzzword? "We'll use a cache" is level zero.
"Cache-aside with a 60-second TTL" is level one. "Cache-aside, and here is what happens when this
key expires while ten thousand requests are in flight, and here is how I prevent that" is level two.
Level two is where senior signal lives.

**Trade-off reasoning.** Every interesting decision has a cost. Candidates who present a design as
obviously correct read as inexperienced. Candidates who say "I'm choosing X; the cost is Y; I'd revisit
this if Z" read as people who have shipped things.

**Communication.** Can the interviewer follow you? Do you check in? Do you notice when they push back?
An interviewer's question is almost always a hint, not idle curiosity.

**Seniority calibration.** Junior candidates are expected to be led. Senior candidates are expected to
lead: to identify the bottleneck themselves, to raise the failure mode before being asked, and to say
how the thing would be operated and rolled out.

Notably absent from the list: *completeness*. You are not expected to design the whole system. You are
expected to design a coherent slice of it well.

---

## 2. The six-step framework

Time budget for a 35–45 minute design round. Adjust proportionally for a 60-minute round; the ratios
hold.

| Step | Minutes | Output |
|---|---|---|
| 1. Requirements | ~5 | Three prioritized functional bullets; quantified non-functional bullets |
| 2. Core entities | ~2 | Three to five nouns |
| 3. API / interface | ~5 | Endpoints covering exactly the functional bullets |
| 4. Data flow *(optional)* | ~5 | Only for pipeline-shaped systems |
| 5. High-level design | ~10–15 | Components and connections that satisfy each endpoint |
| 6. Deep dives | ~10 | Non-functional requirements, bottlenecks, failure modes |

The steps are sequential and each one constrains the next. That constraint is the point: it prevents
the two most common failure modes, which are designing before knowing the problem, and designing a
system so general it solves nothing in particular.

---

## 3. Step 1 — Requirements (~5 minutes)

### Functional requirements

Phrase every one as **"Users should be able to ..."**. This forces you to name an actor and an action,
which is what an API is made of; step 3 becomes mechanical.

Get to a **prioritized list of three**. Not ten. A long list is actively harmful: it consumes the clock,
it commits you to surface area you cannot cover, and it signals that you cannot distinguish core from
peripheral. For a Twitter prompt, the three are: post a tweet, follow a user, read a home timeline.
Search, DMs, notifications, trends, and media are all real features of Twitter and all wrong answers
here.

Say what you are cutting, out loud: *"I'm going to treat direct messages as out of scope — tell me if
you'd rather I include them."* This does three things. It shows judgment, it invites the interviewer to
redirect you cheaply, and it protects you later when they ask why DMs aren't in your design.

**Elicitation technique.** The prompt is deliberately vague, and the interviewer is waiting to see what
you ask. Good questions are the ones whose answers change the architecture:

- *"Is the feed chronological or ranked?"* — changes whether you need a scoring stage.
- *"Do users follow other users, or subscribe to topics?"* — changes the entire fanout problem.
- *"Roughly how many users, and how skewed is the follower distribution?"* — changes push versus pull.

Bad questions are the ones whose answers change nothing: *"Should it be mobile or web?"*,
*"What programming language?"*, *"Do we need authentication?"*. Asking these burns your five minutes and
signals that you don't know which details matter.

### Non-functional requirements

Phrase as **"The system should ..."** and **quantify wherever you can**. `"p99 read latency under 200 ms"`
is a requirement. `"low latency"` is a mood. The quantified version is what lets you later say "this
design gives us 200 ms, and here is the component that would break that promise first."

Cover this checklist, then stop:

- **Scale** — QPS for the dominant read and write paths, and total data volume. Two numbers, not ten.
- **Latency** — for each user-visible operation that has a different budget.
- **Availability** — and be specific about *which* path. Almost every system has a path that must stay up
  (reading a URL) and a path that may degrade (creating one).
- **Consistency** — the single most under-asked requirement. Different endpoints in one system usually
  need different guarantees, and saying so is a strong signal. A timeline can be seconds stale; a seat
  reservation cannot be stale at all.
- **Durability** — can we lose data on a crash? For chat, no. For a "user is typing" indicator, entirely.

Two more if the interviewer's company cares (Amazon in particular):

- **Cost** — most candidates never mention money. Mentioning it once, concretely, is disproportionately
  valuable.
- **Operability** — how it is monitored, deployed, and rolled back.

---

## 4. Step 2 — Core entities (~2 minutes)

List three to five nouns. `User`, `Tweet`, `Follow`. No columns, no types, no keys yet.

This step looks trivial and is not. It is a cheap commitment device: once the entities are on the board,
the API in step 3 and the schema in step 5 have to be consistent with them, and you have removed a
whole class of mid-interview drift where the design quietly changes shape.

Resist the urge to model completely. You will add entities during step 5 as the design demands them, and
adding them there — visibly, in response to a need — reads better than having guessed correctly up front.

---

## 5. Step 3 — API (~5 minutes)

Write the interface. REST is the right default; use gRPC if you're describing service-to-service calls,
and a WebSocket frame vocabulary if the system is push-shaped (chat, live updates).

```
POST /tweets              {text, mediaIds?}          -> {tweetId}
GET  /feed?cursor=&limit= -> {tweets[], nextCursor}
POST /users/{id}/follows  -> 204
```

There should be one endpoint per functional requirement, and no endpoints that don't correspond to one.
If you find yourself writing an endpoint you didn't promise in step 1, you have scope creep; either drop
it or go back and amend the requirements explicitly.

Three details that are free credibility:

**Never accept the acting user's identity in the request body.** `POST /tweets {userId, text}` is a
security hole; the caller can post as anyone. Derive it from the auth token. Interviewers notice this,
and it takes four words to get right.

**Use cursor pagination, not offset,** for anything feed-shaped. Offset pagination over a list that is
being prepended to shows duplicates and skips items. Cursors are opaque and encode a position in the
ordering.

**Name the resource in the plural and let the verb be the HTTP method.** `POST /users/{id}/follows`, not
`POST /followUser`. This is a small thing that signals API fluency.

---

## 6. Step 4 — Data flow (~5 minutes, optional)

Only for systems where data is *transformed* on its way through: analytics pipelines, crawlers,
transcoders, aggregators. Write the transformation as a linear sequence in words before you draw
anything:

```
click event → validate & enrich → deduplicate → window by event time → aggregate → store → serve queries
```

For CRUD-shaped systems (a URL shortener, a booking site) this step adds nothing. Skip it and say you're
skipping it — knowing which tool doesn't apply is itself signal.

---

## 7. Step 5 — High-level design (~10–15 minutes)

### Build incrementally

Take the endpoints from step 3 one at a time and draw only what that endpoint needs. Then take the next
one and extend. The board grows in a way the interviewer can follow, and every component appears at the
moment it becomes necessary, which is the same as explaining why it exists.

The alternative — drawing the complete architecture immediately, then narrating it — is worse even when
the architecture is right, because it reads as a memorized answer rather than a derivation.

### Do not over-engineer

Meet the functional requirements first, with the simplest thing that works. When you see an optimization
coming, **say it and defer it**: *"This read is going to need a cache; I'll come back to it in the deep
dive."* This is not weakness. It shows you know the difference between the skeleton and the refinement,
and it banks a deep-dive topic for step 6.

A single relational database is very often the correct step-5 answer. Candidates who reach for a
distributed store before the load justifies it are demonstrating pattern-matching, not engineering.

### Justify every arrow

If you cannot say what property a component buys you, delete it. "We'll put Kafka here" is worth nothing;
"we'll put a queue here so the write path returns in 10 ms and the fanout happens asynchronously — and
if the queue backs up, timelines are stale rather than broken" is worth a great deal. Same box, entirely
different signal.

---

## 8. Step 6 — Deep dives (~10 minutes)

This is where the interview is decided. Everything before it is table stakes.

### Choose the dives yourself

At senior level, do not wait to be asked. Open with something like: *"The two things I'd want to stress
here are the celebrity fanout problem and what happens when the cache for a hot tweet expires. Let me
take the first one."*

Reliable places to find a good dive:

1. **The uncomfortable number** from step 3's estimation. If one arithmetic result was alarming, that is
   the deep dive. In a feed system it's the fanout write rate; in a location system it's the ingest rate.
2. **The hot key.** Almost every system at scale has one entity that is orders of magnitude more popular
   than the median. Ask what happens to it.
3. **The failure path.** What happens when the downstream is slow, the queue backs up, a node dies
   mid-write, a client retries. Most candidates design only the happy path.
4. **The non-functional requirement you promised** in step 1 but haven't yet delivered.
5. **Whatever the interviewer poked at.** If they asked a clarifying question about a component, they are
   telling you where they want to go. Follow.

### Structure of a good dive

State the problem quantitatively. Give at least two options. Choose one, and name the cost of choosing
it. Say what would make you choose differently.

> "At 100 million followers, fanout-on-write means 100 million row appends for one tweet. Two options:
> keep pushing but shard the timeline store harder, which doesn't fix the write amplification, only
> spreads it; or don't push for accounts above a follower threshold and merge their tweets in at read
> time. I'd take the second. The cost is that reads get more complex — every read now does a second
> query and a merge — and I need a threshold, which is a tuning knob, not a constant. I'd start it
> somewhere around a hundred thousand followers and adjust on measured fanout cost."

That paragraph is the entire skill being tested.

---

## 9. Estimation, and when to skip it

Back-of-envelope arithmetic is a tool, not a ritual. **Compute a number only if it changes a decision.**

Numbers that usually change decisions:
- Write QPS on the fanout path (push versus pull)
- Total data volume (single node versus sharded)
- Connection count (stateless versus stateful fleet sizing)
- Bytes per second through a component (does this fit on one machine?)

Numbers that usually don't:
- Average object size to three significant figures
- Precise storage after five years, when the decision at one year and ten years is the same

Useful anchors, worth memorizing:

| Quantity | Value |
|---|---|
| Seconds per day | ~86,400, i.e. ~10^5 |
| 1 million/day | ~12 per second |
| 1 billion/day | ~12,000 per second |
| Memory read, 1 MB sequential | ~0.25 ms |
| SSD read, 1 MB | ~1 ms |
| Network round trip within a datacenter | ~0.5 ms |
| Network round trip cross-continent | ~150 ms |
| Modern server, connections held | ~100k concurrent sockets |
| Modern server, simple HTTP QPS | ~10k–50k |

Round aggressively. `500M × 2 = 10^9`, not 1,000,000,000. Say "about" and move on. The interviewer is
checking whether you can reason about magnitude, not arithmetic.

---

## 10. Failure patterns

Each of these is common enough to have cost real candidates real offers.

**Designing before scoping.** The single most common failure. Symptom: a box appears on the board in the
first two minutes.

**The component parade.** Listing technologies without justification. Symptom: Kafka, Redis,
Elasticsearch, and Cassandra all appear, and none of them is defended.

**Ignoring the interviewer.** They ask "how does that handle a user with ten million followers?" and you
answer the question you wish they'd asked. Every question is a hint. If they ask twice, you have missed
something important.

**Happy path only.** No timeouts, no retries, no consideration of what a partial failure looks like.
Ask yourself, before step 6 ends: what does this system do when a dependency is slow rather than down?
Slow is harder than down and more common.

**Over-engineering.** Designing for 100× the stated scale. It reads as inability to size a problem, and
it burns time you needed for depth.

**Under-committing.** Presenting three options for every decision and choosing none. Trade-off reasoning
means *choosing*, then naming the cost. A design with no decisions in it is not a design.

**Silence.** Thinking quietly for ninety seconds. Narrate. "I'm weighing whether to..." is fine; a blank
pause is not, because the interviewer cannot score what they cannot hear.

**Running out of time before the deep dive.** The most expensive form of the scoping failure. Watch the
clock at the twenty-minute mark; if you are not into step 5, compress and move.

---

## 11. Handling pushback

When an interviewer challenges a decision, they are doing one of three things, and the responses differ:

**Testing whether you'll fold.** They disagree with a correct decision to see if you defend it. Correct
response: restate the reasoning, ask what constraint they have in mind, and hold the position if it
survives. Folding instantly on a correct answer is scored badly.

**Correcting a real error.** Correct response: say so plainly, integrate it, move on. No lengthy apology;
one sentence and continue. Recovering cleanly from a mistake is positive signal, not neutral.

**Steering.** They think a different area is more interesting. Correct response: go there immediately.
Do not finish your current thought first.

Distinguishing these is mostly a matter of asking one question back: *"Are you thinking of a case where
the write volume is much higher, or something else?"* The answer tells you which of the three it is.

---

## 12. A worked opening

For concreteness, the first four minutes of a "design Twitter" round, in full:

> **Candidate:** Before designing, let me scope this. I'll assume the core product is: a user can post a
> tweet, follow other users, and read a home timeline of tweets from people they follow. I'm treating
> DMs, search, and trending topics as out of scope — say the word if you'd rather I cover one of those.
>
> A few things that change the design. Is the timeline chronological, or ranked?
>
> **Interviewer:** Let's say chronological.
>
> **Candidate:** Good, that removes a scoring stage. Second: how skewed is the follower distribution — do
> we have accounts with tens of millions of followers?
>
> **Interviewer:** Yes, assume so.
>
> **Candidate:** Then that's going to be the central problem, and I'll spend most of the deep dive there.
> For scale, I'll assume 500 million daily actives, each loading the timeline twice a day, so roughly
> 12,000 timeline reads per second on average and call it 50,000 at peak. Writes are far lower — around
> 100 million tweets a day, so a bit over 1,000 a second.
>
> Non-functionally: timeline reads should be under 200 milliseconds at p99, the read path needs to be
> highly available, and I'll take eventual consistency on the timeline — it's fine if a tweet takes a few
> seconds to appear. Writes need to be durable; losing a tweet is not acceptable.
>
> Core entities are User, Tweet, and Follow. Let me write the API.

Four minutes, and the interviewer already knows the candidate can scope, estimate, distinguish
consistency requirements, and identify the hard part before designing it. Everything after this is
easier.

---

## Further reading

- [HelloInterview — Delivery Framework](https://www.hellointerview.com/learn/system-design/in-a-hurry/delivery)
- [system-design-primer — How to approach a system design interview question](https://github.com/donnemartin/system-design-primer#how-to-approach-a-system-design-interview-question)
- [Tech Interview Handbook — System design](https://www.techinterviewhandbook.org/system-design/)
