# TikTok Interview Script (English)

The interviewer may speak Chinese or English. This file mirrors `tiktok_bq_ch.md` one-to-one.

---

## Part 1 — HR Screen

### Self-intro

Hi, I'm Xinyao. I did my master's degree at UIUC and graduated last December. I'm working at NewsBreak since February as a backend engineer on the server team. NewsBreak is an app that focused on the local news for American users.
At NewsBreak, I worked on three main projects. I migrated core APIs from JSON to Protobuf so the client and server could share a clear schema. I also owned the UGC video upload pipeline, including a rate limiter and making the upload status more reliable for users. And I built the backend for our Premium subscription system, like the purchase APIs, subscription state machine, the notification handler and premium access. I also worked on infra and deployment, and took part in the on-call rotation.
Before NewsBreak, I had two internships at ByteDance and Tesla.
I have a strong foundation in backend development and distributed systems, along with hands-on experience building AI agents. I've also worked on production systems and handled real engineering challenges.

### Why are you looking for a new opportunity

I've learned a lot at Newsbreak and built a few backend projects there. What I want next is to do user-facing work on a much bigger and more complex system, and that's why I'm interested in TikTok. It's an innovative, fast-growing company, so I think I'd have a lot more room to grow.

### Career goals

My direction is pretty clear — I want to go deep on backend and large-scale distributed systems. I'm not looking to switch tracks.

Everything I've done this past year converges on one thing. The state consistency in the subscription system, the cross-system state orchestration in video upload, the Protobuf migration — underneath, they're all about how you keep multiple systems correct with respect to each other. That's the class of problem I find most interesting, and the one I have the best instincts for.

Going forward I want to keep doing this somewhere with a higher engineering bar, and grow from owning one pipeline to being responsible for a wider set of technical decisions. That's exactly why I want to join TT — the problems are hard enough and the team is strong enough that I can do this long term and keep learning.

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

**Note:** there are two parallel TikTok pipelines — the NG *Backend Software Engineer Graduate (Global E-commerce) - 2027 Start* (status: Evaluation Passed) and this one. Disclose the other pipeline to the recruiter rather than letting them find it.

### Questions to ask

- "You mentioned the team mainly works on XX — for this role, what would the first six months likely focus on?"
- "What do the next steps in the process look like?"

---

## Part 2 — Coding Round

### Self-intro

Hi, I'm Xinyao. I did my master's degree at UIUC and graduated last December. I'm working at NewsBreak since February as a backend engineer on the server team. NewsBreak is an app that focused on the local news for American users.
At NewsBreak, I worked on three main projects. I migrated core APIs from JSON to Protobuf so the client and server could share a clear schema. I also owned the UGC video upload pipeline, including a rate limiter and making the upload status more reliable for users. And I built the backend for our Premium subscription system, like the purchase APIs, subscription state machine, the notification handler and premium access. I also worked on infra and deployment, and took part in the on-call rotation.
Before NewsBreak, I had two internships at ByteDance and Tesla.
I have a strong foundation in backend development and distributed systems, along with hands-on experience building AI agents. I've also worked on production systems and handled real engineering challenges.

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

- I’d love to learn a little more about your team. What’s an interesting technical challenge you’ve worked on recently?
- And how do new grads typically get involved in projects like that?
- What does the onboarding process look like on your team?

- Thanks for sharing! I appreciate you giving me some context about the team.
- Thanks for sharing! I’d be excited to work on projects like that.
---

## Part 3 — HM Round

### Self-intro

Hi, I'm Xinyao. I did my master's degree at UIUC and graduated last December. I'm working at NewsBreak since February as a backend engineer on the server team. NewsBreak is an app that focused on the local news for American users.
At NewsBreak, I worked on three main projects. I migrated core APIs from JSON to Protobuf so the client and server could share a clear schema. I also owned the UGC video upload pipeline, including a rate limiter and making the upload status more reliable for users. And I built the backend for our Premium subscription system, like the purchase APIs, subscription state machine, the notification handler and premium access. I also worked on infra and deployment, and took part in the on-call rotation.
Before NewsBreak, I had two internships at ByteDance and Tesla.
I have a strong foundation in backend development and distributed systems, along with hands-on experience building AI agents. I've also worked on production systems and handled real engineering challenges.

### Main story 1 — Premium subscription system

I built the backend for our Premium subscription system at NewsBreak. The goal was to offer premium features and make sure subscribers got the access they paid for. I owned the purchase APIs, the subscription state machine, and Apple's notification handler.

I wrote two APIs for the purchase flow. Before payment, a prepare API linked our user account to an app account token. After payment, the app called subscribe API with Apple's signed payload. The backend verified it and granted Premium access. Apple's webhook then handled subscription lifecycle events, including renewals, cancellations, and billing failures.

I built a state machine and stored each user's subscription state in MongoDB. I tracked two separate status: the subscription status and whether the user had Premium access. For example, canceling auto-renewal doesn't immediately remove Premium access because the user has already paid for the current period. And a user in a billing grace period also keeps access, but will lose it if the grace period ends and payment still hasn't recovered.

The hardest part was handling concurrent, out-of-order, and duplicate notifications. For example, a renewal could fail and then recover. If the recovery notification arrived first and the failure notification arrived later, processing the delayed failure notification could incorrectly remove the user's access.

So first I used a Redis lock to serialize processing and a MongoDB version check to prevent concurrency conflict. To handle out-of-order notifications, I stored a timestamp watermark in MongoDB. It's like for the same transaction ID, I rejected notifications with an older event time. When the transaction IDs were different, I called Apple's subscription status API inside the same lock to check which one was current.

For idempotency, I used the notification ID as a Redis key to prevent processing the same notification again.

One mistake I made was initially comparing our server's write time against the record's `updated_time` to decide whether an update was newer. But a delayed notification would get a later timestamp and could overwrite a newer subscription state. I traced the notification logs, switched to notification event time, and tested delayed and reordered notifications. I also checked the affected subscriptions against Apple and repaired their local states.

By the third month after launch, the app had over 2,300 paid subscribers and generated about $190K in annual revenue. We also received positive feedback from users who said they were happy with the Premium experience.

#### Follow-up: If you built it again, what would you change?

I would consider removing the Redis lock and adding the idempotency key into the user status record. I would keep the MongoDB version check we already had.

That would let me save the state, watermark, and idempotency key in one atomic update. If the version check failed, the worker would reread the record and reconsider the notification.

The benefit would be fewer Redis operations and no gap between saving the state and saving the idempotency key. The trade-off would be handling concurrent requests through version conflicts and retries instead of serializing them upfront.

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
