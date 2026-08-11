# TikTok Interview Script (English)

The interviewer may speak Chinese or English. This file mirrors `tiktok_bq_ch.md` one-to-one.

---

## Part 1 — HR Screen

### Self-intro

Hi, I'm [Name]. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also made the server become source of truth for the  upload status, instead of in clients local storage, so we can check the real status and reason from media platform and return them to the user. Finally, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window. I also wrote the entire handler for Apple notifications, including a guard function for out-of-order and duplicate events. And I used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had two internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work and built an in-app booking service so users didn't have to jump out to a third-party app. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule range test experiments.

### Why are you leaving Newsbreak

I was laid off. The company and a re-org and I was in scope.

That year at Newsbreak I grew a lot, and I'm genuinely grateful they gave me the chance to own a full pipeline end to end. That experience mattered to me.

And because I interned at ByteDance, I'm familar with your tools and infrastructure. Also I like my internship experience and ByteDance culture. So I'm really interested in this opportunity.

### Career goals

My direction is pretty clear — I want to go deep on backend and large-scale distributed systems. I'm not looking to switch tracks.

Everything I've done this past year converges on one thing. The state consistency in the subscription system, the cross-system state orchestration in video upload, the Protobuf migration — underneath, they're all about how you keep multiple systems correct with respect to each other. That's the class of problem I find most interesting, and the one I have the best instincts for.

Going forward I want to keep doing this somewhere with a higher engineering bar, and grow from owning one pipeline to being responsible for a wider set of technical decisions. That's exactly why I want to join TT — the problems are hard enough and the team is strong enough that I can do this long term and keep learning.

### Work authorization

I'm on F-1 OPT, first year. My major is STEM, so with the extension I have roughly three years I can work legally. Beyond that, if I stay long term, I'd need the company to sponsor an H1B.

### Questions to ask

- "You mentioned the team mainly works on XX — for this role, what would the first six months likely focus on?"
- "What do the next steps in the process look like?"

---

## Part 2 — Coding Round

### Self-intro

Hi, I'm [Name]. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our UGC video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also moved upload status out of the client's local storage and onto the server, so we can send the real status and the actual failure reason back to the user. Third, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window, wrote an ordering guard for out-of-order and duplicate events, and used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had three internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work — processing third-party travel and hotel data, comparing it against historical data and removing the parts that had diverged. I also built an in-app booking service so users didn't have to jump out to a third-party app. At Alibaba Cloud I worked on big data infrastructure, a data lake — formatting log files and storing them into lake tables through MapReduce. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule test experiments.

I mostly work in Go. Really looking forward to this — let's get started.

### Phrases

| Situation | What to say |
|---|---|
| **Opening** | "Let me take a couple of minutes to read through this. Then I'll restate the problem and walk you through my approach before I start coding." |
| **Restating** | "Let me restate the problem. I'm given ___, and I need to return ___. Is that right?" |
| **Walking an example** | "Let me walk through the first example. The input is ___, the output is ___, and my understanding of why is ___." |
| **Constraints** | "Can I assume ___?" / "If the input is empty, what should I return — 0 or -1?" |
| **Approach** | "Here's my approach. I'm going to ___. Concretely: first ___, then ___, and finally ___." |
| **Complexity** | "Time is O(___), space is O(___). I don't think we can do better, because we have to look at every ___ to know the answer." |
| **Before coding** | "Before I code, here are the edge cases I see: ___ and ___. More may come up as I write — I'll call them out. If that sounds good, I'll start." |
| **Found a case midway** | "Writing this out surfaced a case I hadn't thought about: ___. Let me handle it." |
| **Testing after coding** | "Let me write a few test cases to verify. Starting with example one: ___, then empty input, then ___." |
| **Test doesn't match** | "That's not what I expected — I expected ___ but got ___. Let me see where it diverges." → once found: "The issue is ___ — this should be ___, not ___. Let me fix it." |

**When stuck** (memorize these three)

| Situation | What to say |
|---|---|
| Stuck | "Let me think out loud. I know I need ___, but I'm not sure how to handle ___. My instinct is ___ — does that seem like a reasonable direction?" |
| "Can you do better?" | "I think O(___) is already optimal — we have to look at every element, so linear is the floor. Do you see room I'm missing?" |
| Corrected | "Got it — so ___ comes from ___, not ___. That changes how I'm thinking about it: I'd ___. Let me redo the approach." |

### Testing

**Before coding**, say: empty input, invalid input. **After coding**, run each one out loud with concrete numbers for the intermediate variables — don't just narrate the logic.

| Category | Example | Catches |
|---|---|---|
| Empty | `[]` `""` `[[]]` | index errors, `len(arr[0])` crash |
| Single element | `[x]` `[[x]]` | loop runs once, `for i in range(1,n)` never runs |
| All same | `[1,1,1]`, all-ones matrix | branches that never execute |
| No answer | all ones (looking for 0), cycle (topo sort) | does the -1 branch actually get hit |
| Answer at first/last | `[0,1,1]` `[1,1,0]` | off-by-one |
| **Non-square** | `[[1,1,0],[0,0,1]]` (2×3) | **swapped dimensions, `i*m+j` vs `i*n+j`** |
| 1×n / n×1 | `[[0,0,0]]` `[[0],[0],[0]]` | m and n mixed up |
| Unexpected values | `[0,1,2]` when assuming 0/1 | missing `elif` branch — use `else` |

**What to say**: "Let me check these systematically. Structurally, my loop starts at index 1, so let me verify n = 1: ___. Then empty input: ___. Then the no-answer case: ___. And finally the answer at the last position, since that's where off-by-one usually shows up."

### Seven rules

1. Any silence longer than 15 seconds needs a sentence in front of it giving you permission.
2. Never say a term you can't explain — they will always follow up on your own words.
3. Say the edge cases **before** you code, and leave the door open for more.
4. When asked "can you do better," think about the lower bound first and stand your ground. If you're unsure, say "let me think about that" — **never guess a complexity**.
5. The 30 seconds you spend restating the problem is free thinking time. Your mouth runs the template while your head works on the solution.
6. Finding a new edge case while coding doesn't cost you anything. **Attribute it to the structure of the code**, not to "I forgot."
7. Alternate, don't parallelize — think 20 seconds, say it out loud, think another 20.

### Prompts (AI-assisted round)

Asking the AI for a full solution is fine and expected — what's graded is what you do after you get it. **AI generates options, you decide; AI implements decisions, you verify.**

| Situation | What to ask the AI |
|---|---|
| **Clarifying the problem** | "Split this into two parts: what the statement explicitly says, and what it does NOT say but would change the implementation. Questions only — don't answer them for me." |
| **Structure named in the problem** | "The statement mentions a \<DAG/tree/cache\>. What makes it necessary? If removing it leaves behavior unchanged, say so." |
| **Checking your own reading** | "My reading: given ___, calling ___ returns ___. Does that contradict the statement?" |
| **Stuck on approach** | "Don't write code yet. Give me 2-3 approaches with complexity for each, and the key insight behind the efficient one." |
| **Ready to implement** | "Implement `def f(...)` using \<your approach\>. Constraints: ___. Handle ___." |
| **Don't understand a line** | "What does line N do, and what breaks if I remove it?" |
| **Before trusting the code** | "What inputs would break this? List the assumptions you're least sure about." |
| **Strongest verification** | "Write an obviously-correct slow version plus a random input generator, and diff them over a few thousand cases." |
| **Proving tests discriminate** | "Invert the comparison on line N so I can confirm that test goes red." |
| **Final sweep** | "Any dead code, any test that can never fail, anything the tests don't cover?" |

**Two things to do yourself, not via prompt**

| When | Do |
|---|---|
| After the design discussion, **before** any code exists | Hand-write 4-5 `assert` lines — your contract. Tests written after an implementation encode what the code *does*, not what it *should* do. |
| At least once, in your own terminal | Run it. Never accept "all tests passed" as reported. |

**Anti-patterns**

| Don't | Why |
|---|---|
| "How do I solve this?" → paste → "done" | The one genuinely fatal move |
| Ask AI to write a tech design first | You end up reviewing its architecture instead of authoring your own |
| Re-prompt the same thing when AI is wrong | Fix it by hand — repeated re-prompting is a strong negative signal |
| Accept reported test results | Treat the tool as suspect — that's the whole point of the format |
| Only ask for hints, never answers | Downgrades the AI to a slow search engine, and reads as performing |

### Questions to ask

- "What's the main tech stack the team uses?"
- "What does the code review and release process look like?"
- "How long does it usually take for someone new to start picking up work independently?"

---

## Part 3 — HM Round

### Self-intro

Hi, I'm [Name]. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our UGC video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also moved upload status out of the client's local storage and onto the server, so we can send the real status and the actual failure reason back to the user. Third, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window, wrote an ordering guard for out-of-order and duplicate events, and used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had three internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work — processing third-party travel and hotel data, comparing it against historical data and removing the parts that had diverged. I also built an in-app booking service so users didn't have to jump out to a third-party app. At Alibaba Cloud I worked on big data infrastructure, a data lake — formatting log files and storing them into lake tables through MapReduce. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule test experiments.

Out of all of these, the one I can walk through end to end is the premium subscription system. If that works for you, I'd like to start there.

### Main story 1 — Premium subscription system

Let me talk about something I built in our premium subscription system. My job was to handle the notification Apple sends us and update the current status for each user. We have five states: active/cancel/billing_retry/grace_period/expired. We store the current paid status for each user in mongodb. It contains last_event_time, transaction_id and current status. I create this record only when notification type is subscribed.

The hardest part is notifications are out of order, they might be duplicated, and some might be lost. I need to handle them correctly.

The first thing I hit was concurrency. Imagine that a failed charge and the successful retry arrive at the same time. If both come in at once, two workers read the same record from database, each computes its own answer, and write. The later write overwrites the earlier one. So I added a distributed lock using Redis command SetNX. The key is user id, and value is a globally unique value. The unique value will prevent workers from deleting locks that are not created by them. And I set the TTL as 30 seconds so a worker cannot have the lock forever, with the controller timing out at 10 — keeping the timeout strictly under the TTL means a lock can't expire while the previous worker is still running. To delete the key, I use a Lua script and combine check value and delete into one atomic command. Notifications for the same user get serialized.

Once things were serialized, the second problem was ordering. The order they arrive in isn't the order things actually happened. Notifications can be delayed or redelivered. For example, say the user turns auto-renew on, and then later turns it off. If the "turn on" notification arrives after the "turn off" one, and I just process them in arrival order, that late "turn on" overwrites the cancel and puts them back to active. The user thinks they cancelled, but we auto-renew and charge them next cycle. So I keep a watermark on each record — the event time of the last notification I processed — and a new one only gets processed if it's newer than that. Anything older gets dropped. The key question is which timestamp you compare: I use the event time Apple signed into the notification, not our receive time and not the database's updated-at.

The watermark handles late notifications, but it does nothing for duplicates. Sometimes the request never reaches us, or we have to return a non-200 code, and Apple retries — so the same notification can arrive more than once. For that I use `notification_uuid` as an idempotency key in Redis, with a 7-day TTL. One detail: the key is written **after** the state write succeeds, not claimed before processing. If I claimed it up front and then crashed mid-handler, the key would survive without the state write, and the redelivery would get rejected by our own key — that notification would be lost. Writing it last means at worst we reprocess once, which computes the same state anyway.

Another edge case here is cross-transaction. If the user cancels and resubsecribes later, they are different transactions, so Apple would send notification with different transaction_id. I cannot compare the event_time now because they are different lifecycles, and Apple doesn't guarantee the ordering for events across two transactions. So if the incoming notification's transaction id is different from the one on the record, I call Apple's subscription status API and let Apple tell me which one is currently active. If the incoming one isn't the active one, I just drop it. And even when Apple does confirm it's active, that only means the notification is safe to process — I still only re-point the record's transaction id when the type is `SUBSCRIBED`. A renewal or a cancel for a transaction we've never seen shouldn't be allowed to take over the record.



#### Follow-up 1: What did you get wrong along the way?

The watermark. At first, the timestamp I compared against was the record's updated-at in the database. My thinking was: we update the record every time we process something anyway, so why not just use that as the watermark.

But that timestamp records when I finished processing, not when the thing actually happened. And processing has real lag — from the moment the platform sends a notification to the moment I finish handling it, tens of seconds or more can pass. So the updated-at in the database is always later than the real event time. Then the next notification comes in, I compare, and its event time looks *older* — so I drop it as stale. **The result was that new notifications were getting thrown away and the state just froze on the old value.**

The fix was to add a dedicated field storing the event time the platform signed into the notification itself, and compare against that. That way I'm comparing when things actually happened, which doesn't care how fast we process or whether the clocks on our machines agree.

What this taught me: **to order events, use the time the event carries with it, not the time we observed it.** The first is a fact about the world. The second has our own system's state mixed into it.

#### Follow-up 2: If you built it again, what would be different?

Quite a bit. And not in the sense of adding some feature — **I'd draw the consistency boundary in a different place entirely.**

Here's where the current design starts from. I was thinking: one notification for one user has to be atomic from start to finish. So I took a lock and wrapped the whole thing in it — read the record, claim the idempotency key, compute the new state, write it, drop the cache, write the audit log. Six things, all inside the lock. The upside is direct: no intermediate state is ever visible to anyone else, every before/after pair in the audit log is exactly accurate, and if something goes wrong I can walk it back step by step.

But that choice costs something. Wrapping the lock that wide means I depend on Redis being up, I have to set a TTL on the lock, I have to reason about what happens when the TTL expires and another worker walks in, and on release I have to check with a Lua script that the lock is still mine. All of that complexity exists because I wanted those six things bound together.

**If I built it again, I'd start from the other end: make only the state write atomic, and push everything else outside.**

Concretely, use Mongo's conditional update — `findOneAndUpdate`, with a filter that says `last_notif_event_time < my event time`, and write the notification's unique id onto the record in the same operation. That one operation does three things at once: it checks whether this notification is newer than what's stored, it confirms it hasn't been processed before, and it writes. Correctness comes from single-document atomicity, so no lock is needed. Dropping the cache and writing the audit log move after that update and happen asynchronously.

**Here's the difference.** The current design says "carve out a mutually exclusive window, and inside it I can do whatever I want." The other one says "don't carve out a window — make the write itself carry all the conditions, so if the write succeeds, every condition held." The first is pessimistic: claim it first, then act. The second is optimistic: just try it, and if the conditions don't hold it fails on its own.

**Idempotency changes with it.** Right now I keep the `notificationUUID` in Redis, because it's a separate thing from the state record and needs somewhere to live. But if the check has already moved down into that conditional update, the unique id can go into Mongo directly — either on the record itself, or as a unique index on the audit table's uuid so that the insert *is* the dedup. That way the audit table serves two purposes with one piece of data: it's the log for after-the-fact investigation, and it's the live idempotency key. No separate Redis layer to maintain.

**So what's the trade-off?**

First, **the audit trail gets holes in it.** Right now the audit write is inside the lock, tied to the state write — either both happen or neither does. Move it out, and you can have the state write succeed while the audit write fails, so a row goes missing. For a system that deals with money, "I changed the state but left no record of it" is something you have to think hard about.

Second, **the cache inconsistency window gets a bit longer.** Right now the cache delete is inside the lock, so it happens immediately after the write. Move it out and there's a gap, and during that gap the odds of a reader getting a stale value go up. That said, the impact is limited — we have a TTL as a backstop anyway, and the read paths that genuinely care about freshness bypass the cache and hit the database directly.

The third point isn't really a trade-off, it's **the part neither design changes**: business logic can't be pushed down into the database. A conditional update can express "whose timestamp is newer" and "have I seen this one," but it can't express "who's allowed to create a record when none exists," and it definitely can't express cross-transaction ownership — that requires calling Apple first to ask which transaction is active, which is application-layer work regardless of whether you use a lock. (Worth noting that callback already sits outside the lock today, because it's a cross-network call and putting it inside would make the lock's TTL depend on Apple's response time.) So the guard survives in either design. The only difference is whether it's a block of logic protected by a lock, or a check that runs before the conditional update.

**So the two designs really diverge right at the start**: do you guarantee mutual exclusion over the whole handler, or atomicity over the one write. Pick the first and you get strict traceability, at the cost of complexity and a dependency on Redis. Pick the second and you get fewer dependencies and simpler failure modes, at the cost of accepting some lag in your audit trail and your cache. I picked the first because this is money and I wanted every step to be traceable. But if I were deciding today I'd seriously consider the second — especially somewhere that already has a mature asynchronous audit pipeline, because then the first cost basically goes away.

---

### Main story 2 — Video upload pipeline

Let me talk about a content platform project I worked on. I own the whole server-side pipeline for UGC content — from a creator submitting it, through our content platform processing it, through moderation, to it either going live or getting rejected and taken down. Video is one of the main content types on that path.

I'll describe this from two angles, because at its core it's a content platform plus a governance flow.

First the platform side. We have a media platform service that is the state authority for all content. News, short posts, comments, video — everything lands a metadata record there with the title, URL, content type, current state, and the doc id we generate. The raw video bytes don't live there; those are in S3. That platform orchestrates the whole content lifecycle: content comes in with a "processing" state, we do the downstream work asynchronously, and then the state moves forward to "published" or to one of the failure states.

Then there's how content actually gets processed and ingested. We have a unified content processing platform. Every content type eventually gets normalized into one document format, goes through the same ingestion pipeline, and comes out as a servable doc with a doc id. The thing I want to emphasize is that this is generic and platform-level — video is just one content type. It goes through the same ingestion and moderation flow a regular post does; it just has one extra transcoding step in front. The transcoding itself is a separate video platform, mostly using AWS MediaConvert. Our own ffmpeg setup is only a fallback and generates thumbnails. The multi-bitrate outputs go back into the content's metadata, and at playback time we pick the right bitrate for the network conditions.

Now the governance side, which is where I think this system earns its keep. Once a piece of content is ingested and has a doc id, if it hasn't been moderated yet, our platform actively pushes it to moderation — into a review-request message queue. On the moderation side there's both automated and human review. Once they decide, the verdict comes back on a separate queue, and we update the content's moderation state from it. If it's a violation, we send a rejection notice, take the content down, and stop serving it. So moderation here is a send/receive-split design: we push requests actively, we receive verdicts passively, on two independent queues.

On top of moderation, I also wrote an anti-abuse middleware at the upload entry point. It scores the request IP for bot behavior and does rate limiting, so we can spot scripted spam and abnormal upload patterns. I deliberately shipped it in observe-only mode first — collect signal on real traffic, validate the thresholds, confirm we weren't going to false-positive real creators, and only then consider turning on enforcement.

On the user side, when a creator opens their "my content" page or pulls to refresh, the client fetches status from us. We read the latest state from the media platform live, no cache, because that page is the most sensitive to staleness. Then we translate the platform's internal processing state and moderation state into something the user actually understands — processing, failed, in review, rejected, or live.

The hard part, I think, isn't any single system. It's that this pipeline spans several systems, is asynchronous end to end, and can fail at every step — transcoding can fail, ingestion can fail, or it can just sit in "processing" for too long. So what I had to guarantee is that no matter how it fails in the middle, the final state the user sees is always clear and correct. The way I handled it: every path that can mark content as failed — transcode failure, ingestion failure, and a cron job that scans for things stuck too long — all go through one conditional atomic update that only flips to a failure state if the content is *currently* still in "processing." That way, whichever failure path fires first, it only takes effect once. It dedups naturally, and it can never resurrect content the user already deleted.

So the organizing principle, in one sentence: one unified content platform plus one state authority — all content types processed the same way, with a single hub orchestrating the whole lifecycle from ingestion to moderation to going live or coming down. That's also why I'm particularly interested in the content governance platform this team is building, because what I've been doing is exactly this kind of work: turning content governance into a platform.

### Questions to ask

- "What's the biggest technical challenge the team is focused on over the next couple of quarters?"
- "What does growth look like for engineers here — how do people go from owning features to owning systems?"
