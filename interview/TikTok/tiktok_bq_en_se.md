# TikTok — Solutions Engineer, Ads — Interview Script (English)

Variant of `tiktok_bq_en.md` retargeted for the **Solutions Engineer, Ads** role.
Customer-facing STAR answers in Part 4 are adapted from `../Amazon/amazon_bq.md`.

> **What's different from the SWE script.** This role is judged on whether you can face an
> external advertiser or agency, explain a technical problem to a non-technical audience, and
> drive it to resolution. Raw system depth is table stakes, not the differentiator. The JD calls
> out two things the SWE script never prepared for:
> - *Ability to effectively communicate technical concepts to non-technical audiences*
> - *Ability to collaborate with various sized stakeholder groups internally and externally*
>
> So: halve the implementation detail, double the "who did I talk to, how did we align, how did
> I drive it." Part 4 is the part that wins this loop.

**JD gap to manage.** The posting asks for 3+ years; you have ~1.5 including internships. Don't
raise it. If asked, answer on scope rather than tenure: three production systems owned end to
end, an on-call rotation, and a 40-endpoint migration — then stop talking.

---

## Part 1 — HR Screen

### Self-intro

Hi, I'm Xinyao. You can call me Alex. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our UGC video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also moved upload status out of the client's local storage and onto the server, so we can send the real status and the actual failure reason back to the user. Third, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window, wrote an ordering guard for out-of-order and duplicate events, and used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had two internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work — purging expired records from a large third-party travel dataset — and built an in-app booking service so users didn't have to jump out to a third-party app. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule test experiments.

### Why are you looking for a new opportunity

I'm looking for a bigger stage where my work reaches a much larger audience. Newsbreak has been good to me and I got to own a full pipeline end to end there, which is more scope than most people get in their first year. I'm grateful for that.

But the ceiling on scale is real. I want to work on problems where the traffic and the correctness bar are an order of magnitude higher, and TikTok is at the top of that list for me.

I also interned at ByteDance, so I already know the tooling and the infrastructure, and I liked the culture and the pace there. Being able to come back and do this full-time is genuinely exciting.

And specifically for this role — the work I've enjoyed most has been the parts where I'm the bridge between a system and the people relying on it. Doing that for advertisers, where the technical problem and the business impact sit right on top of each other, is something I'd find a lot more motivating than staying purely internal.

**Red line:** never criticize Newsbreak, never mention layoffs or re-orgs. Pull factor only — scale and technical bar. If pressed on "why not stay and grow there," answer: the systems I'd want to build next don't exist at that traffic level.

### Career goals

I want to stay close to real systems, but I've realized the part I enjoy most isn't writing the code in isolation — it's being the person who connects a technical system to the people who depend on it.

That's actually what most of my work has looked like. The Protobuf migration was really about a contract between us and every client consuming our APIs, and my job was making sure none of them broke. Moving upload status onto the server started from support tickets — users couldn't tell what happened to their own videos. And when I'm on call, half the work is investigation and the other half is explaining to whoever's affected what broke and what it means for them.

So this role reads to me like the natural version of what I already gravitate toward: deep enough technically that I'm debugging real API and backend problems, but pointed outward at customers instead of only inward at the codebase. That's the direction I want to grow in.

**Adaptation note:** the SWE version of this answer says "go deep on distributed systems, not looking to switch tracks." Do NOT use that here — it argues against the role. The pivot is honest: reframe existing work around its customer-facing edge, don't invent new experience.

### Work authorization

I'm on F-1 OPT, first year. My major is STEM, so with the extension I have roughly three years I can work legally. Beyond that, if I stay long term, I'd need the company to sponsor an H1B.

### Recruiter screening questions (short answers)

| Question | Answer |
|---|---|
| **Job-search timeline / deadlines** | Actively interviewing, no competing deadlines right now, so I have flexibility. I'd like to wrap up within the next month or two. |
| **Earliest start date** | Within two weeks of an offer. Timing is flexible. |
| **Current location / relocation** | Mountain View, CA. San Jose is an easy commute, so no relocation needed. |
| **5 days onsite** | Yes, fully open to it. |
| **Preferred coding language** | Most comfortable with Go and Python. Prefer Python for the interview. |
| **Target level** | Targeting IC2, open to whatever level the interviews suggest is the right fit. |
| **Work authorization** | Yes, authorized on F-1 OPT. |
| **Sponsorship needed** | Yes — F-1 OPT with STEM extension eligibility, so H-1B sponsorship later. |
| **Interview availability** | Flexible, can make any slot work, both the U.S. and the China-based windows. |

**On the parallel NG application.** There's also an NG *Backend Software Engineer Graduate
(Global E-commerce) - 2027 Start* application sitting at "Evaluation Passed" with no recruiter
contact and no interview. **Don't raise it unprompted** — different pipeline, different req, and
volunteering it only invites "so which one do you actually want." If they bring it up:

> Yes, I applied to a graduate role earlier, but that one is for a 2027 start and I haven't heard back on it. I'm looking to make a move sooner than that, so this role is what I'm actively focused on.

### Questions to ask

- "For this role, what does the split look like between hands-on engineering and working directly with advertisers?"
- "Who are the main customers the team supports — large brands, agencies, self-serve advertisers?"
- "What do the next steps in the process look like?"

---

## Part 2 — Coding Round

### Self-intro

Hi, I'm Xinyao. You can call me Alex. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our UGC video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also moved upload status out of the client's local storage and onto the server, so we can send the real status and the actual failure reason back to the user. Third, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window, wrote an ordering guard for out-of-order and duplicate events, and used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had two internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work — processing third-party travel and hotel data, comparing it against historical data and purging the expired records. I also built an in-app booking service so users didn't have to jump out to a third-party app. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule test experiments.

I mostly work in Go day to day, and I'm comfortable in Python too. Really looking forward to this — let's get started.

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
- "How much of the day-to-day is writing code versus debugging integrations with customers?"
- "How long does it usually take for someone new to start picking up work independently?"

---

## Part 3 — HM Round

### Self-intro

Hi, I'm Xinyao. You can call me Alex. I did my CS master's at UIUC and graduated last December. Before that I was in a dual-degree program between the University of Michigan and Shanghai Jiao Tong. Since February this year I've been a backend engineer at Newsbreak — it's a local news and AI company, and I'm on the server team.

I've worked on a few things there. First, I migrated a batch of our core APIs from JSON to Protobuf. Before that, both the server and the client would just throw whatever fields they wanted into requests and responses. After the migration they share one schema. Second, I own our UGC video upload pipeline. I wrote an anti-abuse middleware that checks IP reputation through ipinfo and does rate limiting. I also moved upload status out of the client's local storage and onto the server, so we can send the real status and the actual failure reason back to the user. Third, I built new features for our premium subscription system — I added a billing-retry state to the state machine so it handles Apple's billing retry window, wrote an ordering guard for out-of-order and duplicate events, and used a Redis lock to prevent concurrent writes from stepping on each other. On top of that I migrated some Mongo databases to a new cluster and tuned indexes and connection pools along the way. I'm also in the on-call rotation, so I spend a fair amount of time digging into production alerts and finding root causes.

Before Newsbreak I had two internships. At ByteDance I was on the TTOP team, working with TTLS. I did data dump work — processing third-party travel and hotel data, comparing it against historical data and purging the expired records. I also built an in-app booking service so users didn't have to jump out to a third-party app. At Tesla I did a full-stack project, an internal Gantt-chart-style tool for vehicle engineers to schedule test experiments.

Out of all of these, the one closest to this role is the video upload pipeline — it's the one where I was dealing directly with the people affected, translating what the system was doing into something they could act on. If that works for you, I'd like to start there.

**Adaptation note:** the SWE script opens with the premium subscription system. For this loop, lead with video upload instead — it has the customer-pain origin and the cross-team coordination an SE interviewer is listening for. Keep premium as the depth story if they push for hard technical detail.

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

So the organizing principle, in one sentence: one unified content platform plus one state authority — all content types processed the same way, with a single hub orchestrating the whole lifecycle from ingestion to moderation to going live or coming down. And the reason I bring this one up for this role specifically: most of what made it hard wasn't the code, it was that the pipeline crossed four teams and the person at the end of it was a creator who just wanted to know what happened to their video. Getting those two ends to line up is the part I want to keep doing.

### Questions to ask

- "What's the biggest technical challenge the team is focused on over the next couple of quarters?"
- "What does growth look like for engineers here — how do people go from owning features to owning systems?"

---

## Part 4 — Customer-facing BQ (SE-specific)

Adapted from `../Amazon/amazon_bq.md`. Same true events, re-cut for a Solutions Engineer loop:
less implementation detail, more stakeholder, communication, and judgment.

**Ratio for this loop:** S+T ≈ 25%, Action ≈ 50%, Result + takeaway ≈ 25%. In the Action, at
least one sentence must name *a person or team you talked to and what you said to them*. That
sentence is the one being graded.

### Q1. Tell me about a time you solved a pain point for a customer.
**Source story:** Amazon Story C — upload status ownership. **Reuse in TikTok script:** Main story 2.

> **(S)** Back in June we were getting a steady stream of support tickets about video uploads. Creators would upload something and it would sit on "processing" forever with no explanation, or show a failure with no reason attached. And because all of that status lived on the phone, if someone switched devices it all just disappeared.
>
> **(T)** When I dug into the tickets, the pattern was that none of them were really about uploads being broken. They were about people not being able to tell what had happened to their own video. The server never owned that state, so there was no single answer to give them.
>
> **(A)** I traced the whole path — client, our server, the media platform service that actually holds the state, and the review teams feeding results in over Kafka. The fix I pushed for was making the server the one place that owns this and translates our internal states into something a creator can actually read. That last part mattered more than it sounds: internally we have a processing state and a separate moderation state, and I had to sit down with the review team to agree on how their verdicts should surface to a user — because "rejected by moderation" and "transcoding failed" are the same word to a creator, but you have to tell them different things. I also kept it a live read with no cache, since a stale status on that page is exactly the thing that generates a ticket.
>
> **(R)** After it shipped, instead of a video hanging on "processing," people got a real status with an actual reason, on any device they logged into. The tickets stopped. What stuck with me was that the real problem was never technical — it was that users couldn't trust what they were seeing, and the engineering work was just what it took to make the screen honest.

---

### Q2. Tell me about a time you explained something technical to a non-technical audience.
**Source story:** Amazon Story A — anti-abuse false positives, re-cut for the communication angle.
⚠️ Real event; the internal framing was a lapse (found it after the fact, not via monitoring).
Do **not** upgrade "found it passively" into "caught it with proactive monitoring."

> **(S)** I built an anti-abuse middleware for video uploads that flagged traffic based on upload frequency and IP behavior. After it went live, it turned out to also be catching legitimate internal script traffic — real requests were getting blocked.
>
> **(T)** The people affected weren't engineers on my team. They were internal folks whose workflows suddenly broke, and what they knew was "my thing stopped working." I had to explain what was happening and then get agreement on what to do about it.
>
> **(A)** I stayed away from the rule mechanics entirely. What I told them was: our system is trying to spot people abusing uploads, one of the signals is how fast requests come in, and your scripts are fast enough that they look the same to us — so this is us mis-reading you, not you doing anything wrong. Framing it as our bug rather than their misconfiguration mattered, because otherwise the conversation becomes an argument about whose fault it is. Then I gave them a concrete decision instead of an open question: I could either carve out an exception for them, which is fragile and I'd have to maintain forever, or I could stop blocking at the entry point and just collect the signals for the downstream team to act on. I recommended the second and said why — a false positive on a real user costs us more than letting some abuse through for a bit.
>
> **(R)** They agreed on the spot, false positives went to zero, and because the middleware kept collecting signals the content-platform team still identified the actual bad IPs and blocked those precisely. Honestly, the thing I took from it was about the conversation more than the fix: leading with "this is us misreading you" got us to a decision in one exchange, where leading with the detection logic would have turned it into a debate.

---

### Q3. Tell me about a time you had to say no to a request, or push back on a stakeholder.
**Source story:** Amazon Story A (same event, push-back framing) — see caution above.
**Do not reuse Q2 and Q3 in the same interview.** Pick whichever the wording fits.

> **(S)** Same anti-abuse system. Once people knew the middleware could block traffic, I got asked to tighten it — turn on hard blocking and push the thresholds down, because we were still seeing abuse getting through.
>
> **(T)** The request was reasonable on its face. But I'd just come off an incident where that exact system blocked legitimate traffic, and I didn't have evidence the thresholds were safe. So I had to push back on something my own system was built to do.
>
> **(A)** I didn't just say no. I said what I could commit to and what I'd need first. The argument I made was about asymmetry: if we let an abusive upload through, we catch it downstream at review and take it down, so the cost is a delay. If we block a real creator, they get no explanation, they don't retry, and we never find out. Those aren't the same size mistake. So what I proposed was to ship in observe-only mode, collect on real traffic, confirm the thresholds wouldn't catch legitimate creators, and only then talk about enforcement. I gave a clear condition for yes rather than leaving it as a refusal.
>
> **(R)** We went with observe-first, and the signals we collected were what let the downstream team blacklist the real abusers precisely — so we got the enforcement outcome without gambling on the thresholds. That's been a habit ever since: I don't ship an interception rule in blocking mode on day one, I ship it watching first.

---

### Q4. Tell me about a time you debugged a hard problem where the symptoms were misleading.
**Source story:** Amazon Story B — gas-stations memory bomb.
**Scope note:** keep it server-side. Do NOT bring in the LB-amplification angle or client-side bounds.

> **(S)** For about a week our main server cluster kept going unhealthy — five-hundreds spiking, requests timing out, and mongo and memcache alerts firing alongside it. At the worst point more than 70% of requests to that cluster were failing, so it looked like three separate systems were falling over at the same time.
>
> **(T)** I was on call. What made it hard was that it had already happened a couple of times that week and nobody had found the cause, so it kept coming back. Restarting pods made it go away every time, which is exactly why it kept happening.
>
> **(A)** I worked it off the dashboards and logs rather than guessing. The first thing that narrowed it down was that several pods were swelling at once — pods don't die together on their own, so something external was hitting them. The usual dashboards couldn't tell me what, so I brought in flame-graph profiling, and that pointed straight at one endpoint: a gas-stations lookup whose database query had no limit on it. A request with a big enough geographic area pulled tens of thousands of documents in a single call and blew up the pod's memory. The part I was most pleased with was realizing the mongo and memcache alerts weren't separate failures at all — when pods died, their traffic retried onto the healthy ones, and that overload tipped the other systems over. One cause, three sets of alarms.
>
> **(R)** I added a limit and a bounds guard, and something that had recurred four times in a week stopped completely. The counterintuitive bit was the volume: this wasn't a traffic flood, it was a handful of requests. For that class of problem you have to look at the size of a single request, not the request count — which is the opposite of where the dashboards point you.

---

### Q5. Tell me about a time you delivered something without breaking existing consumers.
**Source story:** Amazon Story F — proto migration. **Most on-point story for the API half of this JD.**

> **(S)** My main project this year was migrating our JSON APIs onto a Protobuf contract — over 40 endpoints across user, profile, content, and a few other areas.
>
> **(T)** The hard constraint was that the clients consuming those APIs hadn't all migrated. So this wasn't really a migration project, it was a compatibility project: the new output had to reproduce the old behavior field for field, because anything I dropped would break somebody I couldn't see.
>
> **(A)** Rather than trade speed for safety, I built the verification that gave me both. On staging I ran the new path and the old path side by side on the same request and auto-diffed the responses, with a control group so normal data drift didn't look like a regression. Then I rolled each endpoint out behind a gate instead of flipping everything at once. The judgment call was in triaging the diffs — I didn't fix all of them. A field the old path returned and the new one dropped was a must-fix, because a consumer depending on that key breaks when it disappears. Extra fields I could safely leave, since a consumer can ignore a key it doesn't know. That asymmetry is what let me move fast on the ones that didn't matter.
>
> **(R)** All 40-plus endpoints shipped on schedule, around 120 PRs, with no client-breaking regressions from the rollout. The thing I'd carry into a role like this one is that when you don't control the consumers, "it works" isn't the bar — you need a way to prove you didn't break anyone, before they find out for you.

---

### Q6. A customer reports a bug that turns out to be their own misconfiguration. How do you handle it?
**No source story — this is a hypothetical, answer with method plus a real analogue.**

> I try hard not to lead with "that's on your side," even when it is — the moment it sounds like blame, you stop getting information from them.
>
> So I'd confirm the behavior first and get specific: a request ID, a timestamp, the exact payload they sent. Then I'd reproduce it from our side so I'm describing something I've seen rather than something I'm inferring. When I find it's a config issue, I'd frame it around what to change rather than who was wrong — here's the call you made, here's what our side expected, here's the fix.
>
> And then the part I think actually matters: I'd ask why it was possible to get into that state. If a customer misconfigured something, usually the error we returned was too vague, or the docs were ambiguous, or the API let them do something that was never valid. That's our problem, not theirs. The upload work I did came out of exactly that — users kept filing tickets because a failure came back with no reason attached, and once the server started returning the actual cause, the tickets stopped. So my instinct with a misconfiguration is to fix the individual customer fast, then go change whatever let it happen quietly.

---

### Q7. What do you know about how ads work? / Why ads?
**Honesty red line: you have no ads background. Don't fake it.** The answer is adjacency plus
genuine interest, delivered without apology. See `tiktok_ads_domain.md` if written — otherwise
at minimum know: campaign → ad group → creative; CPM/CPC/oCPM; attribution window;
click-through vs view-through; conversion API / server-side event tracking; pixel.

> I'll be straight with you — I haven't worked on ads systems directly, so I'm not going to pretend to depth I don't have.
>
> What I have done maps pretty closely, though. The premium subscription work was money-critical: payment notifications arriving out of order, duplicated, sometimes lost, and I had to make sure that the entitlement state at the end was correct, because getting it wrong means charging someone who cancelled. That's the same shape as conversion and billing correctness in ads — events arriving unreliably from an external platform, and the number at the end has to be defensible.
>
> And the API migration work is the integration side of it: making sure external consumers don't break when the contract underneath changes. From what I understand that's a lot of what this role deals with — advertisers integrating against our APIs and something not lining up.
>
> The domain vocabulary I'd need to pick up, and I'd expect to spend the first couple of months doing that. But the underlying problems — external systems, unreliable events, numbers that have to be right, customers who need an explanation — those I've done.

---

### Q8. Tell me about a time you worked with a team you had no authority over.
**Source: real cross-team work — review teams (upload), SRE (incidents), platform team (anti-abuse).**
Lower-detail than the others by design; treat as backup if Q1–Q5 are exhausted.

> The upload status work is the clearest one. The state I needed lived in another team's service, and the moderation verdicts came from a third team, so I couldn't ship any of it alone and none of them reported to me.
>
> What worked was showing up with the problem framed in their terms rather than mine. With the review team, I didn't ask them to change anything — I asked how they wanted their verdicts represented to a creator, because they cared about that and it was genuinely their call. With the platform team, the ask was concrete and small enough to say yes to. I did the tracing work up front so nobody had to go figure out their own piece of the picture.
>
> All of it landed, and I've kept the habit since: do the homework before the ask, and make the ask specific. "Can you help with this" gets deprioritized. "Can you confirm this one field means what I think it means" gets answered same day.

---

### Coverage map

| Likely SE question | Use | Backup |
|---|---|---|
| Customer pain / customer obsession | Q1 upload status | Q6 method |
| Explain technical to non-technical | Q2 anti-abuse | Q1 status translation |
| Push back / say no | Q3 anti-abuse | — |
| Hard debugging / root cause | Q4 gas-stations | Q5 proto diffs |
| API / not breaking consumers | Q5 proto migration | — |
| Customer at fault | Q6 method + upload analogue | — |
| Ads domain | Q7 honest adjacency | — |
| Cross-team, no authority | Q8 | Q1 |

**Story reuse warning.** Q2 and Q3 are the same underlying event. Q1 and Q8 overlap on the
upload work. In a single loop, use each event once — if you've spent anti-abuse on "explain to
non-technical," answer a push-back question from the proto triage decision instead (choosing not
to fix drifting fields, and defending that call).

### Questions to ask (SE round)

- "When an advertiser hits an integration problem, what does the path from their report to a fix usually look like?"
- "How much of the role is reactive support versus building tooling so the same problem doesn't come back?"
- "What separates someone doing well in this role at six months from someone struggling?"
