# Amazon SDE I — Behavioral Questions Prep

> Structure:
> - **Part 1 — Q&A**: interview question + spoken-style English answer (ready to say in a real interview). Each answer links to a story in Part 2.
> - **Part 2 — Story bank**: one detailed version per story (full STAR + quantified data + follow-up ammo), reusable across questions.
> - One story can be referenced by multiple questions; Q and Story are linked via Markdown internal links.

---

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改本文件答案时遵守：

1. **语言**：文件保存内容全英文；跟用户对话用中文。
2. **STAR 比例**：S+T ≈ 20%，Action+Result ≈ 80%。症状归 Situation，根因归 Task，两段不重复。
3. **Action 颗粒度**：讲"多层"（链路广度 / 关键决策），不讲"每层实现细节"。把实现细节（如 map state into client status）抬升成"设计决策 / 意图"来讲；纯实现细节下沉到 follow-up 弹药。判断标准：这句去掉后，面试官是否就不知道你做了一个什么关键决定？是→留，否→删或压半句。
4. **收尾**：每题 Result 后配一句 takeaway，且**点题回对应 LP**（Customer Obsession→落在 user/customer；Ownership→落在主动扛下；Dive Deep→落在不停表象/挖根因）。点题要自然带出，禁止生硬喊 LP 名。收尾开头话术 + 收尾类型要在相邻题之间**轮换**，避免像背模板。收尾类型三选一：① takeaway 点题 ② 落在有力/量化的结果画面上 ③ 交回控制权（"Happy to go deeper on any part."）。开头话术库：The takeaway for me was… / For me, the lesson was… / What stuck with me was… / Looking back… / Ever since then, I… / Honestly, that's why I now always…。
5. **默认无 follow-up**：一段 STAR 要自己把 4 要素说全说满；follow-up 弹药单独存，被追问才用。
6. **真实性红线**：只讲用户真做过的事；技术细节以代码 / git 核实为准，不编机制、不编数字；归属存疑的动作用弱化措辞（陈述事实而非揽成"我做的"）。用户说"能讲所有细节"的部分尊重其判断，但仍在 follow-up 区标注深挖风险点。
7. **用词口语化**：偏书面的词（surface / the moment）给口语替代（show up as / as soon as）；生僻词附替代方案。

---

## Part 1 — Q&A

### Q1. Tell me about a time you solved a pain point for a user/customer.
**LP:** Customer Obsession
**Story used:** → [Story C — Moving video upload status ownership to the server](#story-c--moving-video-upload-status-ownership-to-the-server)

> **(Situation)** In June, we were getting support tickets about video uploads. Videos would be stuck on "processing" forever with no explanation, or show a "failure" with no reason. And because all that state lived on the client, switching phones cleared everything — in-progress, failed, even duplicates all just disappeared.
>
> **(Task)** When I looked into it, the root cause was that the server never owned this state — it all lived on the client, so there was no single source of truth. So I set out to make the server the source of truth for the whole upload state.
>
> **(Action)** To do that, I traced the whole pipeline end to end — the client, the server, mp-api (our media platform service), and the downstream review teams. The review teams push their results over Kafka into mp-api, which persists the real, final state in the database. On the server side, I made it the single place that turns those internal states into clear, client-facing statuses, so the client never deals with our internal state machine. And I kept it a real-time read: every time the client requests, the server pulls the current state straight from mp-api, with no caching in between, so the state is always live and consistent across devices. I drove this design end to end across both services.
>
> **(Result)** After the change, users stopped getting stuck — instead of a video hanging on "processing," the server returns a real "failure" with a reason as soon as review doesn't pass. And because the state lives on the server instead of the device, users see their uploads on any phone they log into. The support tickets went away — because the real pain was never technical, it was that users couldn't trust what they saw, and now they can.

---

### Q2. Tell me about a time you had to balance customer experience against another goal. / a time you pushed back to protect users.
**LP:** Customer Obsession (trade-off / push-back angle)
**Story used:** → [Story A — UGC anti-abuse false-positive fix](#story-a--ugc-anti-abuse-false-positive-fix)

> **(Situation)** I built an anti-abuse system for UGC video uploads — a middleware that blocked abusive upload traffic based on patterns like frequency and IP behavior. The goal was to stop bots and script abuse.
>
> **(Task)** After it went live, I found it was also catching legitimate traffic from internal scripts — real, valid requests were getting blocked. So I had a tension: the whole point of the system was to block aggressively, but blocking real users was the worse outcome.
>
> **(Action)** I made the call to put the user experience first — I'd rather let some abuse through temporarily than block a single legitimate request. So instead of keeping the hard block, I redesigned it into a collect-only mode: it stops blocking at the entry point and just gathers the abuse signals for the downstream team to act on. That was a deliberate trade-off — I gave up immediate blocking to protect legitimate users.
>
> **(Result)** False positives went to zero, legitimate traffic flowed normally again, and because the middleware kept collecting signals, the downstream team could still identify and blacklist the real abusers precisely. So we protected real users without giving up on catching abuse — which to me is the whole point: when protecting users conflicts with a system's own goal, the user has to win.

---

### Q3. Tell me about a time you had to dig through several layers to find the root cause of a problem. / Tell me about the most complex technical problem you solved.
**LP:** Dive Deep (also covers Ownership)
**Story used:** → [Story B — gas-stations memory-bomb root-cause investigation](#story-b--gas-stations-memory-bomb-root-cause-investigation)

> **(Situation)** Over about a week, our main server cluster kept going unhealthy — a spike in five-hundreds, requests timing out, plus mongo and memcache alerts firing. At peak, over 70% of requests to the cluster were failing, so on the surface it looked like several different systems were all falling over at once.
>
> **(Task)** I was on call. The tricky part was this had already happened a couple of times earlier that week without anyone finding the root cause, so it kept recurring. I took it on to actually get to the bottom of it, not just restart the pods and move on.
>
> **(Action)** I worked it from our metrics dashboards and logs. Several pods were swelling at once — and pods don't die together on their own, so something external was hitting them. When the usual dashboards couldn't pin down what, I brought in flame-graph profiling, and it pointed straight at one endpoint's handler — a gas-stations lookup whose mongo query had no limit, so a request with a huge geographic bounds pulled tens of thousands of documents in one call and blew up the pod. The part I'm most proud of was connecting the rest of the noise to that same cause — the mongo and memcache alerts weren't separate problems, they were healthy pods getting overloaded when traffic shifted off the dying ones. Once I had it, I added a limit and a bounds guard on that query.
>
> **(Result)** That fix ended something that had been recurring for over a week — four separate storms that took most of the cluster down. Once the limit went in, it never happened again — because restarting the pods would've "worked" every time, but the only way to stop it for good was to keep digging until I hit the one line that actually caused it.

---

### Q4. Tell me about a time you took ownership of something outside the scope of your role. / a time you took on something nobody was handling.
**LP:** Ownership
**Story used:** → [Story D — Tesla scheduler database redesign](#story-d--tesla-scheduler-database-redesign)

> **(Situation)** During my Tesla internship, I took over a full-stack internal tool that let engineers schedule vehicle tests. While maintaining it, I noticed the database was badly designed — a SQL database built with a NoSQL mindset, with tables that should've been foreign-keyed kept separate and some straight-up duplicated, which made queries slow and wasted space.
>
> **(Task)** My job was just to maintain the tool — nobody asked me to fix the database, and the person who designed it had already left — but I could see it would only get worse, so I took it upon myself to redesign it properly.
>
> **(Action)** I researched it end to end and wrote the technical plan myself. The hard part was doing it without disrupting the engineers using it live, so I designed an online migration instead of taking the tool down. I stood up the new schema, backfilled the historical data, and kept the old and new databases in sync both ways through the cutover, so I could roll back at any point. I updated the backend, validated everything in a test environment first, then watched it in production for a full week before retiring the old database.
>
> **(Result)** The redesign cut query latency and shrank the footprint significantly, so the same database could hold far more data going forward — all with zero disruption to the engineers using the tool.
>
> **(Takeaway — closes + ties back to Ownership; swap per wording, see Story D options)** What stuck with me was that no one told me to fix it, and the person who built it was gone — I could've just kept it running. But I treated it as mine, and that's really what ownership means to me.

---

### Q5. Tell me about a time you delivered under a tight deadline. / a time you had to push through obstacles to hit a goal.
**LP:** Deliver Results
**Story used:** → [Story F — Proto migration delivery](#story-f--proto-migration-delivery)

> **(Situation)** This year my main project was migrating our server's JSON APIs to a Protobuf contract — over 40 endpoints, across seven or so areas like user, profile, and content. The hard constraint was that clients hadn't all migrated yet, so I couldn't break a single live field — the proto output had to exactly replicate the old JSON behavior.
>
> **(Task)** We wanted a big batch done by end of Q2, so the pressure was doing it fast without breaking anything in production. And the details were nasty — tons of fields, lots of edge cases, plus proto-specific traps like zero-values getting silently dropped and Go embedded-struct fields not mapping cleanly.
>
> **(Action)** Instead of trading speed for safety, I built a validation setup that gave me both. In a staging environment, I ran the new proto path and the old path side by side and auto-diffed them, with a control group to filter out normal feed noise, so only real regressions showed up. I rolled each endpoint out gradually behind a gate instead of flipping everything at once. And when diffs came back, I triaged instead of fixing everything — a field the proto path dropped was a must-fix, while extra or drifting fields I could safely leave.
>
> **(Result)** All 40-plus endpoints migrated successfully and on schedule, around 120 proto PRs merged, with parity clean enough that replays parsed with zero errors. No client-breaking field regressions from the rollout.
>
> **(Takeaway — closes + ties back to Deliver Results; swap per wording, see Story F options)** What stuck with me was that "fast" and "safe" aren't really a trade-off if you engineer for both — the validation setup is what let me hit the deadline without gambling on production.

---

## Part 2 — Story bank

### Story C — Moving video upload status ownership to the server
**LPs covered:** Customer Obsession (primary), Ownership, Dive Deep (traced full pipeline), Earn Trust (cross-team with review teams)
**When:** 2026-06

**Situation**
Customer-support tickets kept coming in about video uploads. Users would upload a video and it would be stuck on "processing" forever with no explanation, or show a "failure" banner with no reason given. Worse, because all this status lived locally on the client, the moment a user switched phones everything vanished — in-progress uploads, failed ones, even videos flagged as duplicate all disappeared.

**Task**
The root cause was that the server never owned this state — it all lived on the client, so there was no single source of truth. Anytime something went wrong downstream, or the user switched devices, the state got stuck or lost. I set out to make the server the source of truth for the whole upload state.

**Action**
- Traced the whole pipeline end to end: client → Go server (client-facing BFF) → mp-api (media platform service, the state authority) → downstream review teams.
- The review teams push results over Kafka into mp-api, which persists the real, final state in its MySQL database (kept fresh by consuming those Kafka topics; cache invalidated on every state change).
- Made the server the single place that translates mp-api's internal states into clear, client-facing statuses — so the client never touches the internal state machine.
- Kept it a pure real-time read: every client request pulls the current state straight from mp-api with no caching in between, so state is always live and consistent across devices.
- Drove this design end to end across both services.

**Result**
- Users stopped getting stuck: instead of hanging on "processing," the server returns a real "failure" with a reason as soon as review doesn't pass.
- Because state now lives on the server, users see their uploads on any device they log into.
- The customer pain behind all those support tickets went away.

**Follow-up ammo**

*Who's the customer?* — End users uploading videos (creators). Pain surfaced through customer-support tickets.

*How does the server know when to read the status? (very likely follow-up)* — It doesn't actively read. It's pure pull: every time the client refreshes "My Videos," the server synchronously reads live state from mp-api, no cache. mp-api keeps its MySQL state current by consuming Kafka (CPP callback + audit result) and invalidating its cache on change, so any read returns the latest.

*Won't users stay stuck on "processing" forever?* — No. A backup job runs every 10 minutes and force-fails any upload stuck in POSTING for over 1 hour (reason "posting timeout"), and notifies the author.

*Why no cache on the server side?* — UGC status must be real-time. Caching sits in mp-api (10-min TTL + active invalidation on state change); the server deliberately reads through.

*State model (mp-api, two separate enums):*
- **PostState** (upload lifecycle, MySQL post table): POSTING=1 → processing, POSTED=2 → success, POST_FAILED=4 → failed, POST_DUP=9 → duplicate.
- **AuditStatus** (content review, separate): REJECT=3 → rejected.

*failed vs rejected (common follow-up):* — **failed** = technical/processing failure (transcode or CPP content pipeline failed), from PostState. **rejected** = content review didn't pass (judged by the review team), from AuditStatus. duplicate/failed come from the processing pipeline; rejected comes from the review team.

*Downstream review sources:* doughnut / bagel (review) + CPP (content pipeline), delivered to mp-api over Kafka (`audit_result_topic`, `mp_callback`).

*server vs mp-api split:* mp-api (Java) = state authority, persists to DB, consumes review Kafka. server (Go) = BFF, reads state and translates into client-facing statuses.

*How does the client get updates (e.g. processing → failed on screen)?* — That's client-side (polling or push); the server just returns a live snapshot per request. Server code doesn't reveal client refresh behavior — say "the client polls this endpoint" only if sure, otherwise "the server guarantees a live state on every request." Do NOT claim a server-push mechanism — the code has none.

**Takeaway options** (Customer Obsession — pick per wording; default is the first):
1. (default) "The takeaway for me was that the fix wasn't really about state management — it was about giving users a reliable, honest view of their own uploads."
2. (listening to customers) "For me the lesson was that those support tickets were the real signal — the fix just followed where the customer pain actually pointed."
3. (short) "Fix where the data lives, and the user experience fixes itself."

---

### Story D — Tesla scheduler database redesign
**LPs covered:** Ownership (primary), Dive Deep, Insist on the Highest Standards, Deliver Results
**When:** Tesla internship (~2 years ago — fine for Ownership; LP doesn't care about recency)

**Situation**
Took over a full-stack internal tool for engineers to schedule vehicle tests (Gantt-chart style, ~1000 internal users). While maintaining it, noticed the database was badly designed: a SQL database built with a NoSQL mindset — tables that should've been foreign-keyed were kept separate, some were duplicates (e.g. two tables where one table + a type column would do). Long-standing; caused high query latency and wasted space.

**Task**
My role was only to maintain the tool. Nobody asked me to fix the DB; the original designer had already left the team. I took it upon myself to redesign it — a short-term cost for a long-term fix.

**Action** (multi-layer, low-detail — details in follow-up)
- Researched it end to end and wrote the technical plan myself.
- Chose an online migration (not a shutdown) to avoid disrupting live users.
- Stood up new schema → backfilled historical data → kept old⇄new in two-way sync through cutover (enables rollback).
- Updated the backend to the new schema; validated in a test environment first; watched production a full week before retiring the old DB.

**Result**
- Cut query latency and shrank the data footprint significantly → same DB now holds far more data.
- Zero disruption to the engineers using the tool.

**Follow-up ammo**

*Migration mechanics (very likely follow-up):* old→new first (import historical data), then keep old→new syncing continuously; deploy backend + turn on reverse new→old sync at the same time; ran both directions ~a week, observed no discrepancies, then stopped the old DB. Two-way sync is what made rollback safe.

*How did you guarantee correctness before cutover?* Validated the whole thing in a test environment first.

*Rollback plan?* Code rollback — since old and new stayed in two-way sync, could switch back cleanly.

*Isn't the scale small (~1000 users, internal)?* Don't be defensive. Ownership isn't about scale — it's the mindset: unowned problem, original designer gone, I treated it as mine and did an online migration to protect live users. Small scale is why I could own it end to end solo.

*What was actually wrong with the design?* NoSQL-style modeling in SQL: missing foreign keys / normalization, duplicate tables that should've been one table + a type column. One line on this is enough — don't go table-by-table.

**Takeaway options** (Ownership — pick per wording; default is the first):
1. (default) "What stuck with me was that no one told me to fix it, and the person who built it was gone — I could've just kept it running. But I treated it as mine, and that's really what ownership means to me."
2. (long-term vs short-term) "The takeaway for me was that owning something means fixing it for the long term, even when the short-term easy path is to just leave it working."
3. (if asked "outside your role") "It wasn't my job on paper, but I've never really drawn the line at 'that's not my task' — if I can see a problem getting worse, I'd rather own it than wait for someone else to."
4. (short) "For me that's what ownership is — not waiting to be asked, and not stopping at what's technically my job."

---

### Story F — Proto migration delivery
**LPs covered:** Deliver Results (primary), Insist on the Highest Standards, Ownership
**When:** 2026, from mid-March (main project of H1)

**Situation**
Main project: migrating the server's JSON APIs to a Protobuf contract — 40+ endpoints across social/interact/user/profile/contents/push/channel. Hard constraint: clients hadn't all migrated, so proto output had to exactly replicate legacy JSON — zero broken live fields.

**Task**
Deliver a large batch by end of Q2. Tension = fast AND safe (touching live endpoints). Obstacles: many fields, many edge cases, proto traps (protojson drops zero-values; Go embedded-struct fields don't map cleanly).

**Action** (multi-layer, low-detail — mechanics in follow-up)
- Didn't trade speed for safety — built a validation setup to get both.
- Staging dual-path: ran proto path vs legacy path side by side, auto-diffed with scripts; a control group as noise baseline so real regressions stood out from feed drift.
- Gradual rollout behind a gate (AB) instead of flipping all at once.
- Triaged the diffs instead of fixing all: dropped-field (legacy has, proto doesn't) = must-fix; extra/drifting fields = safely leave.

**Result**
- 40+ endpoints migrated on schedule; ~120 proto PRs merged across two repos.
- Parity clean: replays parsed with zero errors; diffs driven down from hundreds to just drift-level noise. No client-breaking field regressions from rollout.

**Follow-up ammo**

*The validation method in detail:* dual-path on the same stage instance — same request sent with a header (`X-Stage-Migration-Path: proto` vs `json`) to hit new vs old code. Diff by docid to cancel feed-recall drift. A/A control (legacy vs legacy) gives the noise baseline; a field only counts as a real bug if proto-vs-legacy diff ≫ legacy-vs-legacy. Also stage-vs-prod to catch real production field shapes.

*The zero-value trap:* protojson defaults to EmitUnpopulated=false, so zero-values (`""`, `0`, `false`, `null`, empty slice) get dropped — but legacy `encoding/json` without omitempty always emits them. If a client depends on the key existing, dropping it is a breaking change. Fix: a custom `emit_zero` annotation (built by me) that forces the field to serialize even at zero.

*Why is "dropped field" the must-fix and not "extra field"?* Asymmetric risk: a consumer can ignore an extra key (safe), but a consumer depending on a key's existence breaks if it disappears — especially numeric/count/id fields where "0" vs "missing" matters.

*What did you build to make this repeatable?* Two protoext annotations — `flatten` (flatten nested structs in JSON / bind outer request fields to inner) and `emit_zero` — reusable across all later endpoint migrations.

*Scale / credit:* my own work — ~120 proto PRs under my name across server + api-schema; the migration was the H1 main line.

**Takeaway options** (Deliver Results — pick per wording; default is the first):
1. (default) "What stuck with me was that 'fast' and 'safe' aren't really a trade-off if you engineer for both — the validation setup is what let me hit the deadline without gambling on production."
2. (triage angle) "The takeaway for me was that hitting a deadline is as much about knowing what *not* to fix — triaging the real risks from the noise — as it is about working faster."
3. (if asked "tight deadline") "Under a real deadline, the instinct is to cut corners on either speed or safety. What I learned was to build the tooling that removes that choice."
4. (short) "For me, delivering under pressure isn't about rushing — it's about making 'fast' and 'safe' the same path."

---

### Story A — UGC anti-abuse false-positive fix
**LPs covered:** Customer Obsession (trade-off / push-back angle), Ownership, Invent and Simplify, Earn Trust (cross-team)
**When:** April 2026
**Used for:** Q2 (Customer Obsession — balance customer vs another goal / push back for users). Also usable for a "failure / lessons learned" question. ⚠️ Has a hidden downside (see caution note below).

**Situation**
Working on anti-abuse for UGC video uploads. I designed and built a three-layer fail-open middleware to catch abusive upload traffic, flagging based on patterns like upload frequency and IP behavior.

**Task**
Block abusers while not hurting legitimate users / internal flows.

**Action**
- After it went live, the rules mistakenly flagged internal crawler-script traffic as abuse (their traffic looked like high-frequency abuse), blocking legitimate requests.
- I decided that wrongly blocking legitimate users was worse than temporarily letting some abuse through, so I dropped the hard block.
- Redesigned the middleware from "block" mode into "collect-only, pass ip_info through" mode — collect the abuse signals at the entry point, don't block, hand off to downstream.
- Design → implementation → redesign, all my call.

**Result**
- Legitimate internal traffic stopped getting blocked (false positives to zero).
- The middleware kept collecting signals → the downstream content-platform team used that data to identify the real malicious crawler IPs → blocked them precisely with a blacklist. We actually caught the real abusers, with zero false positives on legitimate users.
- Personal habit change: ship interception rules in observe-only mode first, confirm no false positives, then tighten.

**Follow-up ammo / caution**
- "Who's the customer?" — internal teams whose scripts depend on the upload pipeline.
- "How did you find out?" — via log investigation / downstream feedback. If asked, say it in one line and pull back to the fix; don't claim credit for proactive monitoring.
- How the rule triggered: upload frequency + IP patterns; internal scripts were high-frequency so they hit the rule. One line, don't expand into three-layer detail.
- ⚠️ Internally this "false positive" was logged as a lapse (didn't check logs promptly after launch). Telling it from a positive angle is fine, but do NOT dress up "found it passively afterward" as "I caught it via proactive real-time monitoring" — it breaks under deep questioning.

**Takeaway options** (Customer Obsession, trade-off angle — pick per wording; default is the first):
1. (default) "What stuck with me was that when protecting users conflicts with a system's own goal, the user has to win — a false positive on a real user costs more than letting some abuse slip through for a bit."
2. (push-back framing) "I learned to push back even against my own system — the anti-abuse tool existed to block, but not at the cost of blocking real users."
3. (short) "A false positive on a real user is more expensive than a false negative on a bot."

---

### Story B — gas-stations memory-bomb root-cause investigation
**LPs covered:** Dive Deep (primary), Ownership, Deliver Results, Earn Trust (cross-team with SRE)
**When:** 2026-06-25 – 07-01 (five incidents, same root cause)
**Scope note:** Tell it as a single memory-bomb story. Do NOT bring in the LB-amplification angle (bad pod poisoning the whole upstream) — that's a separate topic and muddies the causal chain. Do NOT mention client-side bounds diagnosis — keep it entirely server-side.

**Situation**
For about a week, our main server cluster (internally: server-default) kept going unhealthy: a spike in five-hundreds, requests timing out, plus mongo and memcache alerts firing. At peak, over 70% of requests to the cluster were failing — so on the surface it looked like several unrelated systems were all falling over at the same time.

**Task**
I was on call. The hard part: this had already happened a couple of times earlier that week without anyone finding the root cause, so it kept recurring. I took it on to actually get to the bottom of it, not just restart pods and move on.

**Action** (multi-layer, low-detail — details live in follow-up)
- Worked it from metrics dashboards + logs (no guessing). (Real tools: Grafana/Mimir + ELK/Kibana — names only in follow-up.)
- Several pods swelling at once → pods don't die together on their own → something external was hitting them (narrowed the direction).
- Usual dashboards couldn't pin down what → brought in flame-graph profiling → it pointed straight at one endpoint's handler dominating CPU: a gas-stations lookup whose mongo query had no limit. A huge-bounds request pulled tens of thousands of docs in one call and blew up the pod.
- Connected the rest of the noise to the same cause: the mongo and memcache alerts weren't separate failures — dying pods' traffic retried onto healthy pods, and that overload tipped mongo/memcache over.
- Added a limit + bounds guard on the query.

**Result**
- Ended a problem that had recurred for over a week — four separate storms, each with 70%+ of the cluster failing. After the limit went in, it never happened again.

**Follow-up ammo**

*How exactly did you trace a symptom to a specific pod?* — Correlated request IDs between the LB access logs and the server logs (lb_access_logs `x_request_id` = server_logs `X-Request-ID`), which maps each individual request to the exact pod that swelled at that moment.

*How big was one request, really?* — Single doc ~13.9 KB (has a 24×7 busyness array); a continent-level bounds hits 50k–100k docs → 0.7–1.4 GB raw BSON → 2–7 GB heap after serialization copies. 07-01 broke the 16.8 GB limit and OOM-killed 4 pods.

*It was a huge traffic flood, right?* — No, the opposite. Only a handful of requests each time; across the week there were ~20 occurrences and 14 went completely unnoticed. The insight: for this kind of issue you look at the size of a single request, not the request count.

*Why did the pod freeze instead of just being slow?* — Heap growing GB/second triggers Go's GC-assist, which stalls all goroutines on the pod — so every endpoint on that pod times out, not just gas-stations.

*What I built vs. what others suggested:* I did the root-cause investigation, added the limit + bounds guard, and set up the flame-graph profiling (SRE suggested adding profiling). Framing per user's decision.

*How did you rule out other same-week incidents?* — Different signatures: a 6/29 comment-mongos OOM and 6/30 memcache churn had different fingerprints; this one was specifically the per-request heap bomb from the unbounded gas query.

**Takeaway options** (Dive Deep — pick per the interviewer's exact wording; default is the first):
1. (default) "What stuck with me is that the surface almost never points at the root — restarting the pods would've 'worked' every time but taught me nothing. The only way to stop it for good was to keep digging until I hit the one line that actually caused it."
2. (if asked "most complex") "It looked complex — five alerts across three systems — but it was really one root cause. The complexity was on the surface, not underneath."
3. (if asked "root cause") "Only fixing the actual root stopped it for good — anything short of that just reset the clock until the next storm."
4. (short) "The surface kept lying. You have to chase it all the way down."
