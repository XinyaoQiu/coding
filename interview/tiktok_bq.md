# TikTok — Behavioral Questions Prep

> Structure:
> - **Part 1 — Q&A**: interview question + spoken-style English answer (ready to say in a real interview). Each answer links to a story in Part 2.
> - **Part 2 — Story bank**: one detailed version per story (full STAR + quantified data + deep follow-up ammo), reusable across questions.
> - One story can be referenced by multiple questions; Q and Story are linked via Markdown internal links.

---

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改本文件答案时遵守：

1. **语言**：文件保存内容全英文；跟用户对话用中文。
2. **STAR 比例**：S+T ≈ 20%，Action+Result ≈ 80%。症状归 Situation，根因归 Task，两段不重复。
3. **⚠️ TikTok 特点 —— 挖细节（本文件与 amazon_bq.md 的核心区别）**：TikTok BQ 面试官追问极深，看重**具体技术细节 + 量化数据 + 决策链条**，不满足于高层叙事。因此：
   - **Follow-up ammo 区必须做厚做深**：每个 story 的 follow-up 区是重点，覆盖「机制细节 / 具体数字 / 为什么这样选而不那样选 / 边界与失败模式 / 我做的 vs 别人做的」。宁可 follow-up 过量也别不够。
   - **量化优先**：能给数字的地方一律给数字（QPS、延迟、pod 数、内存、请求量、时间跨度、影响用户数、PR 数）。数字必须以代码/git/监控核实为准，不编。
   - **主叙事仍保持颗粒度克制**：Action 讲「多层链路 + 关键决策」，不讲每层实现细节；实现细节全部下沉到 follow-up 区，被追问才放。判断标准同 amazon：这句去掉后面试官是否就不知道你做了什么关键决定？是→留，否→下沉。
4. **收尾**：每题 Result 后配一句 takeaway，自然收束。收尾类型三选一并在相邻题间轮换：① takeaway 点题 ② 落在有力/量化的结果画面上 ③ 交回控制权（"Happy to go deeper on any part."）。开头话术库：The takeaway for me was… / For me, the lesson was… / What stuck with me was… / Looking back… / Ever since then, I…。避免像背模板。
5. **默认无 follow-up**：主叙事的一段 STAR 要自己把 4 要素说全说满；follow-up 弹药单独存，被追问才用。
6. **用词口语化**：偏书面的词给口语替代（surface → show up as；the moment → as soon as）；生僻词附替代方案。
7. **主题分类而非 LP**：TikTok 没有 Amazon 的 16 条 Leadership Principles。用 TikTok 常见 BQ 主题给每题打标签（见下方主题清单），不套 Amazon LP 名。
8. **收录范围**：只写用户明确要准备的题/故事，不擅自加题。素材来源由另一个 session 从 project 更新填入。

### TikTok BQ 常见主题清单（给每题 / 每 story 打标签用）

- **Impact / Ownership** —— 主动扛下、把事做成、可量化的业务影响。
- **Technical depth / Problem solving** —— 复杂技术问题、根因排查、系统设计取舍。
- **Conflict / Collaboration / Cross-team** —— 跨团队协作、意见分歧、如何对齐。
- **Ambiguity / Fast pace** —— 需求不清 / 快节奏下如何推进（TikTok 强调 speed）。
- **Failure / Learning / Growth** —— 失败、错误、复盘与改进。
- **Prioritization / Trade-off** —— 资源/时间有限下的取舍。
- **User / Product focus** —— 从用户或产品价值出发做决策。

> 注：一个 story 可覆盖多个主题；打标签时列主 + 次。

---

## Part 0 — Openers / intros (自我介绍 + HR screen 开场稿)

> 开场/screen 类口语稿总节。三块：0A coding 轮简短自我介绍、0B HR/recruiter 面 background+goals、0C coding 轮项目深度 opener（被问"讲个项目"时用）。

### 0A — Coding-round self-intro (~45–60s)

> coding 面最开始的自我介绍：high-level 覆盖全背景（Newsbreak 三件事 + oncall + 两段实习 + 学历），各一句不展开，结尾主动交回控制权进题。用词偏好：oncall 句用 "digging into production alerts and finding the root cause of issues"（不用更随意的说法）；oncall 明确挂在 Newsbreak（"on-call rotation there"），不是实习。

> "Hey, thanks — happy to be here. So I'm a backend engineer. I graduated from UIUC last December, and since then I've been at Newsbreak, which is a large news app, working mostly on the backend.
>
> A few things I've worked on there — I own a good chunk of our premium subscription system, I've worked on our UGC video-upload pipeline — both an anti-abuse layer that filters out bad upload traffic, and fixing the upload-status side so users get a clear, reliable state instead of videos getting stuck on "processing" — and my main project this year has been migrating our APIs from JSON over to Protobuf, which was a lot of endpoints without breaking anything live. I'm also on the on-call rotation there, so I'm pretty comfortable digging into production alerts and finding the root cause of issues.
>
> Before Newsbreak I interned at Alibaba Cloud and at Tesla, both full-stack — the Alibaba one got me into large-scale systems and reliability, and at Tesla I was building internal tools.
>
> Mostly I like backend work where correctness really matters. Happy to jump into the problem whenever you're ready."

---

### 0B — HR / recruiter screen (professional background + career goals)

> HR 面考表达清晰 + 动机真诚 + 跟岗位对得上，不深挖技术。背景口径同 0A，区别是结尾落到 career goals + why TikTok。career goals 路线定位=「深耕后端 + 大规模系统」（专注有方向感），不是转方向。核心三点讲到即可：①我是谁/做过什么 ②想往哪走(backend深耕+大规模) ③why TikTok(scale对上)。真诚 > 流利。

**[Professional background]**

> "Sure. I'm a backend engineer — I graduated from UIUC last December, and I've been at Newsbreak, a large news app, since then, working on the backend.
>
> The work I do there is pretty core backend — I own a good part of our premium subscription system, I've worked on our video-upload pipeline — both anti-abuse and making the upload status reliable for users — and my main project this year has been migrating our APIs to Protobuf across a lot of endpoints without breaking anything in production. I'm also on the on-call rotation, so I spend a fair amount of time digging into production issues and finding root causes. Before Newsbreak I interned at Alibaba Cloud, which got me into large-scale systems and reliability, and at Tesla building internal tools."

**[Career goals — 必须跟 TikTok scale 挂钩]**

> "As for where I want to go — I want to keep growing as a backend engineer and go deeper on large-scale, high-traffic systems. What I've realized I enjoy most is the work where correctness and reliability really matter — the failure modes, the edge cases, the on-call side of things — and I want to be somewhere the scale actually forces you to get those things right. That's honestly a big part of why I'm interested in TikTok — the scale here is a different level, and I'd like to grow into an engineer who can own and design systems at that scale, not just work within them."

**Delivery notes (internal, don't say out loud):**
- 若追问「为什么离开 Newsbreak / 为什么现在跳」（HR 电话可能口头再问，即使邮件已答过）：**只用 pull factor，绝不用 push factor**。红线——不说「现在工作没 impact」「正在积极找」（Applied 那次的说法，不要重复）。要说：喜欢现在的团队，但 TikTok LIVE 团队做的**全球内容治理 + 高并发底层工具**正是我技术热情的终极所在；可顺带提想去大公司。英文口语版：
  > "Honestly, I really like my current team — this isn't me running from anything. It's more that what your LIVE team works on — global content governance and the high-concurrency infrastructure behind it — is exactly where my technical passion is. And I'd like to grow at a company operating at that kind of scale."

**[Work authorization — HR 问到时主动交代]**

> 身份=F-1 OPT first year（STEM 专业）。策略：先讲 runway（现在能合法工作 + STEM extension 约 3 年），再讲未来需要 sponsorship。别只说"需要 H1B"（听起来像马上要）。

> "Sure — I'm currently on F-1 OPT, and I just started, so I'm authorized to work right now. My degree is STEM, so I'm also eligible for the STEM extension, which gives me around three years total on OPT. Down the line I would need H1B sponsorship — I just want to be upfront about that early."

**[Questions to ask back — HR 轮结尾反问]**

> HR 通常开头已介绍岗位/team，反问别再问"team 做什么"（显得没听）。顺着她的介绍往下挖 or 问流程。

> "You gave a great overview of the team — building on that, what would success look like for someone in this role in the first six months?"

> 备选（问流程收尾）："What do the next steps look like from here, and is there anything from my background you'd like me to expand on for the next round?"

---

### 0C — Coding-round project opener (轻量版，开场 2–5 min 口语稿)

> 被问"讲个项目"时的深度 opener：high-level、少术语、一个钩子 + 一个数字，说完主动交回控制权进题。深挖细节全部留给 HM 面（见 Part 1 / Part 2）。目标时长 ~2 min 核心，可扩到 ~4 min；读现场气氛收放。

**[Core — always say, ~2 min]**

> "Sure. I'm a backend engineer — for about the past year I've worked mostly on the subscription and premium-membership side of a large news app.
>
> The project I'd pick is our subscription billing system. The interesting thing about it is that we never actually handle the money — Apple and Google are the ones charging the user. So all our backend really knows about someone's subscription is what Apple and Google tell us — Apple posts notifications to a webhook on our side, and we consume Google's over Pub/Sub. And that event stream is messy — things come out of order, get duplicated, arrive late. So the real problem isn't 'process a payment,' it's 'keep a correct subscription state on top of an unreliable event stream' — and since it's money, being wrong is expensive.
>
> I worked on two main things there. The first: what happens when a renewal charge fails. The system used to just drop the user right away — but the platforms actually keep retrying the charge for weeks, and a lot of those users recover. So I added a 'billing-retry' state — instead of dropping them, we turn access off but keep their record alive through that retry window, and that feeds a win-back flow to bring them back.
>
> The second was correctness — making sure a late or duplicate notification can't overwrite good state. I wrote the logic that decides, before we change anything, whether an incoming event is genuinely newer than what we already have, or a stale replay we should ignore.
>
> All of this sits on top of a notification pipeline the team already had. Day to day it handles around 1,500 of these subscription events across Apple, Google, and Stripe."

**[Optional — add only if they seem engaged, +1 min]**

> "What I like about it is it's a deceptively simple-sounding domain — 'just handle subscriptions' — but almost all the difficulty is in the failure modes and the ordering, not the happy path. The subtle bugs are things like a duplicate event rebinding a record back to an old state — the kind of thing you only catch if you treat the webhook stream as adversarial."

**[Close — hand back control]**

> "Happy to go deeper on any of it — or we can jump straight into the problem."

**Delivery notes (internal, don't say out loud):**
- No jargon in this version (no RTDN / SetNX / fencing). Save all of that for the HM round.
- Ownership is handled with one light clause — "a notification pipeline the team already had." Don't open the full boundary discussion here.
- If nervous, say only the Core and close. Expand only on a prompt.

---

## Part 1 — Q&A

> 占位框架。每题按下面模板填写。素材由另一个 session 从 project 更新。

<!--
### Q1. <interview question>
**Theme:** <主主题> (also: <次主题>)
**Story used:** → [Story X — <title>](#story-x--<anchor>)

> **(Situation)** …
>
> **(Task)** …
>
> **(Action)** …
>
> **(Result)** … <takeaway 一句收尾>
-->

### Q1. Tell me about your favorite project. What was the hardest part, and what would you do differently?
**Theme:** Technical depth / Problem solving (also: Ownership, Failure/Learning)
**Story used:** → [Story 1 — Subscription billing correctness & the billing-retry state](#story-1--subscription-billing-correctness--the-billing-retry-state)

> **(Situation)** I own a big part of our subscription and premium-membership backend. The backend never touches money — Apple and Google are the merchants of record — so a user's paid access is only as correct as how we process the webhooks they send us, and those webhooks arrive out of order, duplicated, and late.
>
> **(Task)** Two problems fell to me. First, correctness: a late or duplicate notification could overwrite good state and give a user wrong access — and since it's money, wrong is expensive. Second, we were too blunt on payment failures — the moment a renewal charge failed we dropped the user, even though the platform keeps retrying for weeks and many of them recover.
>
> **(Action)** Let me set up the system first, because my part sits inside it. The two platforms deliver differently: Apple POSTs its notifications to a webhook endpoint on our server, and Google publishes to a Pub/Sub topic that our server consumes as a subscriber. Around the code that mutates a user's record, the system takes a per-user lock to serialize concurrent events — the webhook endpoint, the Pub/Sub consumer, and that lock were all existing infrastructure. But a lock only solves concurrency; it doesn't solve ordering. So what I added was, one, an ordering guard that runs before any write and drops an event that's older than the last one we processed or points at a superseded transaction. Two, a new billing-retry state: instead of dropping a user on a failed charge, we suspend access but keep the record recoverable through the retry window, and that state feeds a win-back flow. And three, the refund and revoke paths that weren't handled, plus a lot of diagnostic logging.
>
> **(Result)** The pipeline reliably handles around 1,500 subscription notifications a day across Apple, Google, and Stripe, and the billing-retry state turned a chunk of what used to be hard churn into recoverable, win-back-eligible users. What stuck with me was that the hard part of subscriptions isn't the happy path — it's treating the webhook stream as unreliable and designing state that survives it. Happy to go deeper on the state design or the ordering logic.

---

## Part 2 — Story bank

> 占位框架。每个 story 按下面模板填写。**Follow-up ammo 区是 TikTok 的重点，必须做厚（见 Agent Notes 第 3 条）。**

<!--
### Story X — <title>
**Themes covered:** <主主题>（primary）, <次主题>…
**When:** <YYYY-MM 或时期>

**Situation**
…

**Task**
…

**Action** (multi-layer, low-detail — 细节下沉到 follow-up)
- …

**Result**
- …（尽量量化）

**Follow-up ammo**（TikTok 重点：机制 / 数字 / 为什么这样选 / 边界失败模式 / 我做的 vs 别人做的）

*<追问1>* — …

*<追问2>* — …

⚠️ 深挖风险点：<哪句经不起追问，怎么弱化>

**Takeaway options**（pick per wording；default 第一个）：
1. (default) "…"
2. "…"
3. (short) "…"
-->

### Story 1 — Subscription billing correctness & the billing-retry state
**Themes covered:** Technical depth / Problem solving (primary), Ownership, Failure/Learning
**When:** 2026-04 → 2026-05

**Situation**
The backend never handles the money — Apple and Google are the merchants of record and charge the user directly. So the only thing our system knows about a subscription is what the platforms tell us. The two arrive by different transports: Apple's App Store Server Notifications are HTTP POSTs to a webhook endpoint on our server; Google's RTDN comes over Pub/Sub — Google publishes to a topic and our server consumes it as a subscriber. Either way the event stream arrives out of order, gets duplicated, and shows up late. And it's money, so getting the state wrong means either giving away paid access or cutting off a paying user.

**Task**
Two gaps were mine to close. (1) Correctness: nothing stopped a late or duplicate event from overwriting good state. (2) Retention: on a failed renewal charge the system dropped the user immediately, even though the platform keeps retrying billing for weeks and many of those users recover — so we were throwing away recoverable revenue.

**Action** (multi-layer, low-detail — mechanism sinks to follow-up)
- **Framed the problem around the existing system.** The Apple webhook handler, the Google Pub/Sub consumer, and a per-user lock that serializes concurrent events for one user were already there. The lock handles concurrency; it does not handle ordering. My work sits inside that.
- **Ordering guard (mine).** Before any state write, decide skip-or-process based on event time + transaction identity, so a stale replay or a duplicate can't regress the record.
- **billing-retry state (mine — the main design).** Split entitlement from lifecycle: a failed charge sets access off but keeps the record recoverable through the platform's retry window, and that state feeds the retention/win-back paywall.
- **Refund/revoke + observability (mine).** Added the refund, family-sharing revoke, and consumption-request paths that weren't covered, and enriched the notification logs so this is debuggable in prod.

**Result**
- ~1,500 notifications/day across Apple (~300), Google (~1,200), Stripe (~26); ~24 new subscriptions/day; subscription-status reads ~35M/day.
- billing-retry converts first-failure churn into a recoverable, win-back-eligible state instead of a hard drop.

**Follow-up ammo** — organized by deep-dive theme (this is one project; each theme is a direction the interviewer can pull). Do the mechanism, the number, the "why-this-not-that," and mark mine vs teammates'.

---

**Deep-dive Theme 1 — Ordering & duplicate correctness** (mine: `shouldIgnoreAppleNotification`)

*How does the ordering guard work?* — No record → only a new-purchase (`SUBSCRIBED`) event may create one. Same transaction id → drop if the event time is older than the last one processed. Transaction id mismatch → only a new-purchase event may rebind it; anything else is a superseded/old event, dropped. Two known-benign reorderings are skipped without being flagged as anomalies.

*Which timestamp — platform event time or your receive time? Equal timestamps? Clock going backwards?* — The platform-signed event time (Apple's `signedDate`, Google's event time), not our receive time, so I'm not exposed to skew between my own servers. The check is strictly-older → skip, so two events with the same timestamp are both processed — time alone won't dedupe them. And the platform clock isn't monotonic across different transactions, which is exactly why a resubscribe is handled by transaction identity, not time.

*Same notification delivered twice — how do you dedupe?* — Three cases: a stale re-delivery is dropped by the ordering guard; an exact duplicate with the same timestamp is not rejected by time, so it can reprocess — that's tolerable because the writes are convergent (renewing to the same expiry lands in the same place); concurrent duplicates are serialized by the per-user lock. Honest gap: there is no dedupe on the notification UUID, so this is at-least-once with convergent writes, not exactly-once.

*Isn't there a read-modify-write race between the guard reading last-event-time and the write?* — Yes, and that's exactly what the per-user lock is for — it serializes all processing for one user so the read-modify-write is single-threaded per user. Different users touch different records.

⚠️ There's a real bug here I found: a duplicate new-purchase event for a *different, older* transaction can rebind the record back to that old transaction — the mismatch path has no time guard and the transaction→record mapping is never pruned. Low-probability, but real. (Covered in the redo list.)

---

**Deep-dive Theme 2 — The billing-retry state machine** (mine: the `billing-retry` state; base machine is a teammate's)

*Grace period vs billing-retry — why turn access off?* — It maps to the platform's own signal. Apple's `DID_FAIL_TO_RENEW` carries a subtype: `GRACE_PERIOD` means "keep serving," so that path keeps access on; no subtype (or `GRACE_PERIOD_EXPIRED`) means "stop serving, but we'll keep retrying ~60 days," so that path goes to billing-retry with access off. I kept channel/SKU/transaction/expiry on the record so a later `DID_RENEW` restores instantly.

*Why two status fields instead of one enum?* — `Status` (paid/free/unsubscribed) is the access gate everything downstream reads; `PaidStatus` (active/grace/billing-retry/cancel) carries the "why/recoverable" detail the retention flow needs. billing-retry sets `Status=unsubscribed`, so every existing `Status==paid` check treats them as not-entitled with zero code changes, while the new info lives in `PaidStatus`. Backward-compatible by design.

*60 days — is that a timer you maintain?* — No, that's Apple's retry window. The backend has no timer; it's event-driven — `DID_RENEW` restores, `EXPIRED` (billing-retry subtype) ends it.

*How does a user get out, and what if no event ever comes?* — `DID_RENEW` → active, or `EXPIRED` → terminal. If neither ever arrives (lost webhook), the record sits in billing-retry indefinitely with access off — no sweeper. Access is already off so it's not a revenue leak, but it's drift; the fix is reconciliation (Theme 3).

*Does `Status=unsubscribed` fire the same side effects as a real unsubscribe?* — For NB Premium, no — that write path only touches the record and cache. For a media subscription, yes — it goes through the shared update path that fires newsletter-unsubscribe / email / counter side effects on any paid→not-paid transition. So a media subscription entering billing-retry can trip an "unsubscribe" email on a transient failure — a subtle interaction I'd want to revisit.

*Google has a real pause. Why map it to billing-retry?* — Semantically different, but the client can't render a distinct paused state yet, so it's a deliberate placeholder (same access-off behavior), with a TODO to split it out later.

---

**Deep-dive Theme 3 — Correctness in prod & reconciliation** (mine: the notification logging; reconciliation is a gap)

*How do you know it's correct in prod?* — I enriched the per-notification log so every event records whether it was handled, ignored, in free trial, and a human-readable reason. So I can query why any event was skipped or how it was applied. Honest limit: detection is log-based; there's no automated alert on, say, a wrong-downgrade rate.

*Do you reconcile your DB against Apple/Google?* — There's no periodic reconciliation for drift — it's event-driven. One nuance in our favor, and this is existing system behavior not mine: on the Google path we pull the current subscription state from Google's API while handling the event rather than trusting only the webhook payload; on Apple we verify and parse the signed payload. The gap is a record that misses an event entirely (a dropped webhook) — nothing sweeps for that. What I'd add is a reconciliation job that re-queries the platform for records stuck in billing-retry or past their expiry.

*Why does Google call the API but Apple doesn't?* (depth) — It's the notification model. Apple's notification is a signed JWS that carries the full renewal and transaction info inside it — once we verify the signature the payload itself is the source of truth, so there's nothing to call back for. Google's RTDN is a thin pointer — it basically says "something changed for this purchase token," so we have to call the Play API with that token to get the actual state. So Apple = parse-and-verify a self-contained signed event; Google = use the event as a trigger to fetch live state. A side effect is the Google path is inherently more robust to a stale payload since it always reads live state. (The Google consumer is a teammate's — I'm explaining the mechanism, not claiming it.)

*A refund comes in for a user who already churned — what happens?* — The refund/revoke path first checks the record: if it's not in the Paid state, it logs "already revoked" and no-ops. So it's idempotent for the already-gone case.

---

**Deep-dive Theme 4 — Testing & safe rollout** (mine: the prod test-user gate)

*How would you test out-of-order and duplicate delivery?* — Two layers. Unit level: the ordering guard is a pure function of (record, event, event-time), so I test skip-or-process on crafted sequences — a stale replay, a same-timestamp duplicate, a resubscribe with a new transaction id. End-to-end level: I run real subscriptions in **stage against Apple/Google's sandbox**. The sandbox accelerates the renewal cycle — a monthly sub renews every few minutes instead of every month; that acceleration is a platform feature, not ours, and our stage deployment is just the backend wired up as the Sandbox server that receives those `Environment=Sandbox` notifications — so I can watch the whole state machine (renew → fail → grace → billing-retry → expire → recover) play out end-to-end in one session against the real handler. ⚠️ I have NOT verified Patrick's actual test coverage — he must state which sandbox scenarios and unit cases he really ran; do not claim a replay harness that wasn't built. Honest caveat to keep ready: sandbox timing is compressed and some paths (certain refund / family-sharing cases) are hard to reproduce there, plus env-gating can make a path structurally untestable on stage even with identical code — so sandbox-green ≠ prod-safe. The cross-transaction rebind bug is an example my testing didn't catch.

*You added a prod test-user list — why hardcoded IDs, and why test in prod at all if sandbox works?* — The billing lifecycle itself I can test in stage/sandbox. `isProdTestUser` is for the *last* step: validating the retention paywall on the **prod deployment with the real client** for a small controlled set of accounts before rolling out — the real rendering and end-to-end context you only get on prod (the code comment literally says it's for the grace-period retention-paywall test on prod). It's OR'd with a stage check. The risks I'd call out: it's a hardcoded list, not a managed flag, so it rots; if a real user's id landed in it they'd silently get test behavior; and a sibling `isSkuTestUser` has no callers — dead code I should remove.

*How did you roll out a state-machine change to a money path safely?* — First validate the full state machine in stage against sandbox — the accelerated clock lets me cover renew/fail/grace/retry/expire in minutes — then prod, with the test-user gate for a final controlled check. You can't cleanly A/B a notification handler per user, so stage-sandbox is the main pre-prod gate; the guard defaulting to skip-when-unsure plus the enriched logs are the safety net. If I redid it I'd add a shadow mode — compute the new state and log the diff without applying it — before flipping it on.

---

**Deep-dive Theme 5 — Scale & performance**

*This is ~1,500 notifications/day — would it hold at 100x?* — At today's volume it's comfortable. At 100x the sharp edge is the per-user lock: it retries with a linear sleep (0.5s / 1s / 1.5s) and gives up to a redelivery, so a single hot user with many concurrent events would serialize and burn that backoff. Fine now, but for high per-user concurrency I'd move to a per-user queue and keep the critical section minimal. (The lock is a teammate's; this is how I'd evolve it.)

*`get-subscription-status` is ~35M/day — how is that served?* — It's cache-first: the NB Premium status read goes through a per-user cache key, and every state write invalidates that key. So the hot read path is a cache hit and the DB is only touched on a miss or a write. That's what lets a 35M/day read sit on a subscription store that only sees ~1,500 writes/day. (The read/cache path is largely existing infra; my state writes invalidate it correctly.)

---

**Deep-dive Theme 6 — Ownership & boundaries** (the honesty probe — answer it cleanly)

*Which parts did you design vs extend vs inherit?*
- **Designed (mine):** the billing-retry state; the Apple ordering guard (`shouldIgnoreAppleNotification`); the refund / revoke / consumption-request branches; the NB Premium product-config layer (paywall config, reading-mode gating, retention paywall); Google Play SKU creation.
- **Extended:** the Google skip function — I rewrote its body (link-token / stale-event handling) on top of a teammate's original.
- **Inherited (teammates'):** the Apple webhook handler and Google Pub/Sub consumer; the per-user redis lock; the base state machine (active/grace/expired/cancel and the renew/cancel/expire transitions); Stripe SKU creation.

*Are the behavior-triggered upsell paywalls live?* — Honest: they exist as server config and are A/B-wired, but in production almost all of them (ad-view, high-frequency, comment-post, etc.) are turned off; only the article free-trial trigger is on. So the framework is built and ready, but most triggers aren't rolled out. Don't imply they're actively driving conversions.

---

**Deep-dive Theme 7 — Product judgment (high ad-revenue users)** (mine: the check and its call sites)

*What does the high-revenue rule do?* — If a user made more than $50 in ad revenue over the past year (and isn't in a small QA exclusion list), the code shows them no subscription prompts, doesn't turn on the in-article-ads experiment, doesn't show reading-mode nudges, and doesn't lock premium articles — they get full content. The rationale: don't push a subscription (which removes ads) onto users who already generate a lot of ad revenue, so we don't cannibalize the ad money.

*Who set $50, and how do you know it's net positive?* — Honest: $50 is a hardcoded threshold; I can't claim I set the number or that there's a live measurement loop tuning it. The right way to know it's net-positive would be to A/B the threshold and compare ad revenue retained against subscription revenue forgone. ⚠️ Don't claim a measured result — the code only tells us the rule exists, not that its business impact was validated.

---

⚠️ Cross-cutting risk points (hand back honestly, don't over-claim):
- The **per-user lock**, the **Apple webhook handler + Google Pub/Sub consumer**, the **base state machine**, and **Stripe SKU creation** are NOT mine (teammates'). Explain them fully as context, but on "did you build X" → "that's existing infra; my part was the ordering guard / billing-retry state / product-config layer." Explaining ≠ claiming.
- **No measured retention lift** for billing-retry, and **no measured impact** for the high-revenue rule — describe intent, don't quote invented numbers.
- **Actual test coverage is unverified** — don't claim a replay harness or specific test suite unless it's real.
- The **behavior-triggered upsell paywalls are mostly turned off** in prod — framework built, not rolled out.

**Takeaway options** (pick per wording; default first):
1. (default) "What stuck with me was that the hard part of subscriptions is treating the webhook stream as unreliable and designing state that survives it."
2. "For me the lesson was that with money, the failure modes matter more than the happy path."
3. (short) "Happy to go deeper on any layer."
