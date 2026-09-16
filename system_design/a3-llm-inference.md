# Chapter A3 — LLM Inference Service (ChatGPT-like)

> **Prerequisites:** Chapters 02 (§8 client-push protocols), 03 (queues, backpressure), 04 (rate limiting, consistent hashing)
> **Patterns:** iteration-level scheduling, arithmetic intensity, memory-bound serving, admission control, dual-SLO latency

---

## 1. The problem

A user types a message. The service returns a reply, one token at a time, appearing on screen as it is
produced. Behind it sits a large transformer — assume 70 billion parameters — on GPUs costing more per
hour than the engineer operating them. Most candidates have no framework for this and reach for the
stateless-web-service template: model behind a load balancer, *N* replicas, autoscale on CPU. Every part
of that is wrong, and why is the chapter.

**The property that makes it hard:** a GPU serving one request at a time is idle. Producing a single token
requires reading all 140 GB of weights out of high-bandwidth memory to perform only 140 GFLOP of
arithmetic on them — about 1 FLOP per byte against hardware that wants 148. **The machine is starved by a
factor of a hundred and fifty.** The design exists to close that gap by serving many requests within one
weight-read, and what stops you is not compute and not network but the memory held by the key/value cache
of every in-flight conversation. **The KV cache, not FLOPs, is the binding constraint on concurrency.**

---

## 2. Requirements

### Functional

1. Submit a conversation (a list of messages) and receive a generated reply, **streamed** token by token,
   cancellable in flight.
2. Multi-turn conversations — the model must see prior turns.
3. Per-account rate limits, usage accounting, and content safety on both prompt and output.

Defer, but name: fine-tuning, tool calling, multi-modal input, offline batch inference, retrieval
augmentation. Ask **whether output is streamed** — if not, half this design disappears and the question is
a pure throughput problem. It is the one clarifying question worth the time.

### Non-functional

Two latency SLOs, not one. This is the requirement most candidates get wrong:

- **Time to first token (TTFT)** — p50 under 400 ms, p95 under 1.5 s. What the user perceives as
  responsiveness; dominated by queue wait plus prefill.
- **Inter-token latency (ITL)** — p95 under 50 ms, i.e. 20+ tokens/second. Human reading is ~250 words/min
  ≈ 6 tokens/s, so 20 tokens/s already outruns the reader and going beyond it buys nothing perceptible
  while costing throughput.
- **A single "p99 latency" is meaningless here.** Total latency is `TTFT + output_tokens × ITL` and
  output length varies 100×, so it is a distribution over output length, not a measure of system health.

Also: **cost** — the real objective is dollars per million tokens (§7.8), with utilization as the lever;
**availability** 99.9%, where degradation (smaller model, longer queue, a 429) is acceptable;
**correctness** — generation is nondeterministic by design, so there is no consistency model at all, which
is why almost nothing from Chapter 01 applies; and **fairness** — one account must not occupy the fleet
with 100 concurrent 32k-token generations.

### Explicitly out of scope

Training infrastructure, model quality, prompt engineering, alignment. Name them so the boundary is
deliberate.

---

## 3. Estimation

Fix a configuration; none of the interesting arithmetic works without one.
```
Model:   70B params, bf16 → 140 GB weights; 80 layers, 64 query heads,
         8 KV heads (GQA), head_dim 128
Node:    8 × H100 80 GB = 640 GB HBM, tensor-parallel over all 8;
         per GPU 3.35 TB/s HBM bandwidth, ~495 TFLOP/s dense bf16
Traffic: 100M requests/day, avg prompt 2,000 tokens, avg output 300 tokens
```

**Request rate:** `100e6 / 86,400 ≈ 1,160 req/s`, peak ~3,500. Small; nothing here is driven by QPS. The
load that matters is tokens: `1,160 × 2,000 = 2,320,000` prompt tokens/sec against
`1,160 × 300 = 348,000` output tokens/sec — a 6.7:1 ratio, because chat prompts carry history and replies
are short. **Most compute in a chat service is spent reading the prompt, not writing the answer**, which
is why §7.6 is the largest cost lever.

**Arithmetic intensity — why batching works.** A forward pass costs ~2 FLOP per parameter per token. One
decode step for a batch of *B* reads every weight once (17.5 GB per GPU at TP=8) and does `2 × 70e9 × B`
FLOP, so `intensity = (2 × P × B) / (2 bytes × P) = B FLOP/byte` against an H100 ridge point of
`495e12 / 3.35e12 ≈ 148`. **At batch 1 the GPU runs at 1/148 ≈ 0.7% of peak compute.** The weight read
costs `17.5 GB / 3.35 TB/s ≈ 5.2 ms` per step whether you serve one sequence or a hundred and forty-eight,
so batching up to 148 is essentially free: the same 5.2 ms yields 148 tokens instead of one. That is the
economic argument for everything that follows, and it should be derived out loud.

**KV cache per token — the constraining number.**

```
kv_bytes/token = 2 (K and V) × layers × kv_heads × head_dim × dtype_bytes
               = 2 × 80 × 8 × 128 × 2  =  327,680 B ≈ 0.33 MB

per node: 640 GB HBM − 140 GB weights − ~40 GB activations/workspace ≈ 460 GB for KV

 4,096-token context:  1.34 GB/seq → 460/1.34 ≈ 343 concurrent sequences
32,768-token context: 10.70 GB/seq → 460/10.7 ≈  43 concurrent sequences
```

**This is THE number, and it is not a FLOP count.** A node whose arithmetic would sustain a batch of many
hundreds is capped at 343, and at 43 when users hold long contexts. Grouped-query attention is doing
enormous work: with full multi-head attention (64 KV heads) the per-token cost is 2.6 MB and 4k
concurrency falls from 343 to 43 — an 8× cut in customers per machine, decided during training.

**Fleet size.** Prefill runs large matrices at ~45% MFU: `8 × 495e12 × 0.45 ≈ 1.8 PFLOP/s` per node, or
`1.8e15 / 1.4e11 ≈ 12,700` prompt tokens/s. Decode at batch 256 is past the ridge point and so
compute-bound: `2 × 70e9 × 256 / 1.8e15 ≈ 20 ms` per step → 12,800 output tokens/s at an ITL of 20 ms,
holding `256 × 1.34 = 343 GB` of KV inside the 460 GB budget. Self-consistent.

```
prefill nodes = 2,320,000 / 12,700 ≈ 183      $/1M input  = 183×$16×24 / 200e3 ≈ $0.35
decode  nodes =   348,000 / 12,800 ≈  27      $/1M output =  27×$16×24 /  30e3 ≈ $0.35
```

Roughly **210 nodes (1,700 GPUs) at average load**, provisioned for peak, at $16/node-hour and ~$80k/day —
at perfect utilization, unattainable because loading 140 GB of weights takes two to five minutes and the
fleet cannot chase a spike (§8). The near-equality of the two token prices is an artifact: at 32k contexts
the decode batch falls to 43 and per-node throughput to ~6,300 tokens/s, making output tokens ~2× dearer
and input 8× dearer. Published prices are several dollars per million; the gap is utilization and margin,
and **utilization is the only one architecture controls.**

---

## 4. API

```
POST /v1/chat/completions            auth: Bearer <api key>
  body:   {model, messages:[{role,content}], max_tokens, temperature, stream:true}
  accept: text/event-stream
  ->  200 text/event-stream
      data: {"id":"r_88","delta":{"content":"Hel"},"index":0}
      data: {"id":"r_88","finish_reason":"stop","usage":{"in":2014,"out":287}}
      data: [DONE]
  ->  429 {error, retry_after}    # quota or admission control
  ->  503                         # queue full — shed rather than queue forever

DELETE /v1/chat/completions/{requestId}  -> 204     # explicit cancel
```

**SSE, not WebSocket.** Once the prompt is submitted the channel is strictly server-to-client. Chapter 02
§8 makes the general case; here the payoffs are a stateless HTTP tier, ordinary infrastructure working
unchanged, and automatic browser reconnect, while WebSocket buys bidirectionality nobody needs and imposes
a connection registry on every tier. What SSE does *not* give you is resumption of a partial reply: the
generation state lived in GPU memory freed when the stream broke.

**`max_tokens` is an admission-control input, not a convenience.** The scheduler must bound how much KV
cache a request can grow into before admitting it; an unbounded request is unschedulable.

**Cancellation must propagate to the GPU.** A client closing the tab with 4,000 tokens still to generate
holds the most expensive resource in the system, so the SSE writer detects the closed connection and tells
the orchestrator to evict the sequence that step. Candidates rarely mention this, and a non-trivial
fraction of streams are abandoned at any moment. Usage is reported in the terminal event: billing is a
server-side fact, not a client claim.

---

## 5. Data model

The durable data here is small and boring; the expensive state is ephemeral and GPU-resident. Making that
distinction is most of this section.

```
-- durable, and small --
conversations   conversation_id (pk), user_id, messages JSON[]  # role, content, tokens
usage_ledger    api_key_id (pk), minute_bucket (ck), tokens_in, tokens_out, requests
model_registry  model_id, version, weights_uri, dtype, tp_degree, context_window

-- GPU-resident, ephemeral, never replicated --
kv_blocks       fixed 16-token blocks in a per-node pool
block_table     sequence_id -> [block_id, ...]   # the paged-attention page table
prefix_index    radix tree over token-id prefixes -> block ids, refcounted
```

**Conversation history is durable; the KV cache of that history is not.** Losing a node loses in-flight
generations and warm prefixes but no user data, which is why the GPU tier runs with no replication or
checkpointing at all.

**KV memory is allocated in fixed blocks, not contiguous per-sequence arenas.** A contiguous allocator
reserves `max_tokens` up front — with a 300-token mean against a 4,096-token ceiling, 13× what is used.
Blocks bound the waste at half a block (2.6 MB) per sequence, under 0.3%: paged attention, §7.2. And the
prefix index keys on token IDs in a radix tree rather than on a hash of the whole prompt, because two
conversations sharing their first 1,800 tokens must share exactly those blocks (§7.6).

---

## 6. Architecture, derived

### Attempt 1: one request per worker

`Client → LB → GPU worker (load model, run generate(), return)`

A request occupies a node for `prefill + 300 × 5.2 ms ≈ 1.7 s`, so `1,160 × 1.7 ≈ 1,970 nodes =
15,800 GPUs` against the 210 nodes §3 says the work requires. **Ten times too expensive**, for exactly the
arithmetic-intensity reason: each node runs at under 1% of the compute it is billed for. It also gets the
latency shape wrong — the reply is buffered until complete, so "TTFT" equals total generation time.

### Attempt 2: static batching

Collect 32 requests, run them together, return when all 32 finish. One weight-read now serves 32
sequences, and two things break.

**The batch runs at the speed of its slowest member.** Chat output lengths are heavy-tailed — mean 300,
p99 above 2,000 — so the expected maximum of 32 draws is around 1,500, and every slot is held until it
finishes: `utilization = E[length] / E[max of 32] ≈ 300 / 1,500 ≈ 20%`. Four-fifths of the batch's
GPU-seconds compute padding for sequences that ended long ago; the nominal 32× is really about 6×.

**TTFT now includes batch formation.** A request arriving just after a batch starts waits for the whole
batch — up to 1,500 × 5 ms = 7.5 s — before it is even considered. Shortening the timeout to protect TTFT
shrinks the achieved batch, destroying the throughput the batching was for: **no setting of the timeout is
good.**

### Attempt 3: continuous batching — the core idea

Stop batching *requests*; batch *iterations*. The scheduler re-forms the batch **before every single
token-generation step**. A sequence that emits its stop token at step 40 leaves at step 41; a request that
arrived during step 40 joins at step 41. Nothing waits for anything to drain.

```
step t:   [A B C D E F G H]     H emits <eos>
step t+1: [A B C D E F G I]     I was queued 6 ms ago; H is gone, its blocks freed
step t+2: [A B C D E F J I]     G finished; J admitted
```

Slot utilization goes from ~20% to essentially 100% while the queue is non-empty, and TTFT is fixed
structurally: a new arrival waits one decode step (~20 ms) plus its own prefill. **And the constraint
moves.** With slots turning over freely, what stops the 1,000th admission is that its KV blocks do not
fit; the scheduler's job becomes memory admission control, not compute scheduling.

### Attempt 4: separate the prefill and decode pools

Prefill processes 2,000 tokens at once — large matrices, compute-bound, high MFU. Decode processes one
token per sequence — matrix-vector products, memory-bound, MFU under 5%. Share a pool and prefill preempts
decode: one 2,000-token prefill is ~156 ms of node time, and **all 256 sequences in the decode batch stall
for it**, an ITL spike of 8× target that users see as the text freezing, constantly rather than
occasionally at 1,160 prefills/second. Chunk the prefill or split the pools (§7.3); split, and size them
independently at 183 and 27 nodes — a ratio no single autoscaling policy would discover.

### Attempt 5: prefix cache and SLO-aware admission

Add a **prefix cache** in front of prefill — each turn's prompt is the previous turn's plus two messages,
a 90%+ shared prefix and the largest cost reduction available (§7.6) — and replace FIFO with an
**SLO-aware scheduler**, since FIFO puts a 4,000-token generation ahead of a 50-token one (§7.5).

### Final architecture

```
                 ┌─ auth + quota (token bucket, Ch. 04 §3) ──► 429
Client ─► API GW ┤─ input safety filter ────────────────────► reject
   ▲ SSE         ┌──────────▼──────────┐  admit iff free KV blocks ≥
   └── tokens ───┤    ORCHESTRATOR     │  prompt + max_tokens estimate
                 │ queue + MLFQ + fair │  route by prefix affinity
                 └───┬─────────────┬───┘
        ┌─────────────▼───┐ KV  ┌───▼─────────────┐
        │ PREFILL POOL    │────►│ DECODE POOL     │
        │ ~183 nodes      │RDMA │ ~27 nodes       │
        │ compute-bound   │     │ memory-bound    │
        │ chunked, MFU 45%│     │ continuous B≈256│
        └────────┬────────┘     └────────┬────────┘
                 ▼                       ▼
        PREFIX CACHE (radix tree;   windowed output filter
        HBM → DRAM → NVMe)          → SSE writer → client

  conversations (DynamoDB) ── usage ledger ── model registry (S3)
```

---

## 7. Deep dives

### 7.1 Continuous batching, precisely

The mechanism is **iteration-level scheduling**: the scheduling unit is one forward pass over the batch,
not one request. Each step the scheduler emits a fresh set of sequences, and the kernel accepts a ragged
batch — different positions, different context lengths, non-contiguous KV blocks. Take eight requests of
lengths `[20, 45, 500, 30, 800, 60, 25, 90]`:

```
static:     8 slots held for 800 steps = 6,400 slot-steps; useful work
            = sum(lengths) = 1,570 slot-steps → 24.5% utilization
continuous: a slot frees the step after its sequence ends and refills → ~100%
```

Published headline numbers (Orca, vLLM) exceed 4× because they compare against per-request serving and so
fold in the batching gain too. **The scheduling contribution alone is `E[max]/E[mean]` of the
output-length distribution** — estimable from production logs. The costs are real: batch composition
changes every step, so one CUDA graph cannot be captured and replayed; attention kernels must handle
per-sequence context lengths, which is why this was a research result rather than an obvious
optimization; and the scheduler sits on the critical path every 20 ms, ruling out a network call per step.

### 7.2 The KV cache is the binding constraint

`kv_bytes = 2 × layers × kv_heads × head_dim × dtype_bytes × len × batch = 0.33 MB × len × batch`. Memory
grows linearly in **both** batch size and sequence length, while compute per step grows in batch size
only, so as context windows grow memory binds before compute — long-context products are memory products.

Worse, KV grows *during* generation: a request admitted at 2,000 tokens holding 0.65 GB holds 2.0 GB by
token 4,000. A scheduler admitting on current occupancy over-commits and hits an OOM mid-batch, which is
unrecoverable at the kernel level. It must admit on **projected** occupancy at `prompt + max_tokens` and
**preempt** when the projection was wrong — swapping a victim's blocks to host memory (fast to restore,
costs PCIe bandwidth) or evicting and recomputing its prefill later. Recompute usually wins: prefill is
cheap relative to a stalled node.

**Paged attention** is the allocation strategy: a pool of fixed 16-token blocks plus a per-sequence block
table — virtual memory applied to attention. **Fragmentation** falls from "reserve `max_tokens`, use 300"
to half a block per sequence, recovering 60–80% of KV memory and so 3–4× the concurrent sequences on the
same hardware. And **sharing becomes expressible**: two sequences with a common prefix point their block
tables at the same physical blocks, refcounted, copy-on-write at the divergence point — which is what
makes §7.6 and multi-sample generation cheap rather than multiplicative, at the price of an indirection in
every attention access.

### 7.3 Prefill and decode are different machines

| | Prefill | Decode |
|---|---|---|
| Work per step | 2,000 tokens × 1 sequence | 1 token × 256 sequences |
| Shape, bound by, MFU | matrix × matrix; compute; ~45% | matrix × vector; bandwidth; <5% |
| Owns which SLO, scales with | TTFT; prompt tokens/s | ITL; concurrent sequences |

Because the phases scale with different quantities, a single pool is sized for whichever binds first and
wastes the other. Here the ratio is 183:27, and it moves with product mix.

**Option A: chunked prefill.** Split a prefill into 512-token chunks and attach each to a decode batch as
extra work. The per-step ITL stall falls from 156 ms to ~40 ms, with one pool, one scaling policy, and no
cross-node KV transfer. Costs: lower prefill MFU and higher TTFT, since a prompt now takes four scheduling
rounds.

**Option B: disaggregation.** Separate pools; prefill runs a prompt to completion and ships its KV blocks
to a decode node over RDMA. Each pool is batched and autoscaled for its own profile, a prefill spike
cannot stall decode at all, and the pools may use different parallelism. The cost is the transfer:
`2,000 × 0.33 MB = 660 MB` per request at 1,160 req/s is **765 GB/s of cross-pool traffic**, an
InfiniBand-class requirement and the reason disaggregation is a large-fleet technique, not a default;
proposing it without pricing the interconnect is an unfinished thought. **Choose B above roughly a hundred
nodes, A below** — the crossover is where capacity wasted by a mis-sized pool exceeds the cost of fabric.

### 7.4 Two latency metrics that fight each other

**TTFT = queue wait + prefill compute**; reduce it by admitting sooner, prioritizing prefill, and making
prefill cheaper. **ITL = decode step time**: below the ridge point (batch 148) step time is flat at
~5.2 ms, so batching is free; above it, step time grows linearly — batch 256 costs 20 ms, batch 512 costs
40 ms. **ITL is a direct function of how much throughput you extracted.**

Larger batches mean higher throughput, lower cost per token, and **worse ITL**. Prioritizing prefill
improves TTFT and, since each prefill inserted into the decode stream stalls it, **worsens ITL**.
Prioritizing decode smooths ITL and **worsens TTFT** as requests queue before prefill. No configuration
optimizes all three. Name both SLOs, then maximize **goodput** — tokens per second delivered *inside
both*; tokens produced for a request whose TTFT already blew past 1.5 s have negative value, having
consumed capacity that would have kept another request compliant. Between the ridge point at 148 and the
KV limit at 343 lies a real operating window: hold ITL at its target by capping batch size, hold TTFT by
shedding rather than by queueing, and let the batch be the largest whose step time keeps ITL under
target — measured continuously, never a config constant.

### 7.5 Queueing and SLO-aware scheduling

FIFO is wrong here for a workload-specific reason: **service time varies 100× and is unknown in advance.**
A 4,000-token generation holds a slot for 80 seconds, a 40-token one for 0.8. Under FIFO the short request
waits behind the long one, and the ratio of wait to service — what users perceive as slowness — is
catastrophic for exactly the requests that should feel instant, and continuous batching does not fix it:
slots free one at a time, and long generations hold theirs throughout.

1. **FIFO**, the baseline: fair in arrival order, terrible tail. **Shortest-job-first** would be optimal
   for mean wait, but length is unknown before generation; it can be *predicted* by a small classifier,
   and needs an aging term or long requests starve.
2. **Multi-level feedback queue.** Every request starts at top priority and is demoted after *N* generated
   tokens. Short requests finish before demotion; long ones sink and yield to arrivals. This approximates
   SJF with no prediction, and needs preemption — which paged attention already provides.
3. **Per-tenant fair queueing.** Required regardless: one account issuing 500 concurrent long generations
   would otherwise take the fleet. Deficit round-robin over accounts is cheap and sufficient.

Take **(2) plus (3)**. The cost is that very long generations get measurably slower under load, which is
the right place to put the pain — a user waiting on a 4,000-token essay is already committed and is not
staring at a blank screen. **And bound the queue**: an unbounded one turns overload into an availability
incident, since every admitted request then misses its SLO (Chapter 03 §6). When projected wait exceeds
the TTFT SLO return 429 with `retry_after`; shedding at the door is the difference between a degraded
service and a dead one.

### 7.6 Conversation state and prefix caching

The model is stateless between calls, so turn 5 must present turns 1–4 verbatim plus a new message.
**Resend and recompute** is simple, fully stateless, and quadratic: an *n*-turn conversation prefills
`O(n²)` tokens — ten turns of 200 is 11,000 prefilled tokens to produce 2,000 of context. **Cache the
prefix** instead: retain KV blocks after a request finishes, keyed in a radix tree by the token-ID prefix
that produced them, so turn 5 matches 1,800 of its 2,000 tokens and prefills only 200.

```
prefill work with caching = 200 / 2,000 = 10% of uncached
fleet impact at a 70% hit rate: 183 prefill nodes → ~60
```

**This is the largest single cost lever in the design** — roughly 40% of total fleet cost from one cache,
and why §3 established that prefill dominates. It is also why providers discount cached input tokens: that
discount is architectural, not promotional.

The cache is refcounted GPU memory — the same scarce resource active sequences need — so **the prefix
cache competes directly with concurrency**. Evict cold prefixes (LRU over the radix tree, leaves first)
and tier them: hot in HBM, warm in host DRAM (restoring 660 MB at 25 GB/s takes 26 ms against the 156 ms
of prefill it replaces, so the second tier pays), cold on NVMe.

Routing changes with it. A prefix cache is node-local, so **round-robin load balancing destroys the hit
rate**: turn 5 landing on a different node than turn 4 finds a cold cache. The orchestrator routes by
prefix affinity — hash the leading tokens, prefer the node holding them, fall back to load when it is
saturated — which is consistent hashing (Chapter 04 §6) applied to conversation prefixes, a caching
decision propagating into the load-balancing tier. Partition by tenant, too, or a timing side channel
reveals whether another customer prompted with a given prefix.

### 7.7 Safety filtering as pre- and post-stages

**Input filtering** runs before admission — a small classifier costing 10–30 ms, about 5% of the TTFT
budget, which avoids spending 156 ms of prefill plus seconds of decode on a request that will be rejected.
Run it in parallel with queue admission so it overlaps the wait.

**Output filtering** is harder because the output streams. Buffering the whole reply first is correct and
destroys the product — TTFT becomes total generation time, the failure of attempt 1 — while filtering per
token is meaningless, since safety is not a property of a token. So filter on a **sliding window**:
buffer, classify, release, ending the stream with `finish_reason: "content_filter"` on a violation. Fifty
tokens costs 1 s of added TTFT, too much; start at 10 and grow. Accept the consequence honestly:
**partial unsafe output may already have reached the client** before the classifier fires on the window
containing it. Streaming and perfect moderation are in tension; mitigate with a short first window, a
client contract that `content_filter` means redacting displayed text, and full buffering for high-risk
contexts flagged by the input classifier.

### 7.8 Cost per token as the objective function

```
cost/token = (node $/hour) / (tokens/sec per node × 3,600),  where
tokens/s/node = batch_size / step_time  and
batch_size    = min(KV_capacity / kv_per_seq, the ITL-constrained batch)
```

The denominator is what architecture controls, giving four levers in order of effect: **raise achieved
batch size** (continuous batching §7.1, paged attention §7.2 — 5–10× here); **do less work** (prefix
caching §7.6 — ~40% of fleet cost); **shrink KV bytes per token** (GQA, 8×, plus fp8 KV quantization,
another 2× — each halving doubles concurrency and halves decode cost); and **shrink weight bytes** (fp8 or
int4 halves or quarters the 5.2 ms memory floor, at a quality risk that must be measured, not assumed).

Then the honest summary of the chapter: **utilization and latency are in direct opposition, and no clever
architecture dissolves the conflict.** Running at 95% KV occupancy with batch 400 gives the lowest cost
per token, the worst ITL, and no headroom for a spike; 50% gives excellent latency at twice the price. The
design's job is to expose that dial and let the product set it, differently per tier.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| KV cache exhausted mid-batch | Block allocation fails; the step would OOM | Admit on *projected* occupancy; preempt-and-recompute a victim (LIFO by admission time); never let a step fail |
| Decode node crashes | ~256 in-flight streams die; KV is unreplicated and unrecoverable | No resume is possible — retry from scratch and do not bill it; keep blast radius small with more, smaller nodes |
| Traffic spike, and slow scale-out | Queue grows and TTFT is breached fleet-wide; 140 GB of weights take 2–5 min to load, so capacity arrives after the spike | Shed with 429 + `retry_after`; warm standby nodes; weights pre-staged on local NVMe; scale on queue-wait trend and forecast, never on instantaneous CPU |
| Client disconnects mid-stream | Generation continues, burning the scarcest resource | Detect the closed SSE connection, propagate cancel, free blocks that step |
| One tenant floods with long generations | Fleet occupied; everyone else queues | Per-tenant fair queueing, per-key concurrency caps, `max_tokens` ceilings |
| Prefix cache thrash | Hit rate collapses; prefill load jumps 3× | Alert on hit rate, not only latency; cap the cache's HBM share; prefix-affinity routing |
| Safety classifier slow, or degenerate repetition loop | TTFT inflated, or one request runs to `max_tokens` of noise | Classifier timeout with an explicit fail-open/fail-closed policy per risk tier; hard `max_tokens` stop plus repetition detection |

**Monitoring.** This dashboard looks nothing like a web service's, in roughly this order of value:
**TTFT p50/p95** and **ITL p95** as separate series, never combined; **KV cache utilization**, the true
saturation signal and the right autoscaling input; **achieved batch size**; **preemption rate**, where
non-zero means admission control is over-optimistic; **queue depth and wait**; **prefix cache hit rate**;
**goodput**, tokens/second delivered within both SLOs; and **MFU**. Absent: CPU utilization is
meaningless, request rate nearly so, and the error rate reads healthy while the service delivers 200 ms
per token.

---

## 9. Common mistakes

1. **Treating it as a stateless web service.** Load balancer plus replicas plus autoscale-on-CPU fails on
   every axis: routing must be KV- and prefix-aware, replicas take minutes to start, and CPU is not what
   is contended.
2. **Assuming compute is the constraint.** It is memory; sizing from FLOPs alone and never deriving KV
   bytes per token misses the central fact of the problem.
3. **Proposing static batching**, or "batch for 50 ms and then run" — the mean-over-max arithmetic wastes
   ~80% of the batch, and no timeout setting fixes it — and, relatedly, **naming a single latency SLO**,
   when TTFT and ITL come from different mechanisms and trade against each other.
4. **Round-robin load balancing.** It destroys a prefix cache hit rate worth ~40% of fleet cost, ignores
   which nodes have free KV blocks, and pairs naturally with **resending full history with no prefix
   cache**, accepting `O(n²)` prefill per conversation and paying for it in GPUs.
5. **Unbounded queueing.** Under overload every admitted request misses its SLO; shedding preserves
   service for the requests you accept. Related: **ignoring cancellation**, since abandoned streams burn
   the most expensive resource in the system and are invisible in request-rate metrics.
6. **Claiming streamed output can be fully safety-filtered.** It cannot; the honest answer is a windowed
   filter with a stated residual risk.

---

## 10. Variants

**Offline / batch inference.** No ITL or TTFT SLO, only a deadline. Everything inverts: maximize batch
until KV memory is full, sort by predicted length so batches are homogeneous (static batching is fine once
you can group by length), run on spot capacity. This is why batch APIs cost near half of interactive ones,
and deriving that difference is a strong answer.

**Embedding service.** Prefill only — no decode, no retained KV, no streaming — so the constraint reverts
to plain compute and the design collapses to a batched service with a queue — a useful contrast, isolating
which complications came from autoregressive generation.

**Editor code completion.** TTFT budget ~200 ms because the suggestion must beat the next keystroke,
outputs of 20–50 tokens, enormous prefix reuse since the file changes by one character between requests.
Smaller model, aggressive prefix caching, and **speculative decoding** — a draft model proposes tokens the
large model verifies in one pass, cutting effective ITL 2–3×.

**RAG, agentic loops, and LoRA multi-tenancy.** RAG adds retrieval to TTFT (Chapter A4 §7.2) and inflates
prompts toward prefill; since retrieved context varies per query, place passages *after* the stable system
prompt so the cacheable prefix survives — a small ordering decision with a large cost consequence. Agent
loops are many short calls over one long, growing context, so prefix hit rates approach 95%. And LoRA
multi-tenancy puts thousands of small adapters on one base model with per-sequence selection, sharing the
base-model weight read across all of them: §3's amortization argument on a new axis.

---

## 11. Further reading

- Chapter 02 §8 for the SSE decision; Chapter 03 §6 for backpressure; Chapter 04 §3 and §6 for quota tiers
  and prefix-affinity routing
- Yu et al., "Orca: A Distributed Serving System for Transformer-Based Generative Models," OSDI 2022
  (iteration-level batching); Kwon et al., "Efficient Memory Management for Large Language Model Serving
  with PagedAttention," SOSP 2023 (vLLM and the paged KV cache)
- Ainslie et al., "GQA: Training Generalized Multi-Query Transformer Models," EMNLP 2023
- Agrawal et al., "Taming Throughput-Latency Tradeoff in LLM Inference with Sarathi-Serve," OSDI 2024
  (chunked prefill); Zhong et al., "DistServe," OSDI 2024, and Patel et al., "Splitwise," ISCA 2024
  (prefill/decode disaggregation); Leviathan et al., "Fast Inference from Transformers via Speculative
  Decoding," ICML 2023
- Williams, Waterman, and Patterson, "Roofline: An Insightful Visual Performance Model," CACM 2009 — the
  arithmetic-intensity framing used in §3
