# Chapter A2 — Code Judge (LeetCode)

> **Prerequisites:** Chapters 03 (queues, backpressure, at-least-once), 92 (scheduled bursts), 02 (caching)
> **Patterns:** untrusted code execution, sandbox layering, worker pools under predictable bursts, determinism on shared hardware

---

## 1. The problem

A user submits source code. The system compiles it, runs it against a set of test cases with time and
memory limits, and returns a verdict: accepted, wrong answer, time limit exceeded, runtime error, or
compile error.

The queueing, the worker pool, and the result storage are all ordinary — variations on patterns already
covered. What makes this chapter worth writing is a requirement no other chapter in the book has:
**you are deliberately executing code written by an anonymous adversary, on your hardware, at a rate of
thousands per minute.** Every other system here defends against malformed input. This one accepts arbitrary
programs as its input and must run them.

That reframes the design. The sandbox is not a component of the system; it is the system, and everything
else is scheduling around it.

The second, quieter difficulty is **determinism on shared hardware**. A verdict of "time limit exceeded"
is a claim about how long a program took, measured on a machine that is also doing other things. If the
same submission gets different verdicts on different runs, the product's core promise — that the judgment
is fair — is broken, and users will notice quickly and complain loudly.

**The property that makes it hard:** hostile code must run with enough isolation that a successful escape
is implausible, with enough resource determinism that timing verdicts are reproducible, and cheaply enough
that a contest's opening burst does not require a fleet sized for the peak.

---

## 2. Requirements

### Functional

1. Accept a submission (language, source, problem) and return a verdict.
2. Run against a hidden test suite with per-problem time and memory limits.
3. Support a "run against my own input" mode, which returns output rather than a verdict.

Defer, but name: the contest ranking system (Chapter 72), plagiarism detection, interactive and
special-judge problems (§10), and the problem-authoring pipeline.

### Non-functional

- **Verdict latency** — p50 under 5 seconds, p99 under 30 seconds outside contests. Users wait on this
  screen, and it is the product's most-watched number.
- **Isolation** — a submission must not read other submissions, reach the network, persist anything, affect
  a concurrent run, or escape to the host. This is the requirement; everything else is negotiable.
- **Determinism** — the same submission against the same tests should yield the same verdict. §7.3 is about
  the fact that this is only approximately achievable and what to do about it.
- **Throughput** — 100 submissions/sec sustained, 2,000/sec in a contest burst; §3.
- **Fairness under load** — a contest must not let one user's 50 submissions delay everyone else.
- **Cost** — the fleet is mostly idle between contests, so the design must not require provisioning for the
  peak. §7.4.

### Explicitly out of scope

The problem content itself, the editor, and discussion features.

---

## 3. Estimation

Assume 5 million submissions per day, with contests twice a week.

**Steady-state rate**

```
5e6 / 86,400 ≈ 58 submissions/sec average
peak (evening, non-contest, 2×) ≈ 120/sec
```

**Contest burst — the constraining number**

```
contest with 30,000 participants
  opening minute:   ~40% attempt problem A immediately
                    12,000 submissions in ~120 s → ~100/sec on top of baseline
  final minute:     everyone submits their last attempt at once
                    ~15,000 submissions in 60 s  → 250/sec
  plus resubmissions after failure: ×2-3
                                                 → ~600-750/sec sustained for minutes
```

**Compute per submission**

```
compile (C++/Java):        1-3 s of CPU
run 50 tests × up to 2 s:  up to 100 s worst case, typically ~5 s
container setup/teardown:  0.3-1 s        ← significant, see §7.4
                          ─────────
typical total:             ~8 s of CPU per submission
```

**Worker count**

```
steady:  58/sec × 8 s = 464 concurrent executions
contest: 700/sec × 8 s = 5,600 concurrent executions
```

**Twelve times the steady-state capacity, needed for about ten minutes, twice a week.** That ratio is the
economic problem of this system, and §7.4 is about not paying for it continuously. It also says something
about the sandbox choice: at 5,600 concurrent executions, a per-run isolation mechanism costing an extra
second of setup costs 5,600 seconds of machine time per wave.

**Storage**

```
5e6 submissions/day × ~5 KB source ≈ 25 GB/day ≈ 9 TB/year
test data: ~10,000 problems × ~50 MB ≈ 500 GB, read constantly, highly cacheable
```

---

## 4. API

```
POST /submissions
  {problemId, language, source}
  -> 202 {submissionId, queuePosition}

GET  /submissions/{id}
  -> 200 {status: queued|running|done,
          verdict, passedTests, totalTests,
          runtimeMs, memoryKb, failingTestIndex?, compileError?}

GET  /submissions/{id}/stream          # SSE: per-test progress
POST /run                              # custom input, returns stdout, no verdict
```

Four decisions:

**`202` with a `submissionId`, and results are polled or streamed.** Judging takes seconds; holding an HTTP
request open for it wastes a connection and fails badly on mobile. `queuePosition` is returned because
during a contest the honest answer to "why is this slow" is "you are 3,000 deep in the queue", and showing
it prevents the resubmission storm that follows from users assuming something broke.

**SSE for progress, not WebSocket.** Server-to-client only, short-lived, no client input on the channel.
(Chapter 02, §8.) Streaming per-test progress meaningfully improves perceived latency — a user watching
tests pass is not waiting in the same way.

**`failingTestIndex` but not the failing test's content.** Users need to know they failed on test 34; giving
them the input to test 34 lets them extract the hidden test suite through repeated submission. This is a
genuine adversarial concern rather than a hypothetical (§7.6), and the API is where it is enforced.

**`/run` with custom input is a separate endpoint with its own quota.** It is the same sandbox with a
different result shape, and it is the more attractive target for abuse, because the user controls both the
code and the input and gets the raw output back. Rate limit it more aggressively than submissions.

---

## 5. Data model

```
submissions
  submission_id  BIGINT PK
  user_id, problem_id, language
  source_ref     TEXT          -- object storage key, not inline
  status         SMALLINT
  verdict        SMALLINT
  runtime_ms, memory_kb
  failing_test   INT NULL
  created_at, judged_at

problems
  problem_id     BIGINT PK
  time_limit_ms, memory_limit_kb          -- per language multipliers, §7.3
  test_data_ref  TEXT                     -- object storage
  checker_ref    TEXT NULL                -- special judge, §10

judge_results                             -- per test, for debugging and analytics
  submission_id, test_index, verdict, runtime_ms, memory_kb
```

Three deliberate decisions:

**Source lives in object storage, not in the database row.** It is 5 KB, it is written once and read
approximately once, and 9 TB a year of it does not belong in the transactional store that also serves the
submission-status polling.

**Time limits are per-problem *and* per-language.** A Python solution to the same problem legitimately needs
several times the C++ budget. Storing a single limit and applying a language multiplier at judge time is
the standard approach, and getting this wrong makes entire languages unusable for the harder problems.

**Per-test results are stored, not just the aggregate.** They cost little and they answer the questions that
actually arise: which test is flaky, whether a problem's limits are too tight, and whether a verdict changed
between runs of the same code.

---

## 6. Architecture, derived

### Attempt 1: run the code in the API process

`exec` the submission directly.

Catastrophic, and worth one sentence for the enumeration: the submission runs with the API server's
privileges, can read its environment and secrets, can reach every internal service the server can reach,
can consume all its memory, and can simply not terminate. Every one of these is exploited within hours of
launch.

### Attempt 2: a separate worker process with `ulimit` and a different user

Run as an unprivileged user, apply `RLIMIT_CPU` and `RLIMIT_AS`, `chroot` into a directory.

A real improvement and still not sufficient:

- **No network isolation.** The process can open sockets — to the internal network, to the metadata service
  of the cloud instance (a classic credential-theft path), or outward to exfiltrate the test data.
- **`RLIMIT_CPU` measures CPU time, not wall time.** A program that sleeps, or blocks on I/O, or spawns
  children, runs indefinitely.
- **Fork bombs.** `RLIMIT_NPROC` helps and is per-user, so two concurrent submissions as the same user
  share the limit and interfere.
- **The filesystem is visible.** `chroot` is not a security boundary; escaping it from a process that can
  obtain a file descriptor to a directory outside is a known technique.
- **Everything on the host is a shared resource** — page cache, disk, CPU — so one submission affects
  another's measured runtime.

### Attempt 3: a container per run

Each submission runs in its own container: namespaced PIDs, mounts, network (none), and users; cgroups
capping CPU, memory, and process count; a read-only root filesystem with a small writable scratch mount;
all capabilities dropped; and a seccomp profile restricting syscalls to the small set a computation needs.

This addresses everything in attempt 2, and it is the right baseline. **But a container is not a security
boundary against a kernel exploit** — every process still calls into the same shared kernel, and a kernel
vulnerability reachable through a permitted syscall is a full escape. That is not a theoretical caveat; it
is the reason the next attempt exists.

Two other properties matter here: **the container is created fresh and destroyed after every run**, so
there is no persistence between submissions, and setup costs 0.3–1 second, which §3 identified as
significant at contest scale.

### Attempt 4: harden the boundary

Two ways to reduce the kernel attack surface, and they are not equivalent:

**A micro-VM per run** (Firecracker, gVisor's platform variants). A separate guest kernel means an escape
requires a hypervisor vulnerability, which is a far smaller and far more scrutinized surface than the Linux
syscall interface. Boot times are ~125 ms for a minimal micro-VM — comparable to a container — which makes
this practical rather than aspirational.

**A user-space kernel** (gVisor). Intercepts syscalls and implements most of them in user space, so the
host kernel sees a small, fixed set. Lower overhead than a VM, weaker isolation than one, and a real
compatibility cost — some syscalls are unimplemented, which surfaces as mysterious failures for the
languages and runtimes that use them.

**Choose the micro-VM for a public judge.** The population is anonymous and includes people actively
looking for an escape, the incremental cost over a container is small, and the difference is
qualitative — a container escape is a bug in a large shared kernel; a VM escape is a bug in a small
hypervisor.

**Layer regardless.** Micro-VM *and* seccomp *and* no network *and* cgroups *and* an unprivileged user
inside the guest. Defense in depth, because the assumption is that any single layer will eventually have a
vulnerability.

### Attempt 5: pool and pre-warm the workers

At contest scale, 0.3–1 s of setup per run against ~8 s of work is 4–12% of the fleet spent on setup, and
it is latency the user sees. Keep a pool of pre-booted sandboxes, hand a submission to a warm one, and
destroy it afterward while a replacement boots in the background.

The rule that makes this safe: **a sandbox serves exactly one submission and is then destroyed.** Reusing
one across submissions saves the boot but reintroduces the possibility that submission N leaves something
behind for submission N+1 — in `/tmp`, in a shared memory segment, in a leaked process. Pre-warm, never
reuse.

### Attempt 6: separate the queues

A single FIFO queue means a contest burst delays every non-contest user, and one user's 50 rapid
submissions delay everyone. Split by class (contest / practice / custom-run) with separate capacity, and
apply per-user fairness within each. §7.5.

### Final architecture

```
  Client ──► API ──► object storage (source)
               │
               ▼
     ┌─── queues, by class ──────────────────┐
     │  contest (priority)  practice  custom │
     └──────────┬────────────────────────────┘
                │  per-user fair scheduling (§7.5)
                ▼
     ┌── Judge orchestrator ─────────────────┐
     │  claims work, picks a warm sandbox    │
     └──────────┬────────────────────────────┘
                ▼
     ┌── Worker node (dedicated core per run, §7.3) ──┐
     │  ┌── warm pool of micro-VMs ──┐                │
     │  │  no network                │                │
     │  │  cgroup: cpu, mem, pids    │  1 submission  │
     │  │  read-only rootfs + scratch│  then DESTROY  │
     │  │  seccomp, no capabilities  │                │
     │  └────────────────────────────┘                │
     │     compile → run tests → collect              │
     └──────────┬─────────────────────────────────────┘
                │  test data from local cache ◄── object storage
                ▼
        results ──► Postgres ──► SSE to client
```

---

## 7. Deep dives

### 7.1 The sandbox, layer by layer

The single most important part of the design, and the one an interviewer will push on. Each layer, what it
stops, and what it does not:

| Layer | Stops | Does not stop |
|---|---|---|
| Unprivileged user | Direct access to host files and processes | Anything exploiting a kernel bug; resource exhaustion |
| Mount namespace + read-only rootfs | Persisting anything; reading the host filesystem | Reading data mounted into the sandbox |
| Network namespace with **no interfaces** | All exfiltration and all lateral movement | Nothing else |
| PID namespace | Seeing or signalling other processes | Fork bombs (needs the pids cgroup) |
| cgroups: cpu, memory, pids | Resource exhaustion of the host | Wall-clock overrun from sleeping or blocking |
| Dropped capabilities | Privileged operations even as root inside | Unprivileged kernel attack surface |
| seccomp allowlist | Most of the syscall attack surface | Bugs in the syscalls you must permit |
| Micro-VM / guest kernel | Host kernel exploitation | Hypervisor vulnerabilities |
| One-shot sandbox lifecycle | Cross-submission contamination | In-run misbehavior |
| Wall-clock watchdog **outside** the sandbox | Anything that ignores in-sandbox limits | — |

Three points deserve emphasis:

**No network interface at all, not a firewall.** A sandbox with a network namespace containing only
loopback cannot reach anything, by construction. Filtering rules can be misconfigured; an absent interface
cannot. The cloud instance metadata endpoint is the specific thing this protects — it is reachable from
any process with a network stack and it hands out credentials.

**The wall-clock watchdog must live outside the sandbox.** Every limit enforced by the kernel inside the
guest can, in principle, be affected by whatever is happening inside. An external supervisor that kills the
sandbox after a hard deadline is the backstop that does not depend on anything inside working correctly.

**Read-only rootfs with a small writable scratch mount, sized and capped.** Compilers need to write; the
system must not let a submission fill a disk shared with anything else. The scratch is discarded with the
sandbox.

### 7.2 Compilation is also untrusted

Easy to overlook: the *compiler* runs on attacker-supplied input, and compilers are large programs with
their own vulnerabilities and their own dangerous features.

- **Compile-time execution.** C++ template metaprogramming can consume unbounded memory and time at compile
  time. A few dozen lines can exhaust any memory limit before a single instruction runs.
- **`#include` of arbitrary paths.** A submission can attempt to include files outside its directory, which
  is both an information leak and, historically, a way to read the test data if the filesystem layout is
  careless.
- **Compiler plugins and pragmas** that alter behavior or invoke external programs.
- **Output size.** A submission can emit a binary large enough to fill the scratch space.

**Compile inside the sandbox, with its own limits**, which are usually more generous than the run limits
but must exist. And keep the test data out of the sandbox during compilation — feed it in afterward, or
mount it read-only only for the run phase.

### 7.3 Determinism, and why "time limit exceeded" is inherently fuzzy

A verdict that depends on which machine happened to run it is unfair, and the same submission genuinely can
pass on one run and fail on the next. The sources:

- **CPU contention.** Other submissions on the same host compete for cores, memory bandwidth, and shared
  cache. Cache pressure alone can change runtime by tens of percent.
- **Frequency scaling and thermal throttling.** The same physical core is not the same speed at all times.
- **Heterogeneous fleets.** Machines purchased two years apart differ substantially.
- **Runtime warmup.** JIT-compiled languages have a warmup phase whose cost varies with system state.
- **Memory layout and address space randomization**, which perturb cache behavior run to run.

Mitigations, in descending order of effectiveness:

**Pin each run to a dedicated core, and isolate that core** from the general scheduler. This removes the
largest source of variance and costs capacity, because a machine's concurrency is now its core count rather
than an oversubscribed multiple.

**Disable frequency scaling on judge hosts.** A fixed clock is slower at peak and consistent, and
consistency is the product requirement.

**Measure CPU time, not wall time, for the verdict** — while still enforcing wall time as a safety
backstop. CPU time is far less affected by contention. The subtlety: this makes a program that sleeps or
blocks pass the CPU limit while consuming enormous wall time, which is exactly why the external wall-clock
watchdog (§7.1) must remain.

**Calibrate limits generously.** If the intended solution runs in 200 ms, set the limit at 2 seconds, not
250 ms. Tight limits are how a problem becomes a test of luck. This is a problem-authoring discipline
rather than an engineering one, and it is the highest-leverage fix available.

**Re-run near-boundary submissions.** A submission that exceeds the limit by a small margin is re-run,
possibly on a quieter host, and the better result is taken. This costs capacity and removes most user-
visible flakiness. Some judges also normalize by running a fixed benchmark on each host and scaling limits
accordingly.

**Be honest about the residual.** Perfect determinism on shared hardware is not achievable. The design goal
is that flakiness is rare enough not to be the user's experience of the product, and the honest interview
answer says so rather than claiming exactness.

### 7.4 Contest bursts and worker pool sizing

From §3: 12× the steady-state capacity, needed for ten minutes, twice a week. Provisioning for the peak
means the fleet is ~92% idle.

Four responses, and the right answer combines them:

**Autoscale ahead of the burst, not in response to it.** Contests are *scheduled* — the start time is known
days in advance. Scale up fifteen minutes before, scale down thirty minutes after. This is the single most
effective measure and it requires no cleverness, only that someone connect the contest calendar to the
scaling policy. Reactive autoscaling is useless here: instance provisioning takes minutes and the burst is
minutes long, so the capacity arrives as the burst ends.

**Accept queueing, and show it.** A 30-second queue during a contest's opening minute is tolerable if the
UI says "position 2,400 in queue". It becomes intolerable when it looks like a hang, because users
resubmit, which is the resubmission storm that turns a 2× burst into a 6× one. `queuePosition` in the API
(§4) is a capacity measure disguised as a UX detail.

**Use spot or preemptible capacity for the burst.** Judging is idempotent and interruptible — a submission
whose worker vanishes is simply re-queued — so the workload is close to ideal for cheap interruptible
instances. This is worth stating because it is a real cost argument that follows directly from a property
of the workload.

**Reduce per-run cost.** The 0.3–1 s of sandbox setup is 4–12% of the fleet at burst (§3). Pre-warming
(§6, attempt 5) reclaims most of it.

**Fail fast on the common case.** Most contest submissions fail on an early test. Running tests in order,
smallest first, and stopping at the first failure means the median submission uses a fraction of the worst-
case budget. Ordering test cases so that cheap discriminating tests come first is a problem-authoring
practice with a direct capacity payoff.

### 7.5 Fairness under load

Two distinct fairness problems, with different fixes.

**Across classes.** A contest burst must not starve practice users, and vice versa. Separate queues with
reserved capacity per class — not a shared queue with priorities, which lets a sufficiently large
high-priority burst consume everything.

**Within a class, across users.** A user submitting 50 times in a minute must not occupy 50 workers while
others wait. Per-user concurrency limits (at most N in flight) plus a fair scheduler that round-robins
across users rather than draining the queue in arrival order. **Weighted fair queueing across users is the
right model**, and the practical version is one sub-queue per active user, serviced round-robin.

**Rate-limit at submission time as well.** A per-user submission rate limit prevents the queue from filling
with one person's attempts in the first place, and it is a cheaper defense than fair scheduling because it
never admits the load.

**The `/run` endpoint needs a stricter quota** than submissions, because it is unbounded — a user can call
it continuously without ever solving anything, and it is the endpoint an abuser would use to farm free
compute.

### 7.6 Test data confidentiality, and the abuse surface

Hidden tests are the product. Two ways they leak, and both are actively attempted:

**Direct exfiltration.** A submission that can read the test files and send them out. The network namespace
with no interfaces closes the sending half absolutely, and mounting test data only for the run phase (not
compile) and only for the tests being executed narrows the reading half. Some judges feed input on stdin
and never expose a file at all, which is the strongest version.

**Oracle extraction through verdicts.** Even with perfect isolation, a submission can encode information in
its *verdict*. A program that reads its input and deliberately crashes if the third number is odd, and
loops forever otherwise, extracts one bit per submission from the failing test index and verdict. With
enough submissions the test data is reconstructed.

This cannot be prevented, only made expensive:

- **Rate-limit submissions per user per problem.** The attack needs many submissions; a limit makes it slow.
- **Do not reveal the failing test index in contests.** Revealing only "wrong answer" removes a large part
  of the channel.
- **Randomize test order per submission**, so the index carries less information.
- **Detect the pattern.** A user submitting dozens of near-identical programs that differ only in a
  conditional is a recognizable signature.

Related abuse to name: **using the judge as free compute** — mining, or running long computations across
many submissions — which the CPU and wall limits bound but do not eliminate, and which per-user quotas
address economically rather than technically.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Sandbox escape | Complete compromise of a judge host | Layered isolation (§7.1); judge hosts in an isolated network segment with no credentials and no path to production; assume eventual compromise and limit the blast radius |
| Worker crash mid-judge | Submission stuck in `running` | Lease with timeout; re-queue on expiry; judging is idempotent so a duplicate run is harmless |
| Fork bomb | Host process table exhausted | pids cgroup; PID namespace; external watchdog |
| Disk fill from a submission's output | Host disk exhausted, affecting co-located runs | Capped scratch mount per sandbox; the mount is discarded with the sandbox |
| Contest burst exceeds capacity | Long queues, resubmission storm | Pre-scale on the contest calendar; surface queue position; per-user rate limits |
| Test data cache miss storm | Object storage saturated at contest start | Pre-warm test data onto workers before the contest; the problem set is known in advance |
| Flaky TLE verdicts | Users lose contests to noise, and complain | Dedicated cores, fixed CPU frequency, CPU-time measurement, generous limits, re-run near the boundary |
| Compiler resource exhaustion | Worker consumed before any code runs | Compile inside the sandbox with its own limits (§7.2) |

**Monitoring:** queue depth and wait time **per class**; verdict distribution over time, where a sudden
shift in TLE rate means a fleet problem rather than a user problem; **re-run disagreement rate**, which is
the direct measure of determinism and the metric nobody instruments until users complain; sandbox setup
time; per-host runtime variance for a fixed calibration program; worker utilization against the contest
calendar; and any seccomp violation, which should be rare and is always worth reading.

---

## 9. Common mistakes

1. **Treating the sandbox as a checkbox.** It is the system. A design that says "run it in Docker" and
   moves on has skipped the only hard part.
2. **Believing a container is a security boundary against a kernel exploit.** It reduces the surface; it
   does not separate the kernel.
3. **Firewalling instead of removing the network.** No interface is a stronger and simpler guarantee than
   any rule set, and it closes the instance-metadata credential path definitively.
4. **Forgetting that compilation is untrusted**, and letting a template-metaprogramming bomb consume a
   worker before any code executes.
5. **Reusing sandboxes across submissions** to save boot time, reintroducing cross-submission contamination.
6. **Ignoring determinism**, and shipping a judge whose TLE verdicts depend on what else was running.
7. **Relying only on in-sandbox limits**, with no external wall-clock watchdog.
8. **Provisioning for the contest peak**, paying for 12× capacity that is idle 99% of the time, when the
   bursts are on a published calendar.
9. **A single FIFO queue**, letting a contest burst or one user's 50 submissions starve everyone else.
10. **Revealing the failing test's contents**, handing over the hidden test suite one submission at a time.

---

## 10. Variants

**Interactive problems.** The submission talks to a judge program over stdin/stdout in a dialogue rather
than reading a fixed input. The sandbox now hosts two communicating processes with their own limits, and
the interaction itself must be time-bounded — a submission that stops reading must not hang the judge.
Materially harder, and worth naming as such.

**Special judges (checkers).** Problems with multiple valid answers need a checker program to validate
output. The checker is trusted code but runs on untrusted output, so it needs its own limits and its own
hardening against malformed input.

**CI as a service.** The same problem at a different scale: untrusted code, longer runs, and — the
significant difference — the code legitimately needs network access to fetch dependencies. That single
requirement removes §7.1's strongest layer and forces a much more complex policy: egress proxies,
dependency allowlists, and per-tenant network isolation. A good illustration of how much of this design
rests on "no network".

**Serverless function platforms.** Untrusted code with a hard cold-start latency requirement, which is why
micro-VMs were built for this workload in the first place. Multi-tenant on shared hosts, so the isolation
requirement is if anything stronger, and the determinism requirement is absent.

**Automated grading for courses.** Lower volume, known and non-anonymous users, so the threat model relaxes
considerably — but plagiarism detection becomes a primary feature rather than an afterthought, and
submission history matters more than throughput.

---

## 11. Further reading

- Chapter 03, for queueing, leases, and at-least-once execution; Chapter 92, for scheduled bursts;
  Chapter 72, for the contest ranking this feeds
- Agache et al., "Firecracker: Lightweight Virtualization for Serverless Applications" (NSDI 2020) — the
  micro-VM design and the boot-time numbers behind §6, attempt 4
- The gVisor documentation on its security model, for the user-space-kernel alternative and its honest
  account of the compatibility cost
- The Linux `seccomp`, `cgroups v2`, and `namespaces` manual pages, which are the actual primitives in §7.1
- The `isolate` sandbox used by the International Olympiad in Informatics, for a well-documented
  production judge sandbox and its reasoning about determinism
