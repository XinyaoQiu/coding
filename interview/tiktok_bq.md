# TikTok — Behavioral Questions Prep

> Structure — organized by interview round. Each round block has: self-intro, likely questions, questions to ask back. Detailed reusable stories live in Part 4 (Story bank); question answers link into it.
> - Part 1 — HR / recruiter screen
> - Part 2 — Technical round (coding / tech screen)
> - Part 3 — Hiring Manager (HM) round
> - Part 4 — Story bank (shared, referenced by all rounds)

---

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改本文件答案时遵守：

1. **语言**：**正文（Part 1–4）全部是英文可直接念的稿，不含任何中文**（题目 + 答案）。所有中文策略提示集中放在下方「策略提示区」，正文里绝不夹中文。**例外：Part 5「HM 面中文陪练区」是中文 mock 问答陪练稿，不是面试念稿，允许全中文。** 跟用户对话用中文。
2. **按轮次分块**：文件按 HR 面 / 技术面 / HM 面三大块组织（Part 1/2/3），每块含 自我介绍 + 可能被问的问题 + 反问。可复用的详细故事集中放 Part 4 Story bank，各块问题用 Markdown 内链引用。
3. **自我介绍三块共用同一底稿**（用户确认）：三轮 self-intro 用同一段，按轮次微调结尾。
4. **⚠️ TikTok 特点 —— 挖细节**：面试官追问极深，看重具体技术细节 + 量化数据 + 决策链条。Follow-up ammo 区必须做厚做深（机制 / 数字 / 为什么这样选 / 边界失败模式）；量化优先，数字可编；主叙事保持颗粒度克制，实现细节下沉到 follow-up。
5. **收尾**：每题 Result 后配一句 takeaway，收尾类型三选一并在相邻题间轮换（takeaway 点题 / 落在量化结果 / 交回控制权），避免像背模板。
6. **收录范围**：只写用户明确要准备的题/故事，不擅自加题。

### TikTok BQ 常见主题清单（打标签用）

- **Impact / Ownership** —— 主动扛下、把事做成、可量化的业务影响。
- **Technical depth / Problem solving** —— 复杂技术问题、根因排查、系统设计取舍。
- **Conflict / Collaboration / Cross-team** —— 跨团队协作、意见分歧、如何对齐。
- **Ambiguity / Fast pace** —— 需求不清 / 快节奏下如何推进（TikTok 强调 speed）。
- **Failure / Learning / Growth** —— 失败、错误、复盘与改进。
- **Prioritization / Trade-off** —— 资源/时间有限下的取舍。
- **User / Product focus** —— 从用户或产品价值出发做决策。

### 策略提示区（怎么用正文里的稿 —— 正文保持纯英文，策略都写这里）

**Self-intro（三轮通用）**：学历 → Newsbreak 全职（说得比实习多）→ Alibaba/Tesla 实习各一句 → 结尾落 fast-paced + real-world impact。省略 TikTok / UMTRI 两段实习。毕业时间过去时（2025-12 已毕业）。`[Name]` 是占位。oncall 挂在 Newsbreak 不是实习。按轮次换结尾：HR 落 career goals；技术面落进题；HM 落项目。

**Part 1 HR — career goals**：定位「深耕后端 + 大规模系统」（专注有方向感），不是转方向。三点：①我是谁/做过什么 ②想往哪走 ③why TikTok(scale 对上)。真诚 > 流利。

**Part 1 HR — why leaving Newsbreak / why now**：⚠️ 只用 pull factor，绝不用 push factor。红线——不说「现在工作没 impact」「正在积极找」（Applied 那次的说法，不要重复）。要说：喜欢现在的团队，但 TikTok LIVE 团队做的全球内容治理 + 高并发底层工具正是技术热情终极所在；可顺带提想去大公司（但「大公司」必须紧跟 scale/traffic 的技术理由，别裸说）。

**Part 1 HR — work authorization**：身份 F-1 OPT first year（STEM）。策略：先讲 runway（现在能合法工作 + STEM extension 约 3 年），再讲未来需要 sponsorship。别只说"需要 H1B"（听起来像马上要）。

**Part 1 HR — 反问**：HR 通常开头已介绍岗位/team，别再问"team 做什么"（显得没听），顺着往下挖 or 问流程。

**Part 3 HM — project opener delivery**：opener 里不用 jargon（no RTDN / SetNX / cert-chain / compare-and-set 这类），留给 follow-up 深挖；紧张就只说 Core + close。Story 全程用 we 做主语，不做 mine-vs-teammates 区分。

**Part 4 Story bank**：两个全链路 story（Story 1 subscription / Story 2 video upload）都是 code-verified 的深度弹药，按 deep-dive theme 组织，被 Part 2/3 引用。两个都能当 HM 面主力深挖：subscription 偏分布式正确性/三平台，video upload 偏全链路+用户视角+跨系统。

---

## Shared self-intro

> "Hey, I'm [Name]. I did my Master's in Computer Science at UIUC and graduated last December. Since then I've been a backend engineer at Newsbreak, a large news app, working on real production systems.
>
> A few things I've worked on there — I own a good chunk of our premium subscription system, I've worked on our UGC video-upload pipeline on both anti-abuse and upload status, and my main project this year has been migrating our APIs from JSON to Protobuf across a lot of endpoints. I'm also on the on-call rotation there, so I'm comfortable digging into production alerts and finding the root cause of issues.
>
> Before Newsbreak I interned at Alibaba Cloud, where I built a big-data processing infrastructure — a data lake that ingested log files into our lakehouse storage — and at Tesla, where I built an internal Gantt-chart website for task management used by a lot of internal users.
>
> Across all of these, what I've found is that I like working in a fast-paced environment, solving real-world problems and seeing the impact."

---

## Part 1 — HR / recruiter screen

### 1.1 Self-intro

> "Hey, I'm [Name]. I did my Master's at UIUC and graduated last December. Since then I've been a backend engineer at Newsbreak, which is a tech company focused on local news and AI, and I work on the server side.
>
> I've done a few things here. First, I migrated our core APIs from JSON to Protobuf. Second, for the video-upload pipeline, I built an anti-abuse middleware and improved the upload status handling. The last thing is I developed some new features for our premium subscription system, like a billing-retry flow that wins back users after a failed payment. I'm also on the on-call rotation here, so I'm comfortable digging into production alerts and finding the root cause of issues.
>
> For internships, I interned at Alibaba Cloud, where I built a big-data processing infrastructure — a data lake that ingested log files into our lakehouse storage — and at Tesla, where I built an internal Gantt-chart website for task management used by a lot of internal users.
>
> Across all of these, what I've found is that I like working in a fast-paced environment, solving real-world problems and seeing the impact. That's why I applied to TikTok."

### 1.2 Likely questions

**Q. Walk me through your background.**

Use the 1.1 self-intro.

**Q. What are your career goals / what are you looking for?**

> "As for where I want to go — I want to keep growing as a backend engineer and go deeper on large-scale, high-traffic systems. What I've realized I enjoy most is the work where correctness and reliability really matter — the failure modes, the edge cases, the on-call side of things — and I want to be somewhere the scale actually forces you to get those things right. That's honestly a big part of why I'm interested in TikTok — the scale here is a different level, and I'd like to grow into an engineer who can own and design systems at that scale, not just work within them."

**Q. Why are you leaving Newsbreak / why now?**

> "Honestly, I really like my current team — this isn't me running from anything. It's more that what your LIVE team works on — global content governance and the high-concurrency infrastructure behind it — is exactly where my technical passion is. And I'd like to grow at a company operating at that kind of scale."

**Q. What's your work authorization status?**

> "I'm currently on OPT — it started this February — and with the STEM extension I have about two and a half years left. I would need H1B sponsorship down the line."

### 1.3 Questions to ask back

If not already covered, ask one of:

> "You mentioned the team — building on that, what would make someone successful in this role in the first six months?"

> "What do the next steps look like from here?"

If she already covered both the team and the process:

> "You've actually covered most of what I was curious about, so I don't have questions. I'm looking forward to the next steps."

---

## Part 2 — Technical round (coding / tech screen)

### 2.1 Self-intro

Use the Shared self-intro, ending with:

> "Mostly I like backend work where correctness really matters. Happy to jump into the problem whenever you're ready."

### 2.2 Likely questions

Coding problems live in tiktok_coding.md. If asked to talk through a project or a system, use a short version and point to [Story 1](#story-1-end-to-end-subscription-pipeline-purchase-store-callbacks-state-machine-read-end) — keep the depth controlled here; save the deep dive for the HM round.

### 2.3 Questions to ask back

> "What does the day-to-day engineering workflow look like on the team — how do code review, testing, and on-call work?"

> "What's a technical challenge the team is actively working on right now?"

---

## Part 3 — Hiring Manager (HM) round

### 3.1 Self-intro

Use the Shared self-intro, ending with:

> "Happy to go deep on any of these — the subscription system is probably the one I've owned most end to end."

### 3.2 Project opener

**[Core — always say, ~2 min]**

> "The project I'd pick is our subscription billing system. The interesting thing about it is that we never actually handle the money — Apple and Google are the ones charging the user. So all our backend really knows about someone's subscription is what Apple and Google tell us — Apple posts notifications to a webhook on our side, and we consume Google's over Pub/Sub. And that event stream is messy — things come out of order, get duplicated, arrive late. So the real problem isn't 'process a payment,' it's 'keep a correct subscription state on top of an unreliable event stream' — and since it's money, being wrong is expensive.
>
> I worked on two main things there. The first: what happens when a renewal charge fails. The system used to just drop the user right away — but the platforms actually keep retrying the charge for weeks, and a lot of those users recover. So I added a 'billing-retry' state — instead of dropping them, we turn access off but keep their record alive through that retry window, and that feeds a win-back flow to bring them back.
>
> The second was correctness — making sure a late or duplicate notification can't overwrite good state. I wrote the logic that decides, before we change anything, whether an incoming event is genuinely newer than what we already have, or a stale replay we should ignore.
>
> All of this sits on top of a notification pipeline the team already had. Day to day it handles around 1,500 of these subscription events across Apple, Google, and Stripe."

**[Optional — add only if they seem engaged, +1 min]**

> "What I like about it is it's a deceptively simple-sounding domain — 'just handle subscriptions' — but almost all the difficulty is in the failure modes and the ordering, not the happy path. The subtle bugs are things like a duplicate event rebinding a record back to an old state — the kind of thing you only catch if you treat the webhook stream as adversarial."

**[Close — hand back control]**

> "Happy to go deeper on any of it."

### 3.3 Likely questions

#### Q. Tell me about your favorite project. What was the hardest part, and what would you do differently?
Story used: → [Story 1](#story-1-end-to-end-subscription-pipeline-purchase-store-callbacks-state-machine-read-end)

> "My favorite is the subscription billing system I work on at Newsbreak. The interesting thing about it is that we never actually handle the money — Apple and Google charge the user directly, so all our backend really knows about someone's subscription is what those platforms tell us. Apple posts notifications to a webhook on our side, and we consume Google's over Pub/Sub. My two main pieces were an ordering guard that decides, before any write, whether an incoming event is genuinely newer or a stale replay we should ignore, and a new billing-retry state — instead of dropping a user the moment a renewal charge fails, we turn access off but keep their record recoverable through the platform's retry window, which feeds a win-back flow.
>
> It's my favorite because it looks deceptively simple — 'just handle subscriptions' — but almost all the difficulty is in the failure modes and the ordering, not the happy path. And because it's money, being wrong is genuinely expensive, so correctness actually matters in a way that made the work feel real.
>
> The hardest part was exactly that: keeping the state correct on top of an event stream that arrives out of order, duplicated, and late. A lock handles two events racing, but it doesn't handle a late or duplicate event overwriting good state — so I had to reason carefully about which event should win, using the platform-signed event time and the transaction identity rather than our own receive time. The subtle bugs are things like a duplicate event rebinding a record back to an old state, which you only catch if you treat the webhook stream as adversarial.
>
> What I'd do differently is add a shadow mode before rolling out a state-machine change — compute the new state and just log the diff without applying it, so I can see divergence on real traffic before flipping it on. I'd also add a reconciliation job that periodically re-queries the platform for records that look stuck, since right now if a webhook is ever dropped, nothing sweeps for it. Happy to go deeper on the ordering logic or the state design."

### 3.4 Questions to ask back

> "What's the biggest technical challenge the team is focused on over the next couple of quarters?"

> "What does growth look like on this team — how do engineers here move from owning features to owning systems?"

---

## Part 4 — Story bank

### Story 1 — End-to-end subscription pipeline (purchase → store callbacks → state machine → read/end)
Themes: Technical depth / Problem solving (primary), Ownership, Distributed systems / correctness

**Situation**
NewsBreak runs a paid-subscription system spanning three app stores (iOS/Apple StoreKit, Android/Google Play, Web/Stripe) and multiple products (NB Premium, creator/media subscriptions, NutriScan). A subscription's life doesn't end at purchase — renewals, cancellations, billing retries, grace periods, refunds, and family-sharing revokes all happen asynchronously, days or months later, delivered by each store in its own format and reliability model. We needed one server pipeline that could initiate and verify purchases across all three stores, ingest each store's async lifecycle events safely under at-least-once/unordered delivery, and keep a single correct per-user entitlement that clients and downstream services read.

**Task**
We own the full lifecycle in the `server` monolith: the `/subscription` purchase-entry routes, the async ingestion (Apple signed webhook, Google Pub/Sub RTDN consumer, Stripe webhook), the persistence/state-machine layer over Mongo, and the read path that serves `get-subscription-status` plus retention paywalls. The core requirements: clients must never be able to forge or replay a receipt; every store's out-of-order or duplicate callback must be idempotent; a lost webhook must not strand a user in a wrong state; and one endpoint/record must serve multiple products without divergent copies of logic.

**Action**
**Segment 1 — Purchase entry (three platforms, one route group).** All paid-subscribe flows sit under the `/subscription` group in `api/http/route.go`, all behind `middleware.SessionAuth()`. We split by store SDK, not by product.
  - iOS is two calls. First `POST /subscription/ios-paid-prepare` -> `IOSPaidPrepare` resolves the SKU (explicit `sku` via `FindSKUInfoByName`, else `SkuTypeMediaSubscription` when `media_id>0`), runs pre-flight guards, and returns Apple Advanced Commerce signed `advancedCommerceData`. The app runs StoreKit, gets back a signed JWS, then calls `POST /subscription/ios-paid-subscribe` with `signed_transaction_info`.
  - We verify that JWS server-side: parse the `x5c` header, chain-verify leaf+intermediates against a pinned Apple Root CA-G3 (`pkg/appstore/cert.go`), require ES256/`*ecdsa.PublicKey`. Then bind: `FindUseridByAppAccountToken(transaction.AppAccountToken)` must equal the session userID, else reject. Derive SKU from `transaction.ProductID`, fan out on `skuInfo.SkuType` (media_subscription / nb_premium / NutriScan) into the right `PaidSubscribe` variant.
  - Android is one call: `POST /subscription/android-paid-subscribe` with `package_name`+`purchase_token`. We call `Purchases.Subscriptionsv2.Get(packageName, token)` to verify against Google, upsert the record, then `Purchases.Subscriptions.Acknowledge` back to Google (CRITICAL log if it fails — Google auto-refunds unacknowledged).
  - Web is Stripe (`stripe-checkout` -> session URL, `stripe-paid-subscribe`).
  - Every write lands in mongo db `subscription`, collection `subscription_relation` (media) / `nb_premium_subscription` (premium), on the `api` connection, as an idempotent upsert (`SetUpsert(true)`).
**Segment 2 — Apple async lifecycle (signed webhook).** Renewals/refunds/cancels arrive at `POST /notification/apple-notification` — NO auth middleware, because the JWS signature IS the auth. Body is one field `signedPayload`. We re-run the same x5c chain-verify, decode the nested `signedTransactionInfo`+`signedRenewalInfo`, resolve the local relation by `originalTransactionId`, run `shouldIgnoreAppleNotification` (eventTime-vs-`LastNotifEventTime` ordering guard) BEFORE the type switch, then switch on `notificationType`+`subtype` to mutate state. Every notification is persisted to `AppleNotificationLog` via a deferred write regardless of outcome. Always return HTTP 200 on ignore/no-op so Apple stops retrying.
**Segment 3 — Google async lifecycle (pointer + fetch, via Pub/Sub PULL).** Unlike Apple's push webhook, on boot (only on clusters `server-a4api-push`/`server-a4api-stage`) we start a goroutine that PULLs RTDN from Pub/Sub sub `paid-subscription-billing-sub`. The RTDN is a thin pointer (packageName/type/purchaseToken/eventTimeMillis, NO state). So for every message we re-fetch authoritative state with `Subscriptionsv2.Get(purchaseToken)` and drive ALL writes off that fetched object. userID comes from Google-echoed `ObfuscatedExternalAccountId`. `shouldIgnoreRTDN` handles stale/out-of-order via `LastNotifEventTime`+`LinkedPurchaseToken` chaining. Handler error -> `msg.Nack()` (redeliver); benign skip -> return nil -> `msg.Ack()`.
**Segment 4 — Persistence + state machine + read path.** Every transition is a repo method that mutates an in-memory `SubscriptionRelation` via named entity mutators, then funnels through ONE private writer (`updateNbPremiumSubRel` / `updateSubscriptionRelation`) that does `$set UpdateOne` keyed on `user_id`, deletes the redis cache, and appends a before/after audit log. Two orthogonal fields: top-level `Status` (paid/free/unsubscribed) = coarse access gate the client reads; `PaidStatus` (active/grace_period/billing_retry/cancel/paused) = fine billing lifecycle. Concurrent notifications per user serialized by a redis SetNX lock (30s TTL, 3 tries). Reads go cache-first (`FindNbPremiumSubRelWithCacheAndUpdate`) with lazy self-healing expiry; `get-subscription-status` served from here.
**Segment 5 — Ending (cancel vs expiry vs refund).** All store callbacks converge on `SubscriptionRelation` with three mutation semantics. Cancel (auto-renew off) = soft end: `PaidStatus=cancel`, keep `ExpireAt`, `Status` stays paid until period end. Expiry = hard end at boundary. Refund/Revoke = immediate hard end. Terminal status differs by SKU: media degrades to `Status=free` (`StatusPaidToFree`), NB Premium to `Status=unsubscribed` (`StatusPaidToUnsubscribed`). `IsPaidStatusExpired` adds +2 days grace as a lost-webhook fallback. Read side (`premium.go`) turns `PaidStatus=cancel` into a retention paywall (`Canceled`, or `ExpiredSoon` within 3 days of `ExpireAt`).

**Result**
- End-to-end we built one subscription pipeline that serves three stores and multiple products (NB Premium, creator/media subscriptions, NutriScan) through a single route group and a single Mongo aggregate per user, with the store-callback fan-in reduced to one record type and one private writer.
- Trust is anchored server-side on every path: iOS/Apple JWS is chain-verified against a pinned Apple Root CA-G3 and bound to the session user via AppAccountToken; Android/Google is verified by a live `Subscriptionsv2.Get` and acknowledged back; the async webhooks re-verify (Apple) or re-fetch (Google) rather than trusting the message body.
- The lifecycle is idempotent and ordering-safe under at-least-once, unordered delivery: per-relation `LastNotifEventTime` watermark, per-user redis lock, idempotent upserts, and Ack/Nack semantics that tie Pub/Sub redelivery to handler success. Benign skips Ack; real failures Nack and retry.
- The state model deliberately separates a 3-valued access gate (`Status`) from a billing lifecycle (`PaidStatus`) so `billing_retry` keeps the record recoverable (access off, but sku/channel/txid/expire retained) for Apple's 60-day retry / Google recovery, while a lost webhook still self-heals via a +2-day lazy-expiry fallback on the read path.

**Follow-up ammo** — organized by deep-dive theme; each theme is a direction the interviewer can pull.

---

**Deep-dive — Anti-forgery / trust boundary (how a client can't fake a free subscription)**

- Q: How do you stop a user forging or replaying an Apple receipt? — A: We never trust a raw client receipt. `IOSParseSignedTransaction` calls `appstore.ParseSignedTransactions`, which in `pkg/appstore/cert.go extractPublicKeyFromToken` reads the JWS `x5c[]` header (x5c[0]=leaf, rest=intermediates), builds an intermediate pool, and calls `leafCert.Verify` against a pinned Apple Root CA-G3 PEM. Real x509 chain validation, ES256, key must assert to `*ecdsa.PublicKey`. Not a base64 decode.
- Q: Even with a valid Apple transaction, how do you stop replay onto another account? — A: `FindUseridByAppAccountToken(transaction.AppAccountToken)` must equal the session userID (`middleware.UseridKey`); mismatch rejects with 'AppAccountToken userID mismatch'. The AppAccountToken->userID mapping lives in `ColUserAppAccountToken` (indexed) and is redis-cached under `PrefixUUIDToUserID` to skip a mongo hit per subscribe.
- Q: Google side trust? — A: The RTDN pointer is never the source of truth. We only use its `purchaseToken` for routing, then call `Purchases.Subscriptionsv2.Get` for authoritative state, and take userID from Google-echoed `ObfuscatedExternalAccountId` (empty/0/unparseable -> ignore+Ack). So the trusted fields all come from the authenticated Play API fetch, not the message.
- Q: Why is `/notification/apple-notification` behind NO auth middleware while `/subscription/*` all use `SessionAuth()`? — A: It's an Apple server-to-server callback; there's no session. The JWS signature verification IS the authentication. Adding SessionAuth would break it. Same rationale for the Google consumer — it's a pulled Pub/Sub message authenticated by the GCP credential, not a user session.
- Q: Why validate the SKU's mediaId against the request mediaId in IOSPaidSubscribe? — A: Prevents buying a SKU for media A but crediting media B; the SKU carries its own MediaID and we assert consistency before writing the relation.

---

**Deep-dive — iOS two-step vs Android one-step (why the asymmetry)**

- Q: Why is iOS prepare+subscribe but Android a single call? — A: Apple StoreKit Advanced Commerce requires a server-SIGNED request first (`advancedCommerceData`) before the purchase can run; that's the prepare step. Google's purchase already carries a verifiable `purchase_token`, so there's nothing to pre-sign — one call verifies and records.
- Q: What does prepare actually guard against? — A: `IOSPaidPrepare` runs `already-paid` (`ErrCodeAlreadyPaidMediaSubscription`) and Apple billing-retry (`ErrCodeSubscriptionInBillingRetry`) checks before signing, so we don't hand the app a purchase request that would create a duplicate subscription.
- Q: The prepare-time SKU resolution — how does one endpoint serve three products? — A: `FindSKUInfoByName` when `sku` is explicit, else `SkuTypeMediaSubscription` when `media_id>0`. Later `skuInfo.SkuType` fans out to `PaidSubscribe` (media) / `NbPremiumPaidSubscribe` / `NutriScanPaidSubscribe`. Same code path, product decided by SKU metadata.
- Q: What's the Apple outbound provider token you use for the App Store Server API calls? — A: ES256 JWT, `kid`=KeyID, `aud`=appstoreconnect-v1, lifetime 1 hour (Apple rejects >60 min after iat), built from `GetAppleKey` (`iap_private_key`/`iap_key_id`/`iap_issuer_id`), bundle `com.particlenews.newsbreak`.

---

**Deep-dive — Google acknowledge + Pub/Sub PULL topology**

- Q: What if the Google acknowledge fails after you've written the record? — A: Logged CRITICAL. Google auto-refunds unacknowledged purchases within its window, so this is a must-retry. On the RTDN path the handler error would Nack -> redelivery re-fetches and re-acknowledges; on the client-initiated path it's the CRITICAL log we alert on. `DeveloperPayload='acknowledged_from_backend'`.
- Q: Why a Pub/Sub PULL subscriber instead of an HTTP push webhook like Apple? — A: Google RTDN is delivered to a Pub/Sub topic; we PULL via `sub.Receive` in a long-lived goroutine. There's literally no HTTP endpoint for RTDN (grep of router dirs is empty). We pin it to `server-a4api-push`/`server-a4api-stage` because that cluster is low-utilization/high-capacity and, critically, so only ONE deployment holds the subscription.
- Q: What breaks if every pod ran sub.Receive? — A: N pods would double-consume the same subscription — competing consumers on shared state, redundant `Subscriptionsv2.Get` fetches, and lock contention. Pinning to one cluster keeps a single consumer group effectively.
- Q: Ack vs Nack exact meaning here? — A: `err==nil -> msg.Ack()` (done, never redeliver); `err!=nil -> msg.Nack()` (redeliver/retry). Benign skips (empty token, wrong env, stale event) deliberately `return nil` so they Ack and DON'T redeliver — they're not errors, they're no-ops. Only a real failure like the live fetch failing Nacks.
- Q: Env isolation between prod and stage clusters? — A: `isSandboxPurchase = subscriptionV2.TestPurchase != nil`. Prod cluster receiving a sandbox purchase -> ignore+Ack; stage receiving a real purchase -> ignore+Ack. Cross-env messages are dropped cleanly. Stage uses sub `paid-subscription-billing-sub-stage`.

---

**Deep-dive — Idempotency & ordering under redelivery (both stores)**

- Q: Walk shouldIgnoreAppleNotification precisely. — A: (1) subRel==nil -> only SUBSCRIBED processed, everything else ignored+markIgnored. (2) stored `ExternalTransactionID == originalTransactionId` (same sub) -> ignore only if `eventTime.Before(subRel.LastNotifEventTime)` i.e. stale by signedDate, else process. (3) `PaidStatus==''` special-cases GRACE_PERIOD_EXPIRED and DID_CHANGE_RENEWAL_STATUS+AUTO_RENEW_DISABLED as benign-skip (ignore=true, setIgnored=false). (4) different externalTransactionID -> only SUBSCRIBED may re-point, else ignore.
- Q: The function returns two booleans — difference? — A: First = ignore/skip processing; second = setIgnored, i.e. whether to mark the log row `Ignored=true`. The grace-period race cases return ignore=true but setIgnored=false because they're benign, not truly ignored — keeps the audit log honest.
- Q: Is Apple dedup fully airtight? — A: It's time-based (eventTime vs LastNotifEventTime), NOT NotificationUUID-based. A redelivery with an EQUAL signedDate is not strictly Before the watermark, so it could re-process. In practice the transitions are idempotent-in-effect (re-writing the same terminal state), so it holds today. Nice hardening angle (code-verified): the raw material for a real idempotency key is already there — Apple's `notificationUUID` and Google's Pub/Sub `msg.ID` are both parsed and persisted into audit logs (`apple_notification` / `google_notification`), but they're only *logged* — never queried before processing, and there's no unique index on them. So the cheapest fix is a unique index on the stored id + a check-or-insert before processing, turning the implicit "writes happen to be idempotent" into an explicit near-exactly-once guard, using data we already store.
- Q: Google ordering under at-least-once unordered delivery? — A: `shouldIgnoreRTDN` uses `eventTime` (from `EventTimeMillis`) vs `LastNotifEventTime`, token match, and `LinkedPurchaseToken` chaining. Only type 4 (SUBSCRIPTION_PURCHASED) may create a brand-new relation when subRel==nil. Every write advances `LastNotifEventTime` as the watermark.
- Q: How does LinkedPurchaseToken avoid mistaking an upgrade for a foreign/stale event? — A: On upgrade/downgrade/resubscribe Google issues a new purchaseToken but sets `LinkedPurchaseToken` (read from the FETCHED subscriptionV2, not the RTDN) pointing at the old one, so we chain it to the existing relation's `google_purchase_token` instead of treating the token change as a stranger.

---

**Deep-dive — State machine: two status fields and the billing_retry design**

- Q: Why two status fields, not one? — A: `Status` (paid/free/unsubscribed) is the 3-valued coarse ACCESS gate — client unlocks premium iff `status==paid`. `PaidStatus` (active/grace_period/billing_retry/cancel/paused) is the fine BILLING lifecycle. A degraded record can have `Status=unsubscribed` (access off) yet `PaidStatus=billing_retry` (not dead) — the doc comment in `premium_entitlement.go` spells this out.
- Q: Why does StatusPaidToBillingRetry keep channel/sku/external_transaction_id/expire_at while StatusPaidToUnsubscribed wipes them? — A: billing_retry is recoverable: Apple retries billing up to 60 days, Google has recovery. Keeping the identifiers lets a later BILLING_RECOVERY / DID_RENEW restore `Status=paid` without a fresh purchase. `StatusPaidToUnsubscribed` is terminal — the empty `PaidStatus` is precisely what code reads as 'fully dead'.
- Q: How exactly is grace_period vs billing_retry decided? (code-verified, `service/subscription.go:2156`) — A: Inside `DID_FAIL_TO_RENEW` it's a two-way test on the notification subtype: subtype `GRACE_PERIOD` → `UpdateGracePeriod` (Status=paid, PaidStatus=grace_period, access ON); empty subtype → `BillingRetryPaidSubRel` (Status=unsubscribed, PaidStatus=billing_retry, access OFF). It does NOT read `isInBillingRetryPeriod` / `expirationIntent` to pick the branch. Three precise points: (1) `GRACE_PERIOD_EXPIRED` is NOT a subtype of DID_FAIL_TO_RENEW — it's a separate top-level notificationType (`service/subscription.go:2185`) that also routes to billing_retry (grace ended but Apple's ~60-day retry continues; EXPIRED is the real terminal event). (2) The grace branch has an edge condition: `UpdateGracePeriod` only sets grace_period if `GracePeriodExpiresDate` is still in the future (`GraceUtilAt.After(now)`); if already past, it writes GraceUtilAt but leaves Status/PaidStatus unchanged (no downgrade). (3) The DID_FAIL_TO_RENEW switch has NO else, so any other non-empty subtype is a silent no-op. Google side uses no subtype — it switches on RTDN integer codes: code 6 (in grace) → grace_period, code 5 (on-hold) → billing_retry.
- Q: Why the +2-day extra grace in IsPaidStatusExpired? — A: It's a safety net for lost/late store callbacks. baseExpireAt = max(GraceUtilAt, ExpireAt) + 2 days. Third-party callbacks are preferred; this only fires on the read path so a stale paid record self-heals to unsubscribed if the webhook was dropped — while the +2 days avoids racing a slightly-late legitimate notification.
- Q: paused? — A: `PaidSubscriptionStatusPaused` exists but `StatusPaidToPaused` currently sets `PaidStatus=billing_retry` with a TODO — clients can't render a real paused state yet, so it reuses billing_retry behavior.
- Q: What does one record actually store? (code + live-DB verified) — A: The `SubscriptionRelation` struct (`entity/paid_subscription.go`) backs both `nb_premium_subscription` and `subscription_relation`. Fields (bson): `_id`, `user_id`, `media_id`(omitempty), `in_free_trial`, `status`, `paid_status`, `channel`, `subscription_at`, `expire_at`, `grace_util_at`, `external_transaction_id`, `sku`, `metadata`(omitempty), `google_purchase_token`, `linked_purchase_token`, `created_at`, `updated_at`, `updated_source`, `newsletter_synced`(*bool omitempty), `env`(omitempty), `last_notif_event_time`(omitempty), `action_src`(omitempty). Live check: `nb_premium_subscription` holds ~38.7k docs; a real doc has only ~14 keys because omitempty zero-value fields don't persist. Live distinct `status` = {paid, unsubscribed, gift}; live distinct `paid_status` = {'', active, billing_retry, cancel, grace_period}. So billing_retry is real in prod, empty `paid_status` is the terminal 'fully dead' marker, and there's a `gift` access tier beyond paid/free. A real record showing the two-field design: `status=paid` + `paid_status=cancel` = user turned off auto-renew but keeps access until `expire_at`.

---

**Deep-dive — Persistence discipline: single writer, cache, locking, read consistency**

- Q: How is write consistency kept across ~8 transition methods? — A: Single-writer discipline. Every transition mutates the in-memory relation via an entity mutator, then calls exactly one private writer (`updateNbPremiumSubRel`): re-reads pre-image for audit, `$set UpdateOne` keyed on `user_id`, `DelNBPremiumSubRelCache`, then `insertNBPremiumSubRelLog(ori,new)`. One place owns mongo write + cache invalidation + audit log, so none can drift.
- Q: Cache design and penetration protection? — A: Cache-first `FindNbPremiumSubRelWithCacheAndUpdate` on key `nbpsr:<userID>`. Miss+not-found writes a `null` sentinel with 10min TTL (`ExpireAfterTenMinute`) to stop penetration; real docs cached 1h (`ExpireAfterOneHour`). Invalidation is delete-after-write, not write-through.
- Q: The delete-after-write race? — A: Between `updateNbPremiumSubRel`'s Del and a concurrent read repopulating, a reader could momentarily reload a stale value. Mitigations: the per-user lock serializes the mutating side, and reads that matter for freshness (scene=premium_paid) bypass cache entirely. It's a known write-invalidate tradeoff vs write-through.
- Q: Why per-user redis SetNX lock instead of a mongo txn / optimistic version? — A: The aggregate is one doc per user; contention is only same-user concurrent webhooks. A lightweight `nbprtdnlock:<userID>` SetNX (30s TTL, 3 tries, 500ms*(i+1) backoff) serializes read-modify-write cheaply without a distributed transaction. If not acquired after 3 tries the notification is returned unhandled -> store retries.
- Q: Is there a CAS / conditional write, or is the lock the only concurrency protection? (code-verified) — A: There is NO CAS — the lock is the ONLY thing protecting concurrent correctness. The private writer's filter is identity-only: `{user_id}` (media adds `media_id`), an unconditional `$set` overwrite (`subscription.go:5036/5403`); no `last_notif_event_time` in the filter, no version. The stale-event guard `eventTime.Before(LastNotifEventTime)` is an in-memory Go comparison against the value read at the start of the critical section, NOT a mongo filter condition — so it only holds because the lock serializes. Remove the lock and two concurrent notifications both read the old doc, both pass the in-memory check, both write unconditionally → lost update, and the DB won't reject the stale write. On top of that, `BuildUpdateFields` doesn't even persist `last_notif_event_time` back. So honest framing: the lock isn't a performance optimization on top of an atomic write — it's the sole correctness mechanism, with no DB-layer backstop, so a lock TTL timeout is an unguarded lost-update risk.
- Q: So what's the actual technical hard part here — isn't the lock just replaceable by a one-line conditional update? — A: Right, the lock+in-memory guard isn't the interesting part — a mongo conditional update (CAS: filter with `last_notif_event_time < mine`, a basic Mongo capability, not a new-version feature) would do the time/concurrency dimension better and atomically. The real hard part is the WHOLE problem: keeping subscription state correct on an out-of-order/duplicate/late event stream, which has NO single-line solution because it splits into two orthogonal dimensions. (1) Time dimension — same-transaction, newer-wins — CAS handles. (2) Business dimension — CAS can't: when there's no record who may create one, what to do when a cancel arrives before its subscribe, whether a B-resubscribe supersedes A across transactions. Killer example: reordered `cancel → A subscribe → B resubscribe` where cancel arrives while no record exists — CAS either matches 0 docs and no-ops, or upserts a wrong cancel record; the only correct handling is the business rule "only SUBSCRIBED may create when there's no record" plus a cross-transaction callback to Apple to adjudicate the active transaction. That business dimension has no one-liner — that's the difficulty. Framing: don't pitch "my lock design"; pitch "correctness on an unreliable event stream is two dimensions — CAS for time, business guard + platform callback for the rest."
- Q: TTL expires mid-processing, or userID lookup fails? — A: If the 30s TTL lapses mid-work another worker could enter — bounded by the fact writes are idempotent state sets. If AppAccountToken->userID resolves to <=0 the code proceeds WITHOUT a lock — a real correctness hole worth calling out. Read-your-writes: scene=premium_paid and the RTDN handler read PRIMARY (`api_master`), everything else secondary+cache.
- Q: Any real bug in the lock itself? (code-verified) — A: Yes, a textbook wrong-holder delete. Acquire stores a constant value `"1"` for every holder and release is an unconditional `DEL` with no ownership check (`repository/subscription.go:5053/5058`). Scenario: worker A runs past the 30s TTL (its critical section includes a Google API fetch + several mongo writes), the key auto-expires, worker B acquires the same key and starts processing, then A's deferred release `DEL`s B's lock — so a third worker can now run concurrently with B, breaking mutual exclusion exactly in the slow-processing case the lock exists for. Fix: store a unique per-acquire token and release via a Lua GET-compare-then-DEL. Two related gaps: no TTL renewal/watchdog (30s hard ceiling), and the Apple path skips locking entirely when userID==0. One thing done right: acquire is atomic (go-redis v8 `SetNX` with a non-zero expiry compiles to a single `SET key "1" EX 30 NX`), so there's no setnx-then-expire deadlock window.

---

**Deep-dive — Ending semantics: cancel vs expiry vs refund, and the SKU-divergent terminal state**

- Q: Cancel vs expiry vs refund in one sentence each. — A: Cancel (Apple DID_CHANGE_RENEWAL_STATUS+AUTO_RENEW_DISABLED / Google type 3) = soft end: `PaidStatus=cancel`, keep ExpireAt, Status stays paid to period end. Expiry (Apple EXPIRED / Google 13) = hard end at boundary. Refund/Revoke (Apple REFUND|REVOKE / Google 12) = immediate hard end / chargeback.
- Q: Why do expire and revoke land media in Status=free but NB Premium in Status=unsubscribed? — A: Same trigger, SKU-divergent terminal status. Media path calls `StatusPaidToFree` (Channel=free, clears expire/sku/txid) because a media follower falls back to a free relationship; NB Premium calls `StatusPaidToUnsubscribed` because there's no free tier to fall to — it's simply gone. Downstream consumers key off the different terminal status.
- Q: Refund on a user who already turned off auto-renew (PaidStatus=cancel, Status still paid)? — A: The Apple guard is `if subRel.Status != SubscriptionStatusPaid { skip }`. Here Status is STILL paid, so the guard is false -> RevokePaidSubRel runs and hard-ends it. Only a fully expired/revoked record short-circuits. Correct: a refund must revoke even a cancelled-but-still-active sub.
- Q: Why does Apple's refund arm have an in-switch already-revoked guard but Google doesn't? — A: Apple folds REFUND+REVOKE into one arm with `Status!=paid` skip. Google relies on `shouldIgnoreRTDN` instead — but its already-churned skip only fires when the incoming purchaseToken DIFFERS from stored AND the record is already unsubscribed with empty PaidStatus. A same-token type-12 replay on an unsubscribed record falls through to RevokePaidSubRel again — redundant but idempotent-in-effect (re-writing StatusPaidToUnsubscribed).
- Q: Why fold REFUND and REVOKE together despite different causes (chargeback vs family-sharing loss)? — A: Both mean 'lose access immediately' with identical DB effect; the real-world cause is captured in the audit log, but the state mutation is the same, so one arm avoids duplicated logic.
- Q: CONSUMPTION_REQUEST / REFUND_DECLINED? — A: Logged+acked, no state change. Risk of never answering CONSUMPTION_REQUEST: Apple uses consumption data to inform refund decisions, so declining to respond can bias refund outcomes against us — a known gap, not implemented yet.
- Q: Read side turns terminal state into UX how? — A: `premium.go getPremiumAdConfig`: `PaidStatus=cancel` -> RetentionPaywallTypeCanceled, and if ExpireAt within 3 days (3 min on stage/test) -> ExpiredSoon; grace_period -> GracePeriod; a once-premium now-non-premium user -> Expire.

**Takeaway options:**
1. The unifying design move is fan-in: three stores with very different delivery models (Apple signed-push, Google pointer-pull, Stripe webhook) all reduce to one Mongo aggregate per user and one private writer, so idempotency, ordering, caching, and auditing are solved once rather than per store.
2. Every entitlement decision is re-derived from an authenticated source — verify the JWS or re-fetch from the Play API — and separated into an access gate vs a billing lifecycle, which is what lets the system stay correct through renewals, retries, refunds, and lost webhooks.

---

### Story 2 — End-to-end UGC video upload pipeline (presign → anti-abuse → mp-api → review → read-back)
Themes: Technical depth / Problem solving, User / Product focus, Cross-team / full-pipeline ownership

**Situation**
We own the UGC video upload pipeline that lets a NewsBreak creator record a video and get it live in the feed. It spans four systems: the client, our Go `server` (the API tier under `/Website/ugcvideo`), the Java `mp-api` media platform (the state authority), and the downstream doc-ingest/review systems — CPP for serving-doc creation plus the `bagel`/`doughnut` audit teams. The hard part is that a single upload is a long, asynchronous lifecycle — presign, direct-to-S3 upload, publish handoff, transcode, dedup, CPP ingest, human/ML audit — and the creator is staring at a "my uploads" screen expecting to see processing → published → live, or failed/rejected, in near real time. On top of that we were standing up an anti-abuse layer for the upload route and mid-flight migrating the read-back endpoint to protobuf.

**Task**
We had to make this end-to-end pipeline correct and legible: keep large video bytes off our API tier, hand off cleanly to mp-api, make mp-api the single source of truth for post state, translate its internal state enums into client-facing status the creator understands, and read that state back live without staleness. Alongside that we had to add IP/bot anti-abuse on the upload route without risking false-blocking legitimate creators, and migrate the status read-back path to proto at parity. The guiding constraint throughout was fail-open: never block a real upload on an infra hiccup.

**Action**
- INGEST — presign, not proxy: client calls GET /Website/ugcvideo/init-ugc-video-upload; server does an early fail-open rate pre-check (mp-api /post/check_ugc_video_upload → 429+cooldown BEFORE the client wastes bandwidth uploading a whole video), then asks VIDEO_FEED_SERVER for an S3 presigned URL + a vuid. Client PUTs the raw bytes directly to S3 — the video never transits our API tier.
- ANTI-ABUSE — observe-only middleware on the submit route: GET /add-ugc-video-submission runs SessionAuth() then UgcAntiAbuse(), a Gin middleware that ports bloom-service's multi-layer rate-limiting into server but runs collect-only — it computes an IP bot score (6-aspect, 0-10) and a full smart-rate-limit decision, but a would-be block only emits a Warn log, never c.Abort(). Its real product effect is enriching the request with geo/ASN/botScore ip_info stashed under gin key ugc_ip_info for the controller to forward downstream.
- HANDOFF — publish to mp-api: server resolves/creates a media_id, derives the video URL from the vuid on a cloudfront origin, injects ip_info into cpp_params, and POSTs SendUgcVideoRequest to mp-api /post/publish_ugc_video via DoWithHeaderPassthrough (to preserve mp-api's 429). mp-api inserts the post row at state=POSTING(1), audit_status=UNREVIEWED(0), returns data.post_id, and starts async transcode.
- DOWNSTREAM — CPP + review over Kafka: CPP reports ingest outcome on Kafka topic mp_callback (consumed by CallbackService); success writes doc_id back and flips POSTING→POSTED(2), failure/duplicate/transcode-fail conditionally flip to POST_FAILED(4)/POST_DUP(9). The bagel/doughnut review teams publish verdicts on mp_audit_result, consumed by processAuditResult, which persists audit_status (REJECT=3 etc.), sends reject/report inbox messages, and refreshes the doc back through CPP. A CheckStuckPostingVideoJob sweeps a Redis ZSET every 10 min to force-fail anything stuck in POSTING >1h.
- STATE AUTHORITY — mp-api owns the truth: real state lives as raw int columns post.state / post.audit_status in MySQL (mp.nb.com), with a separate string-vocabulary audit history in Mongo creator_network_review_tasks. Terminal transitions use compare-and-set (updateReasonStateIf ... WHERE state=POSTING) for idempotency and to never resurrect a user-deleted post. Serving reads are fronted by short-TTL Redis on a single node.
- READ-BACK — server as live status translator: client polls GET /Website/ugcvideo/get-my-ugc-video-list; server pulls fresh per request (no intermediate cache) from mp-api /post/media/{id}/all_submitted_docs[_v2] — a time-ordered list where super_fresh_video placeholders carry mp_state and posted items carry doc_id. Posted docs are expanded via documentRepository.QueryDocs (the live, non-cached path) to read audit_status/reject_details. Server maps mp_state→banner (1 processing/4 failed/9 duplicate) and audit_status→banner (0 reviewing/3 rejected/1,2 live-no-banner), reassembling in mp-api's original order with the cursor advancing by len(items) so placeholders keep their slot.
- MIGRATION — proto at parity: get-my-ugc-video-list runs a dual path (X-Stage-Migration-Path:proto header on stage, AB flag proto_migration_get_my_ugc_video_list in prod) sharing identical fetch+expand+reassemble logic and differing only in bind/writer, so we could validate proto vs legacy map response field-by-field before flipping.

**Result**
- A clean separation where the API tier never carries video bytes — only the presign broker, the publish handoff, and the status read-back — keeping large-object traffic off the server fleet.
- mp-api is the single, authoritative state owner; every other system either reports into it (CPP, bagel/doughnut over Kafka) or reads live from it (server), so there is exactly one place the lifecycle is decided.
- Creators see near-real-time, correctly-translated status (processing/failed/duplicate/reviewing/rejected/live) because the read-back path reads live on every request and translates two distinct internal enums into one client vocabulary.
- Anti-abuse landed safely as observe-only: we get real bot-score/rate-limit signal against production traffic and geo/ASN enrichment today, with a clean path to flip to enforcement once the Warn logs validate the thresholds — zero false-block risk in the meantime.
- The proto migration ran behind a header/AB dual path with shared logic, giving us field-level parity validation before rollout.

**Follow-up ammo** — organized by deep-dive theme; each theme is a direction the interviewer can pull.

---

**Deep-dive — vuid binding & why bytes bypass the server**

- Q: How does the vuid tie the presign to the later publish? — A: init-upload returns {presigned_url, vuid} from video-feed-server; submit derives origin_video_url = https://dcusoqhsuupgy.cloudfront.net/origin/{vuid}.mp4, so mp-api ingests by reference to the S3 object the client already PUT. The vuid is the join key between the presign step and the publish step.
- Q: Why keep raw bytes off the API tier? — A: video is large; routing it through the Go server would blow up the API tier's bandwidth/memory. Presigned S3 PUT lets the client upload direct to S3; server only brokers a URL and later references it by cloudfront origin.
- Q: What stops a client publishing a vuid it never uploaded? — A: nothing at the server layer — server trusts the vuid; the downstream transcode step is where a missing/invalid S3 object surfaces as transcode failure → POST_FAILED. That's a real gap worth closing with an existence check, but today it degrades to a failed post, not a bad publish.

---

**Deep-dive — observe-only anti-abuse: mechanism and when to enforce**

- Q: Why observe-only instead of enforcing? — A: the logic is a verbatim port of bloom-service; before we let it Abort real creator uploads we want to validate the ported bot-scoring and 4-policy rate-limit against live traffic. A block path only logs Warn 'ugc_anti_abuse: would block (observe-only)'. We'd measure would-block volume, false-positive rate on known-good creators, and policy-band distribution from those logs before flipping.
- Q: Walk the 3-layer IP-info+score lookup. — A: Layer1 Redis GET ugc_ip_bot_score:<ip>; Layer2 Mongo bloom.ip_info FindOne{_id:ip} (compute+cache score 24h, async backfill if stale); Layer3 brand-new IP → async backfill, cache default score 5 for 1min, return immediately. Layer3 avoids a 2-4s ipinfo.io round-trip blocking the request.
- Q: Why increment counters BEFORE the IP-cooldown check? — A: so a banned IP's requests still inflate the user's counter. If we checked cooldown first and short-circuited, an attacker rotating IPs would reset their per-user record each time. Bumping counters first defeats IP rotation.
- Q: A score of 5 — which policies match? — A: 5 falls in medium_risk (4-6) and general (0-10); we pick ALL bands containing the score and sort most-restrictive-first (by MaxReqsPerUser, then MaxUsersPerIP, then UserBanDur, then IPBanDuration), so medium_risk's 10 reqs/user 5-min ban governs over general's 4 reqs/user.
- Q: setBanIfLonger — construct the bug it prevents. — A: a low_risk request (8m IP ban) arriving after a high_risk 6h ban would otherwise overwrite and shorten the active ban to 8m; the guard only overwrites when the new duration is longer, so the 6h ban survives.
- Q: ipFetchSema is a global semaphore of 50 with non-blocking select-default. Under a new-IP burst backfills are dropped — how does it converge? — A: dropped backfills leave the default-5 score cached for only 1min, so subsequent requests re-attempt; steady-state the fan-out drains and real scores get written with 24h TTL. It's eventually-consistent by design to protect the process from ipinfo.io fan-out.

---

**Deep-dive — two-stage rate limit + fail-open**

- Q: Why rate-limit twice (init pre-check + mp-api publish)? — A: the init check (mp-api /post/check_ugc_video_upload → 429+cooldown) stops the client before it uploads a whole video; mp-api still enforces the final hard limit at publish. The init check is a UX/bandwidth optimization, not the enforcement point.
- Q: What race does the init check NOT close? — A: a client can pass the init check, upload, then hit publish where quota is now exhausted — so publish must independently enforce. Init is advisory; publish is authoritative.
- Q: Why DoWithHeaderPassthrough only for publish_ugc_video? — A: a plain Post collapses all non-2xx into one 500, losing mp-api's 429. Passthrough preserves 429 → ErrorCodeUgcRateLimit(4003001, HTTP 429) so the client shows a cooldown instead of a generic error. Other paths don't need the distinction.
- Q: What's the fail-open exposure? — A: every layer passes on failure — redis nil→Allowed, mongo miss→treated as miss, ipinfo all-fail→default score, quota check errors→allowed, and UgcAntiAbuse is observe-only. So during infra degradation abuse can flow; we accept that to never block legit creators, and mp-api's publish quota is the backstop that isn't fail-open.

---

**Deep-dive — mp-api state model & compare-and-set transitions**

- Q: Why raw int columns with Java-constant enums, no DB enum? — A: PostState (POSTING=1,POSTED=2,DELETED=3,POST_FAILED=4,POST_DUP=9) and AuditStatus (UNREVIEWED=0,APPROVED=1,FEATURED=2,REJECT=3) are static-final ints on Post; no DB enum means consumers must know the mapping. Illegal transitions are prevented not by schema but by compare-and-set UPDATEs.
- Q: Why updateReasonStateIf ... WHERE state=POSTING for terminal transitions? — A: it solves two problems at once — (a) idempotency: a repeated transcode-fail/dup/timeout callback returns 0 rows and skips re-invalidating cache/re-notifying; (b) it never overwrites a user DELETE that happened in-flight, because DELETED≠POSTING so the WHERE fails.
- Q: CPP callback arrives after user deleted the post — trace it. — A: cppSuccess sees state==DELETED, calls documentService.deleteDocument to take the freshly-built doc offline, and writes doc_id with state=null so SQL IfNull(#{state}, state) preserves DELETED — it refuses to resurrect.
- Q: MySQL audit_status vs Mongo AuditTask.status are different vocabularies — reconcile. — A: MySQL audit_status is an int (REJECT=3) and is authoritative for serving; Mongo creator_network_review_tasks is an append-style audit-task history with string status (completed/pending/canceled) and review_result.status (online). 'Rejected' in MySQL and 'not online' in Mongo are deliberately distinct concepts; the serving decision reads MySQL.

---

**Deep-dive — Kafka callback + audit ingest + stuck-posting backup**

- Q: Why isolate prod/staging by mp_env payload field instead of separate topics? — A: stage and prod share the same Kafka + MySQL, so mp_callback/mp_audit_result are shared; each consumer compares payload mp_env against its own mp.env and early-acks mismatches. Risk: a misconfigured consumer group could cross-poison env; mitigated by the in-code env check on every message.
- Q: Three paths can fail a stuck upload (CPP-fail callback, transcode-fail, 1h timeout job) — how avoid double-notify? — A: all three go through updateReasonStateIf(POST_FAILED, ..., expectState=POSTING); whichever fires first flips state, the rest return 0 rows and skip the inbox notice + FAIL_STEP metric. The conditional UPDATE is the dedupe.
- Q: The stuck-posting job holds a Redis lock with TTL = its 10-min interval — what if a run exceeds 10 min? — A: the lock auto-expires and a second pod could start concurrently. It's tolerable because the work is itself idempotent (compare-and-set force-fail), but it's a real concurrency window; a longer lock TTL or lock renewal would be the fix.
- Q: Trace a duplicate-video publish end to end. — A: transcode result → duplicateVideo() finds dupPostId → updateReasonStateIf(POST_DUP=9, 'duplicate video', POSTING) + updateVideoDuplication(true) + inbox notice → invalidateNewPostCache → on read-back the row appears as a super_fresh_video placeholder with mp_state=9 → server maps to 'duplicate' banner.
- Q: Why skip NBScore recompute for source=doughnut? — A: doughnut (high-check) audits already scored the content, so recomputing is wasted work; bagel (creator-update-triggered) audits still need it. Misclassifying doughnut as bagel would just recompute redundantly — waste, not corruption.

---

**Deep-dive — live read-back & two-enum status translation**

- Q: Why read status live per request instead of caching? — A: upload/audit state changes fast (processing→success/failed, unreviewed→approved/rejected); a cached value shows stale status on the one screen where freshness matters most. Server is a pass-through source of truth; the tradeoff is per-request mp-api load + doc-store QueryDocs load.
- Q: Why QueryDocs directly instead of the cached GetDocuments path? — A: the cached path could return a pre-moderation snapshot; QueryDocs reads audit_status/reject_details fresh so a just-rejected doc shows 'rejected' immediately. Cost: bypasses cache, so it's real doc-store load per poll.
- Q: Why two translation functions? — A: mp-api models two axes — PostState/mp_state is the lifecycle of an item still processing/deduped (only on super_fresh placeholders, no doc_id yet), AuditStatus is the moderation outcome of an item that already became a real doc. superFreshBannerText handles the former, AuditStatusBannerText the latter; APPROVED(1)/FEATURED(2) both map to empty banner = live.
- Q: Why advance the cursor by len(items) not by expanded-doc count? — A: placeholders and failed-to-expand docs still occupy a slot in mp-api's ordered list; if we counted only expanded docs the offset would drift out of sync with mp-api's pagination and we'd re-fetch or skip items. Counting mp-api items keeps the cursor aligned.
- Q: v2 vs legacy divergence for the same posted doc? — A: v2 (cv≥262700, newsbreak, Android/iOS, ugc_flow=v2) emits explicit banner Text + MpState=2 + AllowRetry=false/Deletable=true to match the placeholder card shape; legacy reuses MpState as the status carrier (audit_status≤1→1 else audit_status) and sets no Text, preserving old client semantics.
- Q: mp-api all_submitted_docs times out vs a single doc fails to expand — how does it degrade? — A: a list-fetch timeout surfaces as an endpoint error (the whole page fails); a single doc that fails to expand is silently dropped from finalDocuments (not shown empty), so one bad doc doesn't break the page.

---

**Deep-dive — proto migration parity & rollout gating**

- Q: How do you validate proto vs legacy parity? — A: both paths share identical fetch+expand+reassemble and differ only in bind/writer, so we dual-run (X-Stage-Migration-Path:proto header on stage, AB flag in prod) and diff field-by-field. Most-likely-to-drift fields: mp_state, banner Text, offset/size, and repeated-empty ([] vs null) and int64-as-string in Resp.
- Q: Legacy vs proto response envelope? — A: legacy returns a map result{documents,offset,size} via core.WriteResponse; proto returns v1.GetMyUgcVideoListResp{code:0,status:success,result{...}} via WriteUnifiedResponse. Envelope code/status must be injected on the proto side to match legacy's map-writer behavior.
- Q: Why gate useV2 on cv≥262700 not the version field? — A: only newer NewsBreak Android/iOS clients render the v2 unified card shape (banner Text + fixed MpState=2). Older clients fall back to MpState-as-status-carrier. Gating on client version (CVGte) not the version field targets exactly the clients that can render it.

**Takeaway options:**
1. The whole pipeline is organized around one principle — a single state authority (mp-api MySQL post row) that everyone else reports into or reads live from — which is what makes an inherently async, multi-system lifecycle legible to the creator in near real time.
2. We de-risked two changes at once by making them non-destructive: anti-abuse shipped observe-only so we validate thresholds on real traffic before enforcing, and the proto migration ran a dual path sharing identical logic so we prove field-level parity before flipping.


---

## Part 5 — HM 面中文陪练区（mock 问答，非面试念稿）

> 这一区是中文 mock 陪练：模拟 HM 面顺着 premium 主力故事一层层深挖的问答。**不是面试念的稿**（面试说英文，念稿在 Part 1–4 和 Story 1）。这里记「面试官会怎么问 + 该怎么答」的完整版，方便回顾这条深挖链路怎么走。主力故事 = Story 1 subscription。所有回答用 we 做主语。

### Q1. 讲一下你做的项目吧

我讲订阅系统 —— 我最能从头讲到尾的一个。它有意思的地方在于：我们其实从来不碰钱，真正扣款的是 Apple/Google/Stripe，我们后端唯一知道的就是这些平台事后告诉我们的东西。而通知的到达方式各不相同 —— Apple 把签名通知 POST 到 webhook；Google 只给一个 Pub/Sub 指针，得拿它反查 Google API 才知道发生了什么；而且这些通知会乱序、重复、延迟几天甚至几个月才到（续费、取消、扣款失败、退款都是异步的）。所以真正的难点不是"处理一次付款"，而是在一个不可靠事件流上维持每个用户一份永远正确的订阅状态 —— 因为涉及钱，错了代价很大。

我负责整条链路，分四段：① 购买入口（三平台一个路由组，iOS 两步、Android 一步，核心是信任边界——绝不信客户端原始收据）；② + ③ 异步生命周期（Apple 签名 webhook / Google Pub/Sub pull，写库前都有排序守卫）；④ 落库与状态机（两个正交状态字段，能表达"扣款失败、访问先关、记录留着可恢复"的 billing-retry）；⑤ 结束（取消/到期/退款三种语义）。最后收敛成每个用户一条 Mongo 记录、一个统一写入口，所以幂等/排序/缓存/审计只解决一次。

**交付要点**：opener 不堆 jargon（不报函数名/枚举），钩子留给面试官挖；结尾抛可深挖方向（排序去重 / 双状态字段）。

### Q2. 讲一下整条链路，从用户点击订阅开始，到怎么落库

以 iOS 为例（最完整）：① 用户点订阅，客户端先调 prepare 接口 —— iOS 要两步是因为 Apple StoreKit 要求先由服务端签一个购买请求；prepare 里我们解析出买哪个 SKU + 做前置检查（是否已付费/是否在扣款重试），确认后返回签好名的购买请求。② 客户端拿它走 StoreKit 完成支付，Apple 返回一个签名过的交易凭证（JWS），发给 subscribe 接口。③ 服务端验真 —— 绝不信凭证内容本身，而是验签名：解析证书链做完整 x509 校验，验到我们内置的 Apple 根证书。④ 绑定到人 —— 从交易取出 Apple 账户 token，反查对应 userId，必须跟当前登录一致，否则拒绝（防把别人交易安到自己账号）。⑤ 落库 —— 解析 SKU 类型分流到对应产品，用 upsert 幂等写入 Mongo，每用户一条。Android 略不同，落库后还要回调 Google acknowledge（不确认 Google 会自动退款，ack 失败打 CRITICAL）。

到此"点订阅→落库"完成，但之后续费/取消/退款都是异步走另一条入口，最后收敛到同一条记录、同一个写入口。

### Q3. premium 相关 feature，是通过读数据库判断是否 premium 吗？用什么库存？为什么？

是，但不是每次直接打库，是 cache-first 的。存储是 Mongo —— 每用户一条记录，库 `subscription`，NB Premium 走 `nb_premium_subscription` collection，判断是否 premium 就是读 `status` 字段等于 `paid`。读路径缓存优先：`get-subscription-status` 先查 Redis（key `nbpsr:<userId>`），命中直接返回，没命中回源 Mongo 再回填；真实记录缓存 1 小时，"查无此人"缓存一个 null 哨兵但只存 10 分钟（防缓存穿透）；写侧 delete-after-write。

**为什么用 Mongo**：① 天然是"每用户一份聚合文档"，一条记录嵌了 channel/SKU/交易号/过期时间/两个状态字段等，读一次就是完整实体；② 写竞争极小，只有同用户并发 webhook，没跨行事务需求，用 per-user Redis 锁串行化就够；③ Newsbreak 后端本就大量用 Mongo。**为什么读加缓存**：读写严重不对称——写很少（续费/取消等事件一天量级不大），但"是否 premium"被大量 feature 高频读，热读挡在 Redis，Mongo 只在缓存失效/写入时被碰。

**主动讲的取舍**：delete-after-write 不是 write-through，存在窗口——写完删缓存但另一读请求正好回填旧值。兜底：per-user 锁串行化写侧；真正在意实时性的读路径（付费鉴权）直接读 Mongo 主库、绕开缓存。诚实提醒：如被追"是你选的 Mongo 吗"，说这是既有存储、你在其上做设计，别揽成选型。

### Q4. 续费、取消、扣款失败、退款，这些就是写路径吗？

是，全是写路径，且不走购买那个同步入口——这些是平台异步通知我们的。骨架：**验真 → 排序守卫 → 按事件分发状态语义 → 统一写入口落库**。

入口两个：Apple 签名 webhook / Google Pub/Sub pull。进来先验真（Apple 重验签名；Google 消息只有指针没状态，拿 token 回查 Google API 拿权威状态，后续写以查回的为准）。然后排序守卫（改任何状态前）——每条记录记一个事件时间水位线，进来事件比水位线旧就丢，挡住"旧事件覆盖新状态"。过守卫后按类型分发：续费=刷新过期时间恢复 active；取消（关自动续费）=软结束，`paid_status=cancel` 但过期时间不动、`status` 还是 paid，用到期末；扣款失败=进 billing-retry，访问关掉但记录完整留着（SKU/交易号/过期时间不动），Apple 重试扣款最长 60 天，扣成功一条续费通知就原地恢复；退款/撤销=硬结束立刻收回。最后所有事件收敛到同一个统一写入口：内存改记录 → 唯一私有写方法一次性做三件事（`$set` 更新 Mongo + 删缓存 + 写前后对照审计日志），同用户并发通知用 per-user 锁串行化。

### Q5. 排序守卫具体怎么判乱序/重复？

**用什么时间判**：平台自己签名的事件时间（Apple signedDate / Google eventTime），不是我们的接收时间（避免我们多台服务器时钟不同步）；每条记录存 `LastNotifEventTime` 当水位线，每处理一个事件推进它。

**具体分支**（Apple 守卫，改状态前跑）：① 本地没这条记录 → 只有"新订阅"能创建，其它忽略；② 交易号跟存的同一笔 → 事件时间严格早于水位线判过期跳过，否则处理；③ 交易号不同 → 只有"新订阅"能重新指向（如重新订阅），其它丢；④ 两种已知良性乱序（宽限期到期、关自动续费）标成"跳过但不算异常"，不污染审计。Google 同理，多一层 `LinkedPurchaseToken` 链式关联——升降级/重订阅时 Google 发新 token 但带 linked token 指向旧的，靠它接回已有记录，不误判成陌生事件。

**主动讲的缺口**：去重是基于时间的，不是基于通知唯一 ID。若一条通知被重投且事件时间完全相等，不会"严格早于"水位线，可能被再处理一次。目前没事是因为状态转换本身幂等（重复处理"续费到同一过期时间"落在同一处）。代码核实后更精确的说法：通知唯一 ID 其实已经在存了——Apple 的 `notificationUUID`、Google 的 Pub/Sub `msg.ID` 都被解析并写进审计日志表（`apple_notification` / `google_notification`），但**只落日志、处理前从不查、也没建唯一索引**（唯一查过 UUID 的地方是离线统计，不是线上处理）。所以最低成本的 hardening 是：给存下来的 ID 建唯一索引 + 处理前做 check-or-insert，把"写入恰好幂等"这个隐式保证升级成显式的近似 exactly-once ——原料已经在手里，只差临门一脚。

### Q6. 怎么判断"本地还没有这条记录"？

关键是用什么 key 去 Mongo 查。**Apple 侧用 `originalTransactionId`**——同一条订阅一生不变的 ID（续费/宽限/恢复都是同一个，只有重新订阅才换），拿它匹配记录里的 `external_transaction_id`，查到非空、查不到就是 nil（本地还没有）。**为什么不用 userId 查**：Apple server-to-server 通知不带我们的登录态，异步链路只能靠交易身份定位，不能靠人（购买那条同步链路才有登录态，靠 AppAccountToken 反查 userId 绑定）。Google 侧类似，用 purchaseToken + LinkedPurchaseToken 链式关联匹配 `google_purchase_token`。

"记录不存在"时守卫规则：只有"新订阅"(SUBSCRIBED)能创建，其它事件（续费/取消/退款）直接忽略——一个续费事件连原始记录都没有，一定是乱序或异常，不该凭它凭空造记录。这也兜住了"续费比新订阅先到"的乱序：续费先到、记录不存在就丢掉，等新订阅到才建，靠"只有 SUBSCRIBED 能创建"+ 平台会重投。

### Q7. Mongo 里一条记录存了什么？（code + 实查双验证）

`SubscriptionRelation` struct（`entity/paid_subscription.go`）同时背 `nb_premium_subscription` 和 `subscription_relation` 两个 collection。完整字段（bson）：`_id`、`user_id`、`media_id`、`in_free_trial`、`status`、`paid_status`、`channel`、`subscription_at`、`expire_at`、`grace_util_at`、`external_transaction_id`、`sku`、`metadata`、`google_purchase_token`、`linked_purchase_token`、`created_at`、`updated_at`、`updated_source`、`newsletter_synced`、`env`、`last_notif_event_time`、`action_src`。

**实查结果**（连 stage 同集群实查 `nb_premium_subscription`）：
- 规模 ~38,712 条。
- 真实文档只有 ~14 个 key——带 omitempty 的字段零值时不落库，所以"记录存什么"取决于走过哪些路径。
- 线上实际 `status` 值 = {paid, unsubscribed, gift}（多了个 `gift` 赠送层，光看代码没确认到）。
- 线上实际 `paid_status` 值 = {'', active, billing_retry, cancel, grace_period}——印证 billing_retry 真在生产用；空字符串 `''` = terminal"彻底结束"标记；没有 paused（印证 paused 目前复用 billing_retry）。
- 一条真实记录印证双字段设计：`status=paid` + `paid_status=cancel` = 用户关了自动续费但保留访问到 `expire_at`。

### Q8. 这个项目最难的地方是哪里？为什么？

> 两个角度都能撑起"最难"，主答**并发锁**（取舍最硬），备答**状态机设计**。都用 we，机制全是代码核实过的真事，只补了"决策/权衡"叙事。⚠️ 别说影响用户数这类没核实的数字。

**主答 —— 并发控制（per-user 锁的权衡）**

最难的是保证同一个用户的并发通知不会互相踩。听起来是标准问题，但每个现成方案都有坑，最后是在几个都不完美的选项里权衡。

为什么有并发：同一个用户，平台可能极短时间内发来多条通知（扣款失败紧跟重试成功，或一条被重投）。我们的处理是读-改-写：读出当前记录、按事件算新状态、写回去。两条通知同时进来，两个 goroutine 同时读到旧记录、各自算各自写，后写的覆盖先写的——经典 read-modify-write race，而这是钱，覆盖错就是付费状态错。

难在选方案，每个都有代价：① 数据库事务——我们是 Mongo，且本质是"同一用户一条文档"上的竞争，上分布式事务太重，不值；② 乐观锁/版本号——能用，但通知处理里有对外部平台的调用（Google 要回查 API），把外部调用夹在乐观重试循环里，重试代价高还可能对平台重复请求；③ 最后选 per-user Redis 分布式锁（SetNX）——处理某用户通知前先抢这个用户的锁，粒度是每用户一把，不同用户完全不阻塞，只有同一用户的并发才串行化，竞争面压到最小。

但锁本身又带来一串新难题（真正花时间的地方）：① 锁要设 TTL（30 秒），否则进程拿锁崩了用户被永久锁死；但 TTL 一设，处理超时锁自动过期后另一个 goroutine 又能进来——我们能接受这个窗口，是因为状态写入本身幂等，最坏重复写一次同样结果，但这是"用幂等兜底锁的不严谨"，得想清楚兜底成立才敢这么设。② 抢不到锁：重试 3 次带退避，还抢不到就不处理、当失败——平台会重投，与其死等不如放回去等下次投递。③ 一个我特意标出的漏洞：如果通知连用户 ID 都解析不出来，代码会不加锁直接往下走，这是真实正确性缺口，列进要修清单。

所以最难的不是"加个锁"这个动作，而是在"事务太重、乐观锁跟外部调用冲突、悲观锁又要处理 TTL/超时/抢不到"这几个都不完美的选项里，判断出"per-user Redis 锁 + 幂等兜底 + 靠平台重投"这个组合代价最小又刚好够用。并发难的从来不是让它对，是让它在所有边界情况下都对、还不过度设计。

**备答 —— 状态机设计（两个 status 字段）**

最难的是想清楚为什么一维状态不够、要拆成两个正交字段。一开始很自然会想用一个状态枚举描述订阅，但很快发现表达不了 billing-retry 这种情况：扣款失败时，用户的访问权要关掉，但记录绝不能删——因为 Apple 会重试扣款最长 60 天，扣成功了要能原地恢复。这就是一个"访问已关、但订阅还活着可恢复"的状态，一维枚举里放不下：你说它 active 不对（访问关了），说它 expired 也不对（其实还能救）。

所以我们拆成两个正交字段：`status`（paid/free/unsubscribed）是粗粒度访问开关，客户端只读这个判是否解锁；`paid_status`（active/grace_period/billing_retry/cancel）是细粒度账单生命周期。billing-retry 就表达成 `status=unsubscribed`（访问关）+ `paid_status=billing_retry`（记录还活着）。这个设计还有个额外好处：所有下游"status==paid 才给权限"的判断一行代码都不用改就自动把 billing-retry 当作无权限，向后兼容；而需要"知道为什么、能不能救"的挽留流程去读 paid_status。难点就在于把"能不能访问"和"账单处于什么阶段"这两件本来被耦合在一起的事识别出来、拆开——想清楚这个正交分解，后面所有边界状态（取消但没到期、宽限期、扣款重试）就都能干净表达了。

### Q9. premium 系统的技术难点（并发控制这条线，当一个故事讲）

> 这是 premium 主力故事的"技术深度"总弹药，讲成一个连贯的故事：从并发问题 → 选方案 → 锁不可靠 → wrong-holder delete bug 及修法 → 幂等兜底。全部代码核实过。⚠️ 讲 bug 用"我分析代码时发现的缺陷"框，不是"我 debug 出来的事故"；是现有系统缺陷、不是已修复的功劳，被追"修了吗"就说"识别出来了、列进 hardening 清单、还没排上"。乱序去重是另一条正交的线，不放进这个故事（见 Q5）。

这个系统真正的技术难点，我会讲并发控制这条线，因为它是每个方案都有坑、需要一路权衡下来的。

起点是一个并发的 read-modify-write race。同一个用户，平台可能在极短时间内发来多条通知——比如一条扣款失败紧跟一条重试成功，或者一条通知被重投。我们处理每条通知的方式是读-改-写：读出这个用户当前的订阅记录，根据事件算出新状态，再写回去。问题是如果两条通知同时进来，两个 goroutine 会同时读到同一份旧记录，各自算、各自写，后写的把先写的覆盖掉。而这是钱，覆盖错了就是付费状态错了——要么白送权限，要么把付费用户踢掉。所以得让同一个用户的通知串行处理。

接下来是选方案，我们在几个都不完美的选项里权衡。第一个想法是数据库事务，但我们用的是 Mongo，而且这本质上只是"同一个用户一条文档"上的竞争，为这么小的竞争面引入分布式事务太重、不划算。第二个想法是乐观锁、加版本号——读的时候记版本，写的时候比对，冲突了就重试。这个能用，但有个致命问题：我们的通知处理里夹着一个对 Google 的 API 回查（Google 的通知只给一个 token，得拿它反查真实状态），乐观锁一旦冲突就要重跑整个处理，等于重复调用外部平台，代价很高还可能触发限流。所以最后选了悲观锁——用 Redis 的 SetNX 做一把 per-user 的锁，key 是 `nbprtdnlock:<userID>`。粒度是每个用户一把锁，好处是不同用户之间完全不阻塞，只有同一个用户的并发通知才串行化，把竞争面压到了最小。

但真正难的地方是在选完锁之后——带 TTL 的分布式锁根本不保证互斥。锁必须设 TTL（我们设了 30 秒），否则一个持锁的进程崩了，这个用户就被永久锁死。可是 TTL 一设就有一个根本性的问题：如果处理时间超过了 TTL，锁会在你还在处理的时候自动过期，另一个 worker 就能进来。而我们的临界区偏偏不短——里面有对 Google 的网络回查、还有好几次 mongo 读写，在 mongos 压力大的时候超过 30 秒完全可能。所以这里我意识到一件事：带超时的分布式锁不是一个可靠的互斥锁，它只能用来降低竞争、减少重复工作，不能拿来做正确性的保证。

顺着这个往下看锁的具体实现，就能发现一个很经典的 bug——wrong-holder delete，误删别人的锁。我们加锁的时候，存进去的 value 是一个固定的字符串 `"1"`，每个持有者都一样；释放锁的时候是无条件地 `DEL` 这个 key，完全不检查是不是自己加的。把这两点和上面的 TTL 过期放在一起，就能构造出一个真实的失败场景：Worker A 抢到了用户 X 的锁，开始处理，但因为临界区里那个 Google 回查慢、加上几次 mongo 写，处理时间超过了 30 秒；到 30 秒时，锁自动过期了；这时 Worker B——另一条同一用户的通知——SetNX 成功，拿到了这把锁，开始处理；然后 Worker A 终于处理完，走到 defer 里的释放逻辑，无条件 `DEL` 掉这个 key——可这个 key 现在是 B 的锁。于是 B 的锁被 A 删掉了，第三个 Worker C 又能立刻抢到，跟 B 并发地跑同一个用户的读-改-写。结果就是：锁本来是为了保护"处理慢"这种情况，可它恰恰在处理慢的时候失效了，互斥被彻底破坏。

怎么修？根因是"释放锁的时候无法判断这把锁还是不是自己的"。正确做法是给锁一个身份：加锁的时候不再存固定的 `"1"`，而是存一个每次获取都唯一的 token（比如一个随机 UUID）；释放的时候不能直接 `DEL`，而要用一段 Lua 脚本原子地做"先 GET 出当前 value，只有它等于我加锁时存的 token，才 DEL"。这样即使我的锁已经因为超时过期、被别人重新抢走了，我释放时一看 value 已经不是我的 token 了，就不会误删。为什么必须用 Lua 而不是"先 GET 再 DEL"两步——因为 GET 和 DEL 之间又有一个新的竞争窗口，只有把比较和删除放进一个原子脚本里才安全。除此之外还有两个相关的缺口：一是这把锁没有续期机制，30 秒是硬顶，没有 watchdog 去延长它，所以只要处理超时就一定会失去互斥——更彻底的修法是起一个后台协程定期续期，或者干脆用 redsync 这类自带 token 校验和 watchdog 的成熟库；二是 Apple 那条路径上，如果通过 AppAccountToken 查不到 userId，代码会跳过整个加锁块、直接处理，等于完全没有并发保护。（唯一做对的一点是：加锁是原子的——go-redis 的 SetNX 带一个非零过期时间，底层编译成单条 `SET key "1" EX 30 NX` 命令，所以不存在"先 SETNX 再单独 EXPIRE、中间崩了就永久死锁"那个更老的坑。）

所以这条线收尾我会落到一个认知上：在这样一个输入不可靠、又有并发、锁本身还会失效的环境里，我们其实没有依赖任何单一机制的绝对可靠。锁只是用来减少竞争和重复工作，真正兜住正确性的是**状态写入本身是幂等的**——即使因为锁过期，两个 worker 真的同时进来了，重复处理一条"把订阅续到同一个到期时间"的事件，最终落在同一个结果上，没有副作用。难的不是加一把锁，而是想清楚"锁一定会在某些情况下失效"，然后把系统设计成锁失效时依然正确，而不是假设锁永远生效。

**⚠️ 代码核实后的准确现状 + 难点该怎么定位（比上面这段更新、更准，面试按这个来）：**

核实结论：**当前没有 CAS，锁是唯一的并发保护、不是性能优化**。写入 filter 只有 `{user_id}`、无条件 `$set` 覆盖（`subscription.go:5036`）；排序守卫的时间判断是 Go 内存里比"进临界区读到的值"，不是 mongo 条件；`last_notif_event_time` 甚至没持久化回 mongo。所以拿掉锁就直接 lost update，锁超时=无兜底的 lost update 风险。上面那段说"正确性靠状态写幂等兜"其实是理想化了——当前并没有显式设计这层兜底。

**所以别把"并发锁"当技术难点讲**：它的时间/并发维有一行更优解——mongo 的条件更新（CAS，filter 加 `last_notif_event_time < 我的`，是 mongo 最基础能力不是高版本特性），HM 一句"为什么不用条件更新"就把它秒了。

**真难点该定位成"乱序/重复/延迟的平台事件流上维持付费状态最终正确"这个整体**——它没有一行解，因为分两个正交维度：① **时间维**（同交易谁新才覆盖）——CAS 能原子解决、并发也一起解决；② **业务维**（无记录时谁能建、cancel 先于 subscribe 到达怎么办、B resubscribe 取不取代 A 的跨交易归属）——CAS 解不了，得靠业务守卫 + 回调 Apple 裁决。**杀手锏例证**：乱序 `cancel → A subscribe → B resubscribe`，cancel 先到时记录还不存在——CAS 要么匹配 0 条什么都不做、要么 upsert 出一条错误的 cancel 记录，只能靠"无记录只准 subscribe 建"的业务规则 + 跨交易回调解。用这个 case 堵 HM 的"CAS 不就行了"。

**讲法**：难点=乱序整体正确性（展开、举 cancel-先到 case 证明没简单解）；并发锁退成"被追问到才诚实带出的一个次优环节"——"当前它其实是唯一保护、没 CAS 兜底，我 review 后意识到该把时间维下沉成 mongo 条件更新"。姿态是设计者的成长/Dive-Deep（我理解这难题的多个维度和当前实现边界），不是"我的锁设计很妙"（会被 CAS 秒），也不装成"当初权衡好的 trade-off"（当时没上 CAS、没做那个权衡，是直觉加锁）。压轴句："乱序事件流的正确性是两个维度——时间维用 mongo 原子条件更新落到数据库、锁只是优化；业务维（无记录建记录、跨交易归属）没有单点解，得靠业务守卫加回调平台裁决。"
