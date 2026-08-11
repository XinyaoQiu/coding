# TikTok BQ — Agent Notes（内部规则，勿当面试内容）

> 面试稿是**两份平行的念稿**，结构一一对应（章节数必须相等）：
> - **`tiktok_bq_ch.md`** —— 中文念稿。面试官可能讲中文，这是真会念出口的稿子。
> - **`tiktok_bq_en.md`** —— 英文念稿，按中文稿逐节对译。
>
> 两份都按 **HR 面 / Coding 面 / HM 面** 三轮组织，都不含策略提示。本文件放 agent 规则、策略提示，以及底部的 Story bank（深挖弹药，不是念稿）。

## Agent Notes（内部规则，勿当面试内容 / 给 agent 读的，不是给用户读的）

生成/修改面试稿时遵守：

1. **两份稿必须同步**：改了中文稿就要同步改英文稿，反之亦然。章节标题一一对应，数量相等——改完用脚本比对一遍。
2. **念稿里不放策略提示**：⚠️ 红线、"不主动讲"、"口径提醒"这类全部留在本文件。念稿里只有能直接说出口的话。
3. **英文稿要求口语化**：短句，避免长难句和复杂词（不用 mitigate / orchestrate / leverage 这类）。代码内容和变量名保留原样（`status`、`paid_status`、`originalTransactionId`、`findOneAndUpdate` 等）。
4. **自我介绍三轮共用同一底稿**：完整版复制三遍，只改结尾——HR 停在实习；Coding 落"我们直接开始吧"；HM 落"我最能从头讲到尾的是 premium 订阅系统"。
5. **⚠️ TikTok 特点 —— 挖细节**：面试官追问极深，看重具体技术细节 + 决策链条。主叙事保持克制，深挖内容下沉到 follow-up 或本文件底部的 Story bank。
6. **收录范围**：只写用户明确要准备的题/故事，不擅自加题。
7. **技术口径基线**：premium 那条以 `~/Project/.claude/scripts/apple_notification_handler.go` 为准。状态机取值：`status` = paid/free/unsubscribed；`paid_status` = active/cancel/grace_period/billing_retry/expired。**注意 Story bank 里有些细节是旧版实现，和 handler 不一致时以 handler 为准。**

### TikTok BQ 常见主题清单（打标签用）

- **Impact / Ownership** —— 主动扛下、把事做成、可量化的业务影响。
- **Technical depth / Problem solving** —— 复杂技术问题、根因排查、系统设计取舍。
- **Conflict / Collaboration / Cross-team** —— 跨团队协作、意见分歧、如何对齐。
- **Ambiguity / Fast pace** —— 需求不清 / 快节奏下如何推进（TikTok 强调 speed）。
- **Failure / Learning / Growth** —— 失败、错误、复盘与改进。
- **Prioritization / Trade-off** —— 资源/时间有限下的取舍。
- **User / Product focus** —— 从用户或产品价值出发做决策。

### 策略提示区（怎么用念稿 —— 念稿里不放这些，全写这里）

**Self-intro（三轮通用）**：学历 → Newsbreak 全职（说得比实习多）→ 字节/TikTok（TTOP 组，与 TTLS 合作）+ Alibaba/Tesla 实习各一句 → 结尾落 fast-paced + real-world impact。**字节那段必须讲**——面的就是这家，"待过还想回来"是留存信号，也是"为什么想来"最硬的论据；省略 UMTRI。毕业时间过去时（2025-12 已毕业）。`[Name]` 是占位。oncall 挂在 Newsbreak 不是实习。按轮次换结尾：HR 落 career goals；技术面落进题；HM 落项目。

**Part 1 HR — career goals**：定位「深耕后端 + 大规模系统」（专注有方向感），不是转方向。三点：①我是谁/做过什么 ②想往哪走 ③why TikTok(scale 对上)。真诚 > 流利。

**Part 1 HR — why leaving Newsbreak / why now**：⚠️ 用户**已被 layoff**（投简历时还在职）。四段结构：① 主动澄清状态差（简历写在职、现已离职，别等背调查出来）② 一句带过组织调整，不抱怨、不解释细节、不主动强调「不是绩效原因」 ③ 转成 pull factor ④ 落到「非常想来」。

pull factor 口径：**别说「流量/规模高一个量级」**——面的组未必 toC，说了可能和事实矛盾，也显得假。要说的是「一个人 own 整条链路（开发/测试/联调/infra）→ 天花板是没人 review 设计，不知道哪些做法是真对、哪些只是恰好没出问题 → 想要工程体系更成熟 + 问题本身够难的团队」。这个理由对任何组都成立。

红线——不说「现在工作没 impact」「正在积极找」（Applied 那次的说法）；**也别说「我挺喜欢现在的团队」**（会让人担心给了 offer 也不来）。想去大公司可提，但必须紧跟具体技术理由，别裸说。

**表达兴趣**：光说「非常想来 / 喜欢价值观」没说服力，必然被追问细节。用两张硬牌：① **字节实习经历**（TTOP 组，"待过还想回来"= 留存信号，被追问有真实经历可答）② 匹配度具体化（"分布式系统正确性正是贵司做得最深的一类问题"）。面试前查到具体组的方向就替换掉通用表述，整段说服力压在这一句上。

**Part 1 HR — work authorization**：身份 F-1 OPT first year（STEM）。策略：先讲 runway（现在能合法工作 + STEM extension 约 3 年），再讲未来需要 sponsorship。别只说"需要 H1B"（听起来像马上要）。

**Part 1 HR — 反问**：HR 通常开头已介绍岗位/team，别再问"team 做什么"（显得没听），顺着往下挖 or 问流程。

**Part 3 HM — project opener delivery**：opener 里不用 jargon（no RTDN / SetNX / cert-chain / compare-and-set 这类），留给 follow-up 深挖；紧张就只说 Core + close。Story 全程用 we 做主语，不做 mine-vs-teammates 区分。

**Story bank（本文件底部）**：两个全链路 story（Story 1 subscription / Story 2 video upload）是深挖弹药，**不是念稿**——念稿里的对应内容在两份稿的 Part 3。按 deep-dive theme 组织，面试官往细里追时从这里取料。subscription 偏分布式正确性/三平台，video upload 偏全链路+用户视角+跨系统。⚠️ 部分内容是旧版实现，和 handler 冲突时以 handler 为准（见规则 7）。

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

**Segment 2 — Apple async lifecycle (signed webhook).** Renewals/refunds/cancels arrive at `POST /notification/apple-notification` — NO auth middleware, because the JWS signature IS the auth. Body is one field `signedPayload`. We re-run the same x5c chain-verify, decode the nested `signedTransactionInfo`+`signedRenewalInfo`, translate `AppAccountToken` to a userID (no session on this path), load the relation by `user_id`, adjudicate cross-transaction ownership with Apple when the record names a different transaction, take the per-user lock, then run `shouldIgnoreAppleNotification` BEFORE the type switch and dispatch on `notificationType`+`subtype`. Every notification is persisted to `AppleNotificationLog` via a deferred write regardless of outcome. Return HTTP 200 on ignore/no-op so Apple stops retrying — but non-2xx on genuine technical failure (lock unavailable, Mongo error, Apple callback failure), since Apple's retry is purely transport-level and swallowing an error into a 200 drops the notification permanently.

**Segment 3 — Google async lifecycle (pointer + fetch, via Pub/Sub PULL).** Unlike Apple's push webhook, on boot (only on clusters `server-a4api-push`/`server-a4api-stage`) we start a goroutine that PULLs RTDN from Pub/Sub sub `paid-subscription-billing-sub`. The RTDN is a thin pointer (packageName/type/purchaseToken/eventTimeMillis, NO state). So for every message we re-fetch authoritative state with `Subscriptionsv2.Get(purchaseToken)` and drive ALL writes off that fetched object. userID comes from Google-echoed `ObfuscatedExternalAccountId`. `shouldIgnoreRTDN` handles stale/out-of-order via `LastNotifEventTime`+`LinkedPurchaseToken` chaining. Handler error -> `msg.Nack()` (redeliver); benign skip -> return nil -> `msg.Ack()`.

**Segment 4 — Persistence + state machine + read path.** Every transition is a repo method that mutates an in-memory `SubscriptionRelation` via named entity mutators, then funnels through ONE private writer (`updateNbPremiumSubRel` / `updateSubscriptionRelation`) that does `$set UpdateOne` keyed on `user_id`, deletes the redis cache, and appends a before/after audit log. Two orthogonal fields: top-level `Status` (paid/free/unsubscribed) = coarse access gate the client reads; `PaidStatus` (active/cancel/grace_period/billing_retry/expired) = fine billing lifecycle. Concurrent notifications per user serialized by a redis SetNX lock (30s TTL, 3 tries). Reads go cache-first (`FindNbPremiumSubRelWithCacheAndUpdate`) with lazy self-healing expiry; `get-subscription-status` served from here.

**Segment 5 — Ending (cancel vs expiry vs refund).** All store callbacks converge on `SubscriptionRelation` with three mutation semantics. Cancel (auto-renew off) = soft end: `PaidStatus=cancel`, keep `ExpireAt`, `Status` stays paid until period end. Expiry = hard end at boundary. Refund/Revoke = immediate hard end. Terminal status differs by SKU: media degrades to `Status=free` (`StatusPaidToFree`), NB Premium to `Status=unsubscribed` (`StatusPaidToUnsubscribed`). `IsPaidStatusExpired` adds +2 days grace as a lost-webhook fallback. Read side (`premium.go`) turns `PaidStatus=cancel` into a retention paywall (`Canceled`, or `ExpiredSoon` within 3 days of `ExpireAt`).

**Result**
- End-to-end we built one subscription pipeline that serves three stores and multiple products (NB Premium, creator/media subscriptions, NutriScan) through a single route group and a single Mongo aggregate per user, with the store-callback fan-in reduced to one record type and one private writer.
- Trust is anchored server-side on every path: iOS/Apple JWS is chain-verified against a pinned Apple Root CA-G3 and bound to the session user via AppAccountToken; Android/Google is verified by a live `Subscriptionsv2.Get` and acknowledged back; the async webhooks re-verify (Apple) or re-fetch (Google) rather than trusting the message body.
- The lifecycle is idempotent and ordering-safe under at-least-once, unordered delivery, via four mechanisms that each cover one failure class and none of which subsumes another: a per-user redis lock for concurrency, a per-relation `LastNotifEventTime` watermark for reordering **within** a transaction, a callback to the platform for ownership **across** transactions (where the timestamps stop being comparable), and a `notificationUUID` claim for redelivery. Plus idempotent upserts and Ack/Nack semantics that tie Pub/Sub redelivery to handler success — benign skips Ack, real failures Nack and retry.
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

- Q: Walk shouldIgnoreAppleNotification precisely. — A: Order matters — ownership first, recency second. (1) subRel==nil -> only SUBSCRIBED processed, everything else ignored+markIgnored. (2) different externalTransactionID -> timestamps aren't comparable across transactions, so the caller has already adjudicated with Apple before we get here; process and re-point. (3) same transaction + `PaidStatus=expired` -> GRACE_PERIOD_EXPIRED and DID_CHANGE_RENEWAL_STATUS+AUTO_RENEW_DISABLED are benign-skip (ignore=true, setIgnored=false); scoped to the live transaction so a cross-transaction straggler still leaves a trail. (4) same transaction -> ignore if `eventTime.Before(subRel.LastNotifEventTime)`, else process.
- Q: Why is the watermark checked last rather than first? — A: It only means something within one transaction, where Apple orders the signed dates. Checking it before the ownership branch would let a late SUBSCRIBED for a superseded transaction bypass it, rolling the record back to a dead subscription — by type alone that is indistinguishable from a genuine resubscribe.
- Q: The function returns two booleans — difference? — A: First = ignore/skip processing; second = setIgnored, i.e. whether to mark the log row `Ignored=true`. The grace-period race cases return ignore=true but setIgnored=false because they're benign, not truly ignored — keeps the audit log honest.
- Q: Is Apple dedup fully airtight? — A: It's time-based (eventTime vs LastNotifEventTime), NOT NotificationUUID-based, and the watermark stops **no** redeliveries at all: a redelivery carries an identical signedDate, which is not strictly Before the mark, so it passes. Tightening to `!After` would reject same-millisecond distinct notifications instead — losing a real event is far worse than reprocessing one, so the conservative form stays. It holds today because the transitions are idempotent-in-effect (re-writing the same target state). A real idempotency key would be a separate Redis claim keyed on `notificationUUID` with a TTL covering Apple's full retry window (five attempts at 1/12/24/48/72h ≈ 157 hours, so 7 days). Note the audit log is **not** the place for it: it deliberately carries no unique index, because duplicate rows there are the only record of how often a notification was redelivered.
- Q: Google ordering under at-least-once unordered delivery? — A: `shouldIgnoreRTDN` uses `eventTime` (from `EventTimeMillis`) vs `LastNotifEventTime`, token match, and `LinkedPurchaseToken` chaining. Only type 4 (SUBSCRIPTION_PURCHASED) may create a brand-new relation when subRel==nil. Every write advances `LastNotifEventTime` as the watermark.
- Q: How does LinkedPurchaseToken avoid mistaking an upgrade for a foreign/stale event? — A: On upgrade/downgrade/resubscribe Google issues a new purchaseToken but sets `LinkedPurchaseToken` (read from the FETCHED subscriptionV2, not the RTDN) pointing at the old one, so we chain it to the existing relation's `google_purchase_token` instead of treating the token change as a stranger.

---

**Deep-dive — State machine: two status fields and the billing_retry design**

- Q: Why two status fields, not one? — A: `Status` (paid/free/unsubscribed) is the 3-valued coarse ACCESS gate — client unlocks premium iff `status==paid`. `PaidStatus` (active/cancel/grace_period/billing_retry/expired) is the fine BILLING lifecycle. A degraded record can have `Status=unsubscribed` (access off) yet `PaidStatus=billing_retry` (not dead) — the doc comment in `premium_entitlement.go` spells this out.
- Q: Why does StatusPaidToBillingRetry keep channel/sku/external_transaction_id/expire_at while StatusPaidToUnsubscribed wipes them? — A: billing_retry is recoverable: Apple retries billing up to 60 days, Google has recovery. Keeping the identifiers lets a later BILLING_RECOVERY / DID_RENEW restore `Status=paid` without a fresh purchase. `StatusPaidToUnsubscribed` is terminal — `PaidStatus=expired` is what code reads as 'fully dead', and it clears those identifiers because nothing remains to recover from.
- Q: How exactly is grace_period vs billing_retry decided? (code-verified, `service/subscription.go:2156`) — A: Inside `DID_FAIL_TO_RENEW` it's a two-way test on the notification subtype: subtype `GRACE_PERIOD` → `UpdateGracePeriod` (Status=paid, PaidStatus=grace_period, access ON); empty subtype → `BillingRetryPaidSubRel` (Status=unsubscribed, PaidStatus=billing_retry, access OFF). It does NOT read `isInBillingRetryPeriod` / `expirationIntent` to pick the branch. Three precise points: (1) `GRACE_PERIOD_EXPIRED` is NOT a subtype of DID_FAIL_TO_RENEW — it's a separate top-level notificationType (`service/subscription.go:2185`) that also routes to billing_retry (grace ended but Apple's ~60-day retry continues; EXPIRED is the real terminal event). (2) The grace branch has an edge condition: `UpdateGracePeriod` only sets grace_period if `GracePeriodExpiresDate` is still in the future (`GraceUtilAt.After(now)`); if already past, it writes GraceUtilAt but leaves Status/PaidStatus unchanged (no downgrade). (3) The DID_FAIL_TO_RENEW switch has NO else, so any other non-empty subtype is a silent no-op. Google side uses no subtype — it switches on RTDN integer codes: code 6 (in grace) → grace_period, code 5 (on-hold) → billing_retry.
- Q: Why the +2-day extra grace in IsPaidStatusExpired? — A: It's a safety net for lost/late store callbacks. baseExpireAt = max(GraceUtilAt, ExpireAt) + 2 days. Third-party callbacks are preferred; this only fires on the read path so a stale paid record self-heals to unsubscribed if the webhook was dropped — while the +2 days avoids racing a slightly-late legitimate notification.
- Q: What does one record actually store? (code + live-DB verified) — A: The `SubscriptionRelation` struct (`entity/paid_subscription.go`) backs both `nb_premium_subscription` and `subscription_relation`. Fields (bson): `_id`, `user_id`, `media_id`(omitempty), `in_free_trial`, `status`, `paid_status`, `channel`, `subscription_at`, `expire_at`, `grace_util_at`, `external_transaction_id`, `sku`, `metadata`(omitempty), `google_purchase_token`, `linked_purchase_token`, `created_at`, `updated_at`, `updated_source`, `newsletter_synced`(*bool omitempty), `env`(omitempty), `last_notif_event_time`(omitempty), `action_src`(omitempty). Live check: `nb_premium_subscription` holds ~38.7k docs; a real doc has only ~14 keys because omitempty zero-value fields don't persist. Live distinct `status` = {paid, unsubscribed} (plus free on the media line); live distinct `paid_status` = {active, cancel, grace_period, billing_retry, expired}. So billing_retry is real in prod, and `expired` is the terminal 'fully dead' marker — spelled out rather than left empty, since omitempty would make a finished lifecycle indistinguishable from one that never started. A real record showing the two-field design: `status=paid` + `paid_status=cancel` = user turned off auto-renew but keeps access until `expire_at`.

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
- Q: Who INITIATES the audit — does mp-api just receive results, or push? (code-verified) — A: mp-api actively initiates it. After the doc is created and doc_id persisted, `CallbackService.cppSuccess` does `if (auditStatus == UNREVIEWED) postService.SendToAuditQueue(postId, docId, mediaId)`, which PUSHES the audit request to a Kafka `auditRequestTopic` (variants: default / community / high). The audit teams (doughnut = high/machine check, bagel = creator-update-triggered) are the SOURCES of the verdict, which comes back on a SEPARATE topic `mp_audit_result` consumed by `processAuditResult`. So it's a request/response over two distinct Kafka topics — mp-api pushes the request, consumes the result — not just passive receipt.

---

**Deep-dive — CPP is a general content platform, and where transcode/doc_id come from**

- Q: Is CPP video-specific? — A: No — CPP (cpp-launcher) is the GENERAL doc-ingest platform for ALL content: ~21 Kafka-in/Kafka-out pipelines, every one emitting schema subject=document. News, short_post, com_post, comments, AIGC, i18n, and video all flow through it; video is just one ctype (native_video/has_video). mp-api produces the platform-side post for every type and sends them to CPP over the `mp_news` topic (the news-mp chain). `/post/publish_ugc_video` only differs by setting ctype=native_video after transcode merge, then joins the same insertPost→sendDocument→CPP→callback→audit flow as everything else.
- Q: How is doc_id generated and how does it reach mp-api? — A: doc_id is the doc's `_id`, minted on the CPP/doc-store side (not mp-api). It comes back as the `_id` field in the Kafka `mp_callback` payload (`CallbackService` reads `data.get("_id")`), and mp-api persists it via `updateDocId` → `UPDATE post SET doc_id=..., state=IfNull(state), audit_status=... WHERE id=postId` (the IfNull guards a user DELETE from being resurrected).
- Q: Cloud transcode or self-hosted? — A: Cloud-first. Transcode is a separate service — the video-platform (Python) — where AWS MediaConvert is the primary transcoder (every S3 upload event schedules a `create_job`, output to `s3://.../transcodes/{vuid}/default.mp4`); self-hosted ffmpeg is only a fallback when MediaConvert fails, plus thumbnails (mjpeg) and letterbox/cropdetect probing. AWS Transcribe does speech-to-text captions. Results flow back via HTTP `update_state` to video-feed-server + Kafka, landing in the doc's `VideoMetaData.TransCodes` (a format→CDN-URL map the server picks the best URL from at feed time). Note: presign/init_upload lives in a DIFFERENT service (video-feed-server), not the transcode video-platform.

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
