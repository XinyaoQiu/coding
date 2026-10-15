# Chapter 90 — Rate Limiter

> **Prerequisites:** Chapters 02 (local caches, TTL), 03 (backpressure, retries, circuit breakers), 04 §3 (the five algorithms), 04 §6 (consistent hashing)
> **Patterns:** shared mutable state on the hot path, atomic read-modify-write, approximate distributed counting, explicit failure policy

---

## 1. The problem

A service must bound how much any one client consumes. Every request passes through a middleware that
answers one question — *may this proceed?* — in constant time, and either forwards it or returns
`429 Too Many Requests`.

Chapter 04 §3 gives the five counting algorithms and the failure of each. This chapter designs the
**service** around them: where the middleware sits, where the counters live, what makes an increment
correct when a hundred gateways increment it at once, how it survives its own storage going down, and
what the client is told.

**The property that makes it hard:** a rate limiter is shared mutable state on the hot path of one
hundred percent of traffic, added in order to control a few percent of it, so every property it has is
paid for by every request. If the check costs 3 ms, every request costs 3 ms more. If the counter store is
down and the limiter fails closed, the service is down — taken down by the component whose purpose was to
keep it up. The design is a continuous negotiation between *accuracy*, which wants one authoritative
counter, and *cost*, which wants no network call at all.

Everything else is arithmetic. **The asymmetry between what it protects against and what it costs
everyone is the problem.**

---

## 2. Requirements

### Functional

1. Enforce rules of the form "at most N units per window W for key K", K derived from the request: client
   IP, authenticated user ID, API key, or tenant ID.
2. Evaluate several rules per request — per-IP, per-user, per-endpoint — and let the strictest bind.
3. Weight requests by cost, so an expensive endpoint consumes more allowance than a cheap one (§7.4).
4. Return `429` with the headers a well-behaved client needs (§4).
5. Rules are runtime configuration. A limit you cannot change during an incident is not a control.

### Non-functional

- **Added latency** — p99 under 2 ms, ideally near zero on the common path. This is the number that
  forces the local-bucket design in §7.3. It is a budget: the limiter gets a fixed slice of the service's
  latency SLO and no more.
- **Availability strictly greater than the service it protects.** A limiter down 0.1% of the time that
  fails closed has made the service worse than not having it. This is what §7.2 is about.
- **Accuracy — approximate, deliberately.** The requirement is not "never admit the N+1th request" but
  "the sustained admitted rate stays within a small bounded factor of the limit." Insisting on exactness
  produces a slow, fragile design. State the bound and buy latency with it.
- **Throughput** — 1,000,000 requests/second at peak, three rules each.
- **Fairness** — one tenant exhausting shared capacity must not degrade another's (§7.5).

### Explicitly out of scope

Volumetric DDoS absorption, which belongs at the network edge — a limiter in your gateway has already paid
for the packet by the time it decides to drop it. Also authentication itself, usage-based billing (which
needs *exact* counts in a durable store, not approximate ones in a cache), body inspection, and WAF rules.

---

## 3. Estimation

Assume one million requests per second at peak across the gateway fleet.

```
1,000,000 req/s × 3 rules per request = 3,000,000 counter read-modify-writes/sec
```

If each rule is its own round trip that is three million network calls per second and three times the
latency. Both are fixed by one decision: **make the client identity the shard key**, so all of a client's
rules live on one shard and are evaluated by one script in one round trip — three million calls collapse
into one million.

A Redis node is single-threaded. A small Lua script — read a hash, do arithmetic, write back, set a TTL —
costs about 10 µs of CPU, and the surrounding protocol handling costs more than the script does. A
realistic planning figure is 100,000 scripted operations per second per node; call it 80,000 for headroom.

```
1,000,000 ops/s ÷ 80,000 = 13 nodes → 16 shards, each with a replica = 32 instances
```

```
5,000,000 clients active per window × 3 rules = 15,000,000 keys
× ~100 B (key, expiry, dict entry; the value is tiny) ≈ 1.5 GB ≈ 95 MB/shard
```

Memory is not the constraint and never becomes one. Say so and move on; the urge to design a storage tier
here is the first trap.

**The constraining number is the round trip, not the counter.**

An in-datacenter Redis call runs about 0.3 ms at p50 and 2–5 ms at p99 once connection-pool waits and the
occasional slow script are included. Three milliseconds added to a service with a 150 ms p99 is a 2% tax
nobody notices; added to a gateway fronting a cache-served endpoint with a 12 ms p99 it is a 25% tax, and
someone will. Worse, the limiter's tail is **correlated** — when Redis is slow it is slow for every
gateway at once, so the limiter's p99 becomes the service's p50.

That drives the two decisions that matter: co-locate a client's rules on one shard so there is at most one
round trip, then remove even that for most requests with a local bucket (§7.3).

---

## 4. API

Three interfaces, commonly conflated. The enforcement interface is internal — one call,
`check(clientKey, rules[], cost)`, returning whether the request is allowed plus the binding rule's limit,
remaining allowance, and reset time. The wire contract is what the world sees, and it is part of the
design:

```
200 OK                          429 Too Many Requests
X-RateLimit-Limit:     1000     Retry-After:           41
X-RateLimit-Remaining: 973      X-RateLimit-Limit:     1000
X-RateLimit-Reset:     41       X-RateLimit-Remaining: 0
```

The control plane is a small CRUD service whose versioned output is pushed to every gateway and held in
process memory, so rule distribution is never on the request path:

```
PUT  /rules/{id}  {scope: user|ip|apiKey|tenant, match:{path,method}, limit, windowSec, cost}
POST /rules/{id}/override  {key, limit, expiresAt}   # incident lever: pin one client
```

**429, not 503.** A 503 says the server is broken; a 429 says this caller is going too fast. Client
libraries and load balancers treat them differently, and a mislabeled 503 makes an upstream balancer mark
a healthy instance unhealthy and remove it — turning a throttle into an outage.

**Report the binding rule.** When three rules apply, the headers must describe the one that will deny the
next request — the one with the least remaining allowance. A generous per-IP remainder reported while the
per-user rule is about to deny is worse than reporting nothing, because the client believes it.

**`Retry-After` is the point of the whole contract.** Without it a throttled client retries immediately,
and a throttled *fleet* retries in lockstep — the retry storm of Chapter 03 §5 aimed at the endpoint that
was already saturated. With it, plus client-side jitter, the retry arrives when there is capacity.
Publishing the limit converts an adversarial relationship into a cooperative one for the large majority of
clients who are not adversaries, and the load avoided from them dwarfs the load from the minority who
ignore it. The objection that publishing limits tells an attacker the threshold is true and unimportant:
an attacker binary-searches it in seconds, and the only party the secrecy inconveniences is the integrator
trying to build a well-behaved client.

---

## 5. Data model

```
key:   rl:{<clientId>}:<ruleId>
value: hash {w: <window start, epoch sec>, c: <count this window>, p: <count previous window>}
ttl:   2 × windowSec
```

Three deliberate decisions.

**The hash tag.** In Redis Cluster the braces select the part of the key that determines the slot, so
`rl:{user:8812}:api-read` and `rl:{user:8812}:api-write` land on the same node by construction. That is
what makes "one round trip evaluates all of a client's rules" true rather than aspirational, and why the
key is shaped this way instead of the more natural `rl:api-read:user:8812`.

**The TTL is the entire cleanup story.** No sweeper, no expiry job, no garbage collection. An idle
client's keys evaporate two windows after its last request, so the working set is proportional to *active*
clients rather than *known* ones — 5 million keys rather than 500 million. Setting the TTL inside the same
atomic script as the increment matters: the classic bug is `INCR` followed by a separate `EXPIRE`, where a
crash between them leaves a key that never expires and a client that is permanently limited.

**The sliding-window-counter shape** (Chapter 04 §3): O(1) memory, no boundary spike, accurate to within
the uniformity assumption on the previous window. A sliding window *log* would be exact at one entry per
request — 50 billion timestamps at 10,000/hour across 5 million clients, which is why it is not on the
table.

---

## 6. Architecture, derived

### Attempt 1: an in-process counter per gateway

Each gateway keeps a map from client to counter and enforces locally. Zero latency, no dependencies,
trivially available — and **it breaks the moment there is more than one gateway.** With G gateways and a
balancer spreading a client's requests, each sees 1/G of that traffic, independently permits N, and the
effective limit is `G × N`. At 200 gateways a limit of 1,000 per minute admits 200,000. The limiter is not
approximately right; it is wrong by two orders of magnitude, in the direction that matters.

### Attempt 2: a shared counter with GET and INCR

```
c = GET rl:{user}:rule       # 999
if c >= limit: deny
INCR rl:{user}:rule          # 1000
```

**This leaks, by a quantifiable amount.** Read and write are separate round trips, so between them the
counter is stale, and every request from that client arriving inside the gap sees the same pre-increment
value: `10,000 req/s × 0.0003 s ≈ 3` in flight during one 0.3 ms gap. Three readers see 999 and all admit.
The overshoot per boundary is bounded by the number of concurrent in-flight checks — exactly the quantity
that grows when a client is abusive. **The limiter leaks most under precisely the conditions it exists
for.**

Both obvious repairs are bad. `WATCH`/`MULTI` becomes an optimistic transaction that retries on
contention, and contention is guaranteed on a hot key, so the retry loop degenerates exactly when loaded.
Deciding on the return value of `INCR` fixes the race but consumes allowance for *denied* requests, so a
client hammering an exceeded limit never recovers — its own rejections keep the counter pinned above the
threshold.

### Attempt 3: one atomic script

Do the whole read-modify-write inside a Lua script, which Redis runs to completion without interleaving:

```lua
-- KEYS[1] = rl:{client}:rule    ARGV = limit, windowSec, cost
local now, win = tonumber(redis.call('TIME')[1]), tonumber(ARGV[2])
local start    = now - (now % win)
local h        = redis.call('HMGET', KEYS[1], 'w', 'c', 'p')
local w, c, p  = tonumber(h[1]) or start, tonumber(h[2]) or 0, tonumber(h[3]) or 0
if     w < start - win then c, p = 0, 0        -- both windows stale
elseif w < start       then p, c = c, 0 end    -- roll forward one
local est  = c + p * (1 - (now - start) / win) -- sliding window counter
local cost = tonumber(ARGV[3])
local ok   = (est + cost) <= tonumber(ARGV[1])
if ok then c = c + cost end
redis.call('HSET', KEYS[1], 'w', start, 'c', c, 'p', p)
redis.call('EXPIRE', KEYS[1], win * 2)
return {ok and 1 or 0, math.floor(ARGV[1] - est), win - (now - start)}
```

**The atomicity is the correctness argument.** Not a performance trick and not a round-trip optimization —
it is the reason the limit holds at all. Without it concurrent gateways interleave and the limit leaks by
the amount derived above. That is the sentence to say out loud, because it is the difference between
having chosen Lua and having heard that Redis has Lua.

Two details are deliberate. The clock comes from `redis.call('TIME')`, not from the gateway: gateway
clocks disagree by tens of milliseconds even under NTP, and a client whose requests land on skewed
gateways would straddle window boundaries inconsistently. Taking the clock from the one process that owns
the counter removes the problem. And the script is registered once and invoked by SHA (`EVALSHA`), so the
body is not on the wire a million times a second.

### Attempt 4: shard by client, route without a lookup

One node does not do a million operations per second; sixteen do — but only if a gateway can determine
which shard owns a client without asking anyone.

**Consistent hashing on `clientId`** (Chapter 04 §6) gives exactly that: hash the client onto the ring,
walk clockwise, connect. No central lookup on the request path, no directory service to become a single
point of failure. Because virtual nodes spread each physical node over many ring positions, adding a
seventeenth shard moves roughly 1/17 of clients instead of rehashing all of them, and the moved clients
lose their counters and restart from zero — an acceptable failure here, since a few clients get one extra
window's allowance. **A rate limiter is one of the very few systems where losing state is nearly free**,
and the design should spend that freedom aggressively.

One round trip is still one too many at a 3 ms p99, so the gateway also keeps a local token bucket per
active client, drawing tokens from the shard in batches and touching the network only when a batch runs
low (§7.3).

### Final architecture

```
rule store ──push──► gateway rule cache (in process, never on the request path)

client ─► L7 LB ─► ┌─────────────── API gateway ───────────────┐ ─► upstream service
                   │ [1] pre-auth limiter,  key = IP / ASN      │
                   │ [2] authentication                         │
                   │ [3] post-auth limiter, key = user / tenant │
                   │      each: local bucket ──miss──┐          │
                   └─────────────────────────────────┼──────────┘
                              consistent hash on clientId
                                                     ▼
                       ┌─────────┬─────────┬─────────┐
                       │ shard 0 │ shard 1 │ ... 15  │ Redis, EVALSHA, +1 replica each
                       └─────────┴─────────┴─────────┘
```

---

## 7. Deep dives

### 7.1 Where the limiter lives, and why the order matters

The limiter is gateway middleware, and it runs **twice**: once before authentication and once after.
Before authentication the only identity available is the network one — source IP, or its autonomous
system. That is the only thing an *unauthenticated* flood can be limited by, and it must come first
because the alternative is paying for authentication before deciding to reject. Authentication is usually
the most expensive middleware in the chain: a signature check, a revocation lookup, a call to a session
store. If a flood of garbage tokens reaches it, the limiter has protected the upstream service and
sacrificed the auth path, which fails first and takes every legitimate login with it.

After authentication the useful identity exists — user, API key, tenant — and the limits that matter
commercially can be applied. These *cannot* run earlier: it is a data dependency, not a preference.

So the ordering is forced: **IP limits go before auth because auth is expensive and IP is the only
identity that exists; user limits go after auth because the user identity does not exist before it.** A
single limiter on one side of auth is wrong in one of two ways, and which way tells you which mistake was
made. Two consequences: the pre-auth limit must be generous, because a corporate NAT puts tens of
thousands of legitimate users behind one address and a tight per-IP limit is an outage for an entire
office; and it should be the cheapest implementation available, ideally purely local, because it guards
exactly the traffic you do not want to spend a round trip on.

### 7.2 Fail-open versus fail-closed

Redis is unreachable and the gateway must answer within two milliseconds. **Fail open** — admit: the
service works, the protection is gone for the duration. **Fail closed** — reject: the protection holds,
the service is down.

For a public API, **fail open is the right default, and it should be said deliberately rather than
arrived at by accident.** The limiter exists to stop abuse; abuse is a minority of traffic during a
minority of the time. Failing closed converts a dependency's outage into a total outage of the thing it
protects, making the limiter a strictly larger source of downtime than the abuse it prevents. A component
added for availability must not reduce availability.

**But fail closed is correct for some rules, and knowing which is the actual skill.** Fail closed when the
limit is a security control rather than a capacity control: login attempts, where failing open enables
credential stuffing at full speed; password resets and SMS sends, where each admitted request costs real
money to a third party. The test is *if this goes unenforced for ten minutes, is the damage recoverable?*
Extra load is recoverable; a drained SMS budget and a breached account are not.

**And the binary is false.** When the central store is unreachable the gateway need not choose between "no
limit" and "no service": it falls back to its local bucket (§7.3) with a conservative allowance, typically
`limit / expected_gateways`. Enforcement becomes approximate and generous instead of absent, with overshoot
bounded by `gateways × local allowance` — a number you can compute in advance. Naming this fallback is
meaningfully better than picking a side of the binary.

**The failure nobody plans for is not the policy, it is the timeout.** With a one-second Redis timeout,
every request during an outage waits a full second before failing open; at a million requests per second
the connection pool is exhausted instantly and the fleet falls over long before any fail-open logic runs.
The timeout must be a small multiple of the expected latency — 5 ms against a 0.3 ms p50 — and a circuit
breaker (Chapter 03 §5) must trip after a handful of failures so later requests skip the call entirely.
**The breaker, not the failure policy, is what keeps the gateway alive during a Redis outage.**

### 7.3 The local token bucket, and the overshoot you are buying

Removing the network hop from the common path is the standard latency optimization. It has three
distinguishable designs; they are not equivalent, and picking the wrong one is a common error.

**(a) Static split.** Each gateway enforces `N/G` with no coordination. Zero latency, zero dependency,
and wrong whenever traffic is uneven — which is always, because balancers distribute by connection and a
keep-alive client sends everything to one gateway. That client gets `N/G`: with 200 gateways, a customer
paying for 1,000 per minute receives 5. Rejected — unless the L7 balancer routes by a consistent hash of
the client, in which case the split is exact and the design is excellent, at the cost of pinning your
load-balancing strategy to your limiter's needs.

**(b) Lease and refill — the recommendation.** The central store stays authoritative. A gateway asks for a
batch ("20 of this client's allowance"), the script decrements the central counter by 20 and returns them,
and the gateway spends locally. The total ever granted **cannot exceed the limit**, because the counter is
decremented at lease time rather than spend time. Round trips drop by the batch factor:

```
client at 2,000 req/s, batch 20 → 100 central calls/s instead of 2,000
3 ms on 5% of requests ≈ 0.15 ms amortized
```

The cost is not overshoot but **stranding**: a gateway holding 15 unspent tokens for a client that stops
sending has removed them from that client's allowance for the rest of the window. Across G gateways up to
`G × batch` tokens strand, so the batch must stay small relative to the limit — roughly
`batch ≤ limit / (10 × G)`, which for small limits degenerates to 1. That is correct: **a limit of 5 per
minute should not be enforced locally at all.** Gateway death strands its batch until the window rolls,
which is self-healing.

**(c) Optimistic local counting with periodic reconciliation.** Each gateway counts locally and every S
milliseconds reports its count and receives the global total. Cheapest and loosest; overshoot is what the
fleet admits during one interval before it learns:

```
200 gateways × local allowance 10, sync every 200 ms = 2,000 admitted against a limit of 1,000
→ 3× overshoot for one sync interval
```

Acceptable for a coarse abuse limit, unacceptable for a paid quota. Use (c) for the pre-auth IP limit, (b)
for per-user and per-tenant limits, (a) only when the balancer cooperates.

The general principle transfers: **you are trading a bounded, computable inaccuracy for the removal of a
network hop.** State the bound. A design that says "we cache locally for performance" without saying how
wrong it can be has not made the trade, only the mistake.

### 7.4 Cost weighting per endpoint

Counting requests assumes requests are interchangeable. They are not: a key lookup costs a millisecond of
one core, a search fifty and an index cluster, a report export seconds and a held database connection. A
client capped at 1,000 requests per minute can consume fifty times more capacity by shifting its mix,
entirely within the rules.

**Denominate the limit in capacity units rather than requests.** Each endpoint declares a cost derived
from measured resource consumption, normalized so the cheapest is 1, and the limiter consumes `cost` units
per request. The script in §6 already takes `cost`; this is configuration, not mechanism.

Two refinements. **Cost can be charged after the fact** from the measured cost rather than a static
estimate, which matters where cost varies with response size; the under-estimated request has already run,
which is fine, because the limiter bounds sustained rate rather than any individual request. And **a very
expensive endpoint deserves a concurrency limit rather than a rate limit** (§10) — "holds a database
connection" is a claim about simultaneity, not rate.

The cost is maintenance: an endpoint that becomes ten times more expensive after a refactor, with its
weight left at 1, silently reopens the hole. Derive weights from production metrics on a schedule.

### 7.5 Multi-tenant fairness

A global limit protects the service from everyone; it does not protect tenants from each other. If the
downstream sustains 100,000 requests per second and one tenant's batch job ramps to 90,000, everyone else
shares the remaining 10,000 and suffers an outage caused by a stranger — with every individual limit
respected.

**Hard per-tenant quotas.** A fixed slice each, excess rejected even when capacity is idle. Simple,
perfectly isolating, wasteful: with a hundred tenants whose peaks do not coincide, most of the fleet sits
idle reserving capacity for peaks that are not happening.

**Weighted fair queueing at admission.** Keep a per-tenant deficit counter and admit in a rotation
weighted by entitlement, so under contention every tenant gets its share and otherwise whoever wants the
capacity gets it. Work-conserving, but it needs a queue and therefore a queueing delay, and it functions
only if the gateway knows the downstream's real capacity — otherwise it fairly schedules an overload.

**The practical middle, and the recommendation:** per-tenant limits set generously above entitlement, plus
a **global admission controller** that activates only when the downstream shows saturation — rising
latency, queue depth, an explicit signal — and then sheds by tenant share, throttling whoever is furthest
above entitlement first. Normally the limiter is work-conserving and nobody is constrained; abnormally it
degrades toward proportional fairness.

The cost is honesty about what you promised. A tenant that routinely succeeds at three times its
entitlement will build a client assuming it, and will file an incident when the surplus is withdrawn under
contention. **Publish the guaranteed number in `X-RateLimit-Limit` and treat everything above it as
explicitly best-effort.** A system that silently allows more than it promises trains its users to depend
on the surplus.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Redis shard unreachable | Rules on that shard cannot be evaluated | 5 ms timeout plus a circuit breaker; fall back to the local bucket at `limit/G`; fail open for capacity rules, closed for security rules (§7.2) |
| Redis slow but alive | The limiter's tail becomes the service's median | Tight timeout, breaker on latency as well as errors, alert on limiter p99 separately |
| Shard added or lost | ~1/N of clients lose their counters | Acceptable by design — one extra window's allowance for a small fraction of clients |
| Gateway holding leased tokens dies | Tokens stranded until the window rolls | Small batches; loss bounded by `G × batch`, self-healing |
| Single client at extreme rate | One shard's CPU saturated by one key | Local buckets absorb it; a genuinely abusive client belongs on a block list |
| Rule misconfigured to zero | Total outage for that scope | Versioned rules, canaried on a fraction of gateways, one-command rollback, non-zero validation at write time |
| Control plane down | No rule changes possible | Gateways run indefinitely on the last cached ruleset; rule distribution is never on the request path |

**Monitoring:** rejection rate by rule and by tenant — the primary business signal, since a rate climbing
without a traffic increase means a rule is wrong rather than that clients got worse; limiter-added latency
at p50 and p99, separately from the service's; script error rate and breaker state; the fraction of
decisions served locally versus centrally, which is how you know the optimization works; and the count of
clients within 10% of their limit, the leading indicator for a support ticket.

---

## 9. Common mistakes

1. **Reciting the five algorithms and stopping.** The algorithm is the easy part and Chapter 04 has it;
   the question is about atomicity, placement, sharding, and failure policy.
2. **A non-atomic read-modify-write.** `GET` then `INCR` leaks by the number of concurrent in-flight
   requests, largest exactly when the client is abusive. The Lua script is the correctness argument.
3. **Not stating the failure policy.** "We use Redis" without answering "what happens when Redis is down"
   leaves the most consequential decision in the design unmade.
4. **Failing closed by default**, turning the availability component into the availability problem — or
   failing open on a login endpoint, converting a capacity decision into a security incident.
5. **Ignoring the timeout.** Fail-open logic that runs after a one-second timeout saves nothing; the
   connection pool is already exhausted. The breaker is what works.
6. **Per-gateway limits with no coordination**, multiplying the effective limit by the fleet size, and
   **placing the limiter on only one side of authentication**, which either makes per-user limits
   impossible or lets an unauthenticated flood consume the auth path.
8. **Counting requests rather than cost**, letting a client take fifty times the capacity by shifting to
   expensive endpoints without breaking a rule, and **omitting `Retry-After`**, which turns every
   throttled client into a tight retry loop aimed at the endpoint that was already saturated.

---

## 10. Variants

**Concurrency limiting rather than rate limiting.** Bound a client's requests *in flight* instead of per
second. Little's Law relates them — `concurrency = rate × latency` — but they degrade differently: a
concurrency limit tightens automatically when the downstream slows, because each request holds its slot
longer. The right control for anything holding a scarce resource such as a database connection.

**Adaptive limits.** Adjust by AIMD against an observed signal — increase while latency is healthy, halve
at the first sign of saturation, the control law TCP uses. Removes the need to know the downstream's
capacity in advance, at the cost of a loop that can oscillate mid-incident. **The leaky bucket** is the
non-adaptive version of the same instinct: queue and drain at a fixed rate (Chapter 04 §3), accepting
added latency as the price of never bursting.

**Quota and billing systems** look like rate limiters and are not: they need exact counts, durability, and
monthly windows — a durable store and a reconciliation job, not a cache with a TTL. Sharing one counter
between the two is the mistake, because the limiter may lose state freely and the biller may not.

**Edge limiting at the CDN.** For volumetric and IP-based limits, enforcing at the points of presence
stops traffic before it reaches your network at all — the only way to handle an attack larger than your
ingress bandwidth. Per-user limits stay in your gateway, because the CDN does not know your identities.
Client-side limiting in an SDK is the same idea one hop further out: cheapest of all, purely advisory,
a courtesy layer and never a replacement.

---

## 11. Further reading

- Chapter 04 §3 for the five algorithms in general form and §6 for consistent hashing; Chapter 03 §5–§6
  for the retries, circuit breakers, and load shedding this chapter assumes
- [Redis — Rate limiting patterns](https://redis.io/tutorials/howtos/ratelimiting/)
- [Stripe — Scaling your API with rate limiters](https://stripe.com/blog/rate-limiters)
- [Cloudflare — How we built rate limiting capable of scaling to millions of domains](https://blog.cloudflare.com/counting-things-a-lot-of-different-things/)
- RFC 9110 §15.5.30 for `429`, and the IETF httpapi working group's draft "RateLimit header fields for
  HTTP" for the header conventions used here
- [Netflix — concurrency-limits](https://github.com/Netflix/concurrency-limits), for the adaptive
  concurrency variant and its AIMD control law
