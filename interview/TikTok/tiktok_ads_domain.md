# TikTok — Ads Domain Primer (Solutions Engineer)

Companion to `tiktok_bq_en_se.md` Q7. You have **no ads background** — the goal here is not to
sound like a specialist. It is to (a) not be lost when the interviewer uses a term, (b) ask
intelligent questions, and (c) reason correctly about the *class* of problem, which you genuinely
do know: unreliable external events, numbers that must be defensible, customers who need an
explanation.

> **Accuracy note.** The concepts below are industry-standard and stable. TikTok's specific
> product names, UI labels, and API versions change frequently — verify anything TikTok-specific
> against the TikTok Business Help Center / Marketing API docs before the interview. Do not quote
> a version number or a limit you haven't checked.

> **Honesty red line.** Never claim to have run campaigns, worked on ad serving, or debugged a
> real advertiser integration. Say "my understanding is…" or "I'd want to check…" when reasoning
> from first principles. A wrong confident answer is far worse here than "I don't know that one."

---

## Part 1 — The 60-second mental model

An ads platform sits between two customers with opposing interests:

- the **advertiser**, who wants conversions at the lowest cost, and
- the **user**, whose attention and experience you're spending.

Everything else is machinery for matching them and then *proving* what happened. As a Solutions
Engineer you sit on the advertiser side of that machinery: they integrate against your APIs, they
report that something is wrong, and you find out whether it's their setup, your platform, or a
genuine misunderstanding of how attribution works. **Most escalations are the third one.**

The single most useful thing to internalize: **almost every advertiser complaint is "the number
is wrong."** Spend is wrong, conversions are wrong, the platform's number doesn't match their
internal number. Your job is to know the dozen reasons those two numbers legitimately differ.

---

## Part 2 — Campaign structure

Three levels, universal across TikTok / Meta / Google (names vary slightly):

| Level | What it sets | Advertiser question it answers |
|---|---|---|
| **Campaign** | Objective, overall budget | "What am I trying to achieve?" |
| **Ad Group** (Meta: Ad Set) | Targeting, budget, bid, schedule, placement, **optimization goal** | "Who do I show it to, and how much do I pay?" |
| **Ad** / Creative | The actual video, copy, CTA, landing page | "What do they see?" |

**Objective** is chosen at campaign level and constrains everything below it. Roughly tiered by
funnel stage: Reach / Video Views (awareness) → Traffic / Lead Generation (consideration) →
Conversions / App Installs / Catalog Sales (conversion).

Two facts that generate real support tickets:

- **You usually cannot change the objective after creation.** The advertiser must rebuild. This
  is a common, frustrating escalation.
- **The optimization goal must match what you can measure.** Optimizing for "purchase" when
  purchases aren't being reported back to the platform means the algorithm has no signal, so
  delivery stalls or spends badly. This is the single most common root cause behind "my campaign
  isn't spending" and "my costs are terrible."

---

## Part 3 — Auction and pricing

Ads are sold by continuous auction, not fixed price. When a user opens the app and an ad slot is
available, eligible ads compete in real time.

**Winner is not the highest bid.** It's roughly:

```
ranking score ≈ bid × predicted action rate × (quality / relevance)
```

An ad predicted to perform well can beat a higher bid. This is why "I raised my bid and still
don't get impressions" is a legitimate question with a non-obvious answer.

**Pricing terms** — know these cold, they come up constantly:

| Term | Means | Notes |
|---|---|---|
| **CPM** | Cost per 1,000 impressions | The billing basis for most awareness buying |
| **CPC** | Cost per click | |
| **CPA / CPI** | Cost per action / install | Usually an *optimization target*, not a billing model |
| **oCPM** | Optimized CPM | **Billed per impression, optimized toward conversions.** The default for most conversion campaigns |
| **CTR** | Clicks ÷ impressions | Creative quality signal |
| **CVR** | Conversions ÷ clicks | Landing page / funnel quality signal |
| **ROAS** | Revenue ÷ ad spend | The metric e-commerce advertisers actually care about |
| **Frequency** | Avg impressions per unique user | High frequency → creative fatigue, CTR decay |

**The oCPM trap worth understanding.** Advertisers set a target CPA and then get angry when
actual CPA exceeds it. But oCPM bills on impressions — the target is an optimization objective the
system aims at, not a price ceiling it guarantees. If conversion signal is sparse or delayed, the
model mispredicts and cost overshoots. Being able to explain that calmly is exactly the job.

**Learning phase.** New ad groups need a volume of conversion events (industry norm ~50 in a
week; verify TikTok's current number) before optimization stabilizes. During it, performance is
volatile and cost is high. Advertisers who edit the ad group mid-learning reset it — so the
advice "stop touching it" is real advice, not a brush-off.

---

## Part 4 — Attribution — the highest-value section

**This is where a Solutions Engineer earns their salary.** Almost every "your numbers are wrong"
ticket is an attribution question in disguise.

### The attribution window

Two dimensions, and the pair is what defines a window:

- **Click-through (CTA):** user clicked, then converted within N days. Typically 1 / 7 / 14 days.
- **View-through (VTA):** user *saw* but didn't click, then converted within N days. Typically
  0 (off) / 1 day.

TikTok's default is commonly 7-day click + 1-day view; **verify current defaults**.

### Why the platform's number and the advertiser's number never match

Memorize this list. It is the answer to the most common escalation you will face.

1. **Different attribution models.** The ad platform uses last-touch within its own window. The
   advertiser's analytics tool (GA4, an MMP like AppsFlyer/Adjust) may use last-non-direct-click,
   data-driven, or a different lookback. Neither is wrong; they answer different questions.
2. **View-through.** The platform counts VTA conversions; the advertiser's web analytics almost
   never does — it has no idea an impression happened. This alone can explain a large gap.
3. **Cross-device.** User sees the ad in-app on a phone, buys later on a laptop. The platform can
   often stitch that via logged-in identity; the advertiser's cookie-based analytics cannot.
4. **Attribution timing.** Platforms typically report a conversion back to the **impression/click
   date**, while the advertiser's system records it on the **conversion date**. Same events,
   different dates — so day-by-day comparison mismatches even when totals agree.
5. **Deduplication.** If a conversion fires from both the browser pixel and the server-side API
   without a shared `event_id`, it gets double counted.
6. **Signal loss.** iOS ATT (App Tracking Transparency) and cookie deprecation mean a material
   share of conversions can't be deterministically attributed and may be modeled or missing.
7. **Multi-platform overlap.** If the advertiser also runs Meta and Google, each platform claims
   the same conversion under its own last-touch rule. Summing across platforms exceeds true total.

**How to say this to a non-technical advertiser** (this phrasing is the deliverable):

> "These two systems aren't measuring the same thing, so they shouldn't be expected to agree.
> Ours reports on the day the ad was seen; yours reports on the day the sale happened. And ours
> includes people who saw the ad and bought later without clicking, which your analytics can't
> see at all. What I'd suggest is that we don't chase an exact match — we pick one as the source
> of truth for decisions, and use the other for trend direction."

---

## Part 5 — Conversion tracking and the integration surface

This is the **API and backend systems design** half of the JD — and it's the part closest to work
you've actually done.

Three ways a conversion gets back to the platform:

| Method | How | Weakness |
|---|---|---|
| **Pixel** (web) | JS snippet fires an event from the browser | Blocked by ad blockers, ITP, cookie restrictions |
| **Events API** (server-side) | Advertiser's backend POSTs the event directly | Requires engineering work on their side |
| **MMP** (mobile app) | AppsFlyer / Adjust / Branch sits in the middle and attributes installs | Third party in the loop; another set of numbers to reconcile |

**Server-side is the direction the whole industry has moved**, because browser-side signal keeps
degrading. TikTok's is the Events API (verify the current name and version).

### Standard events

`ViewContent` → `AddToCart` → `InitiateCheckout` → `AddPaymentInfo` → `CompletePayment`, plus
`Lead`, `CompleteRegistration`, `Search`. Custom events are supported but don't optimize as well,
because the model has less cross-advertiser prior on them.

### Deduplication — the concept you already own

When an advertiser runs pixel *and* Events API together (recommended, for coverage), the same
purchase arrives twice. The fix is a shared **`event_id`** sent on both, so the platform drops the
duplicate. `event_source_id` / pixel ID identifies which pixel; `event_id` dedups the event.

> **This is literally the idempotency-key pattern from your premium subscription work.** Same
> problem: an external system delivers the same event more than once, and you need exactly-once
> effect. You can say this in the interview — it's true and it's the strongest bridge you have.

### Matching

Server-side events carry hashed identifiers (email, phone, IP, user agent, click ID) so the
platform can match the conversion to a user who saw the ad. **Match rate** is the share that
successfully match. Low match rate → attributed conversions drop → advertiser says "my
conversions fell off a cliff," and the actual cause is that they stopped sending an identifier
after a site migration. That is a *textbook* SE ticket.

### Click ID

TikTok's is `ttclid` (Meta: `fbclid`, Google: `gclid`). Appended to the landing page URL on
click; the advertiser should capture it, persist it, and send it back with the conversion event.
It gives deterministic attribution without relying on cookies. **Advertisers forgetting to
persist `ttclid` through their checkout flow is a very common integration bug.**

---

## Part 6 — The escalations you'd actually handle

Know the top few causes for each. This is the most interview-relevant table in the file.

### "My campaign isn't spending"

1. Bid or budget too low to win auctions
2. Targeting too narrow — audience is too small
3. Ad still in review, or creative rejected
4. Optimization goal has no signal (conversion events not arriving)
5. Payment method failed / account flagged
6. Schedule or dayparting misconfigured
7. Still in learning phase

### "My conversions dropped suddenly"

1. Pixel removed or broken by a site deploy — **check whether events are arriving at all first**
2. Site migration dropped `ttclid` capture, or stopped sending hashed identifiers → match rate collapsed
3. Consent/CMP change now blocks the pixel before it fires
4. Attribution window changed
5. Real performance drop (creative fatigue, seasonality, competition)
6. Deduplication newly working correctly — the *previous* number was inflated

### "Your numbers don't match mine"

→ Part 4. Work down that list in order; start by establishing which direction the gap runs and
whether it's a totals gap or a per-day gap.

### "My costs went up"

1. Auction competition (seasonal — Q4, holidays)
2. Creative fatigue: frequency up, CTR down
3. Audience saturation
4. Learning phase reset from an edit
5. Bid strategy or optimization goal changed

**Method — the general shape, which matters more than any specific cause:**

1. Reproduce and scope. Get campaign/ad group ID, timestamps, a request ID if it's an API issue.
2. Establish whether data is *arriving*. Missing-at-source and wrong-after-arrival are completely
   different investigations, and this one check splits them.
3. Find what changed, on either side. Their deploy is as likely as ours.
4. Distinguish real performance change from measurement change. Advertisers conflate these
   constantly and it drives the wrong response.
5. Fix the customer, then fix the class — vague error, ambiguous doc, or an API that permitted an
   invalid state.

---

## Part 7 — Vocabulary quick reference

| Term | Meaning |
|---|---|
| **Pixel** | Browser-side tracking snippet |
| **Events API** | Server-side conversion reporting |
| **MMP** | Mobile Measurement Partner (AppsFlyer, Adjust, Branch) |
| **SKAdNetwork / SKAN** | Apple's privacy-preserving iOS attribution — aggregated, delayed, limited |
| **ATT** | App Tracking Transparency — iOS 14.5+ opt-in prompt; major signal-loss cause |
| **`ttclid`** | TikTok click identifier for deterministic attribution |
| **`event_id`** | Deduplication key across pixel and server events |
| **Match rate** | Share of conversion events matched to a platform user |
| **Lookalike audience** | Audience modeled on a seed customer list |
| **Custom audience** | Advertiser-uploaded list (hashed emails/phones) |
| **Retargeting** | Ads to users who already engaged |
| **Frequency capping** | Limit on impressions per user |
| **Brand safety** | Controls over adjacent content |
| **Placement** | Where the ad appears (feed, Pulse, Search, etc.) |
| **DPA / catalog ads** | Dynamically generated product ads from a feed |
| **Spark Ads** | TikTok format that boosts an existing organic post |
| **Learning phase** | Early period before optimization stabilizes |
| **Incrementality** | Whether the ad *caused* the conversion — the deepest measurement question |

**Incrementality is worth one extra sentence** because it's the intellectually honest critique of
all attribution: last-touch credits a conversion that might have happened anyway. Proper answer is
a holdout / geo experiment, not a better attribution model. Mentioning this signals you actually
understand measurement rather than having memorized acronyms.

---

## Part 8 — Questions to ask (these signal domain seriousness)

- "When an advertiser says their numbers don't match ours, how far does the team usually go — is it explaining the methodology difference, or actually auditing their integration?"
- "How much of the escalation volume is genuine platform bugs versus integration or expectation issues?"
- "Is the team's output mostly per-customer resolution, or building tooling and docs so the same issue doesn't recur?"
- "How much does the team work with self-serve advertisers versus large managed accounts? I'd imagine the problems are pretty different."

---

## Part 9 — Study order if time is short

**If you have one hour** — Part 2 (structure), Part 4 (why numbers differ), Part 6 (top escalations).
Those three cover the majority of what a first-round SE conversation touches.

**Two more hours** — Part 5 (integration surface: pixel vs Events API, `event_id`, `ttclid`,
match rate), then the Part 7 vocabulary.

**Then** — Part 3 (auction/oCPM/learning phase) and incrementality, for depth if the interviewer
pushes.

**Two bridges to your real experience, worth rehearsing** (both are honest):

1. **`event_id` deduplication ≡ your `notification_uuid` idempotency key.** External platform
   delivers duplicate events; you need exactly-once effect. You've solved this in production, on
   a money path.
2. **Advertiser API integrations breaking on contract changes ≡ your Protobuf migration.**
   Consumers you don't control, fields you can't drop, and a need to prove you didn't break anyone
   before they tell you. That's the same discipline.
