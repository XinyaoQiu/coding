# Chapter A1 — Online Multiplayer Game (Chess)

> **Prerequisites:** Chapters 02 (§8 push protocols), 03 (logs and replay), 30 (connection fleets), 83 (deterministic state machines)
> **Patterns:** server-authoritative state, the clock as game state, matchmaking under a widening window, reconnection, deterministic replay

---

## 1. The problem

Two players are matched, play a game of chess against each other in real time under a time control, and
one of them wins.

Chess is a good vehicle for this question because it strips away the thing that usually dominates
multiplayer game design — high-frequency state synchronization — and leaves the parts that generalize. The
state is tiny, moves are infrequent, and the rules are unambiguous. What remains is: who decides what is
true, how do you pair strangers fairly and quickly, what happens when someone's connection drops, and —
the part almost everyone underestimates — **how do you run a clock that two people in different countries
both consider fair?**

The clock is the interesting problem. A chess clock is not a display; it is game state that determines
outcomes. When a player has four seconds left and their move takes 300 milliseconds to reach the server,
someone must absorb that 300 milliseconds, and every possible answer is unfair to someone.

**The property that makes it hard:** the authoritative state must live on the server because clients lie,
but the *time* component of that state is affected by network latency that the server cannot observe
directly and cannot eliminate. Every other design decision here is comparatively mechanical; the clock is
where the judgment is.

---

## 2. Requirements

### Functional

1. Match two players of similar skill into a game.
2. Relay and validate moves in real time, enforcing the rules of chess.
3. Enforce a time control and terminate the game on checkmate, resignation, draw, or flag.

Defer, but name: spectating, tournaments, puzzles and analysis, chat, and variant rules.

### Non-functional

- **Move round-trip latency** — p99 under 150 ms from move submission to the opponent seeing it. Chess
  tolerates more latency than an action game, but in bullet formats (one minute per side) it is felt
  acutely.
- **Clock accuracy** — the server's accounting must be correct to within a few tens of milliseconds and
  must be *explicable*. Players lose games on time and will dispute it.
- **Matchmaking latency** — p50 under 10 seconds. A player staring at a spinner leaves.
- **Correctness** — an illegal move must never be accepted, and a completed game's result must never
  change. Both are absolute.
- **Availability** — a server failure must not lose a game in progress. Games last minutes and abandoning
  them is a serious product failure.
- **Concurrent games** — assume 100,000 simultaneous games; §3.
- **Consistency** — both players must see an identical game state. There is no acceptable eventual
  consistency for the board; there is for the lobby and the rating.

### Explicitly out of scope

Anti-cheat modeling (§7.6 covers why it is a detection problem, not a prevention one, but the model itself
is out of scope), payment, and social features.

---

## 3. Estimation

Assume 100,000 concurrent games — 200,000 connected players.

**Connections — the constraining number**

```
200,000 concurrent WebSocket connections
÷ ~50,000 per game server (lower than Chapter 30's 100k, because these
  connections carry game state and logic, not just relay)
= ~4 game servers, plus redundancy
```

Four machines. The system is small, and saying so matters: this is not a scale problem, it is a
correctness and latency problem, and a candidate who spends the interview sharding is answering the wrong
question.

**Message rate**

```
average game: 40 moves, ~5 minutes
moves/sec across the platform = 100,000 games × 40 moves / 300 s ≈ 13,300 moves/sec
plus clock sync, presence, and draw offers: call it 30,000 msg/sec
```

Thirty thousand messages per second across four servers. Trivial per machine. **Compare this to Chapter 82,
where one auction generated 100,000 messages per second by itself** — a game has exactly two recipients per
message, so there is no fanout problem at all. That absence is what makes the connection count, not the
message rate, the sizing input.

**State per game**

```
board (64 squares, packed)                    ~32 B
clocks, side to move, castling, en passant    ~32 B
move history (40 moves × 4 B)                 ~160 B
metadata                                      ~200 B
                                             ───────
                                             ~500 B/game
100,000 games × 500 B                        = 50 MB
```

**Fifty megabytes for every game in progress on the platform.** The entire live state fits in the memory of
one machine several times over. This single number licenses the whole architecture in §6: keep games in
memory, own each in a single process, and treat durability as a log rather than as a database of record.

**Matchmaking**

```
players entering the queue at peak ≈ 500/sec
pool size at any instant           ≈ 5,000 waiting
```

Small enough that the matching pass can be a scan over an in-memory structure rather than anything clever.

---

## 4. API

```
// WebSocket, per player
→ join_queue    {timeControl, ratingHint}
← matched       {gameId, color, opponent, timeControl, serverTime}

→ move          {gameId, moveSeq, uci: "e2e4", clientSentAt}
← move_applied  {moveSeq, uci, whiteMs, blackMs, serverTime, positionHash}
← illegal       {moveSeq, reason, positionHash}

→ resign / offer_draw / accept_draw
← game_over     {result, reason, ratingDelta}

← clock_sync    {serverTime}        // periodic, unsolicited
→ pong          {clientTime, serverTime}   // RTT measurement, §7.2
```

Four decisions:

**Every server message carries `serverTime`, and every move response carries both clocks.** The client
never computes remaining time from its own accounting; it renders what the server last told it,
interpolating locally between updates. A client that tracks its own clock will drift, and the moment it
disagrees with the server is the moment a player believes they were robbed.

**`moveSeq` is a monotonic per-game counter supplied by the client and validated by the server.** It makes
moves idempotent under retry and lets the server reject a move computed against a stale position — if the
client's sequence does not match the server's expectation, the client's view is behind and the move is
refused rather than misapplied.

**`positionHash` is returned on every move.** The client compares it against its own computed position. A
mismatch means the two views have diverged — from a dropped message, a bug, or tampering — and the client
resynchronizes from the server. This is cheap insurance against the worst failure in a game system, which
is two players looking at different boards and neither knowing.

**WebSocket, not SSE.** Unlike Chapters 82 and 31, the client sends as often as the server does. This is
the case WebSocket is for. (Chapter 02, §8.)

---

## 5. Data model

```
In memory, in the process owning the game:

game
  game_id, white_id, black_id
  position          -- bitboards or a mailbox array
  side_to_move
  castling_rights, en_passant_square, halfmove_clock
  white_ms, black_ms          -- remaining, in milliseconds
  last_move_at_ms             -- server monotonic clock reading
  increment_ms
  move_history[]
  status

Durable:

games                  game_id, players, time_control, result, pgn, started_at, ended_at
game_events            game_id, seq, event_type, payload, server_ts   -- append-only
players                player_id, rating, rating_deviation, games_played
```

Three deliberate decisions:

**The clock is stored as `remaining_ms` plus `last_move_at_ms`, not as a running countdown.** Nothing
decrements on a timer. The elapsed time is computed on demand as `now - last_move_at_ms`, and only when a
move arrives or a flag check runs. A design that decrements clocks on a tick has 100,000 timers running
and accumulates drift; this design has none and is exact. It is a small representational choice with a
large consequence, and §7.2 depends on it.

**`last_move_at_ms` reads a monotonic clock, not the wall clock.** Wall clocks step (Chapter 11, §7.2), and
a clock that steps backward mid-game would credit a player with time they did not have — or flag them
instantly.

**`game_events` is append-only and complete**, including illegal-move attempts and disconnections. It is
the recovery log (§7.4), the replay source for analysis, and the evidence for a cheating investigation.
The in-memory game is a materialization of it, exactly as in Chapters 82 and 83.

---

## 6. Architecture, derived

### Attempt 1: peer-to-peer, clients agree between themselves

Players connect directly; each validates the other's moves.

Fails immediately on trust. A modified client can claim an illegal move, claim its opponent flagged, or
simply assert a win. There is no arbiter, and in a competitive game with ratings there must be one.

It also fails on NAT traversal and on the fact that neither peer can be trusted to record the result. Worth
one sentence, because "the server is authoritative" is the foundational decision and it deserves to be
derived rather than assumed.

### Attempt 2: stateless servers with the game in a database

Every move: load the game from the database, validate, apply, write back, notify the opponent.

Correct and slow. Each move costs at least two database round trips plus a lookup to find the opponent's
connection.

```
move latency = client→server (30 ms) + DB read (5 ms) + DB write (5 ms)
             + opponent lookup (2 ms) + server→opponent (30 ms) ≈ 72 ms
```

Within budget, but the database is now handling 13,300 read-modify-write cycles per second on rows that
are contended between exactly two clients, and every one of those cycles is a transaction against a row
that a moment ago was in memory. It is a lot of machinery to protect state that is 500 bytes and has two
writers who alternate strictly.

More importantly, it makes the *clock* awkward: computing remaining time requires reading the last move
timestamp from the database, so a flag check is a query, and checking 100,000 games for flags means
polling the database continuously.

### Attempt 3: the game lives in memory, in one process

Each game is owned by one process. Moves are applied in memory. The opponent's connection is on the same
process (or reachable through it), so notification is a local write to a socket.

```
move latency = client→server (30 ms) + validate + apply (<1 ms)
             + append to log (async) + server→opponent (30 ms) ≈ 61 ms
```

The state is 500 bytes and the two participants are known, so there is nothing to coordinate. Moves for one
game are processed sequentially by construction, which means no locking and a natural total order — the
same single-owner serialization as Chapters 82 and 83, arrived at from a third direction.

**Route both players of a game to the same server.** This is what makes the opponent notification a local
operation rather than a cross-server hop, and it halves the tail latency. The matchmaker assigns the game
to a server and directs both clients there.

### Attempt 4: durability by event log

In-memory state is lost when a process dies, and abandoning games is unacceptable.

Append every event — move, clock state, connection change — to a durable log before acknowledging. On
failure, a replacement process replays the log for its games and resumes. The log is small (a few hundred
bytes per game) and append-only, so this costs a few milliseconds off the critical path and can be batched
across games.

Because move application is a deterministic function of `(position, move)`, replay reconstructs the exact
state. The clock is reconstructed from the recorded timestamps rather than recomputed from wall time, which
is why `game_events` records the clock values rather than only the moves.

### Attempt 5: separate matchmaking from game serving

Matchmaking is a global, stateful, batch-flavored problem: it needs to see the whole pool. Game serving is
per-game and latency-sensitive. Running them in the same process means a matchmaking pass can add jitter to
move handling.

Split them. The matchmaker holds the queue, runs a pass every few hundred milliseconds, and hands formed
games to game servers.

### Final architecture

```
  Client ──WebSocket──┐
                      ▼
            ┌── Gateway / LB (sticky by gameId) ──┐
            └──────────┬──────────────────────────┘
                       │
      ┌────────────────┼─────────────────┐
      ▼                                  ▼
  Matchmaker                      Game server (4 nodes)
  ┌──────────────────┐            ┌──────────────────────────┐
  │ pool by time     │            │ 25,000 games in memory   │
  │ control, indexed │  assign    │ per node (~12 MB)        │
  │ by rating        │──────────► │                          │
  │ widening window  │            │ per game: single owner,  │
  │ (§7.3)           │            │ sequential move handling │
  └──────────────────┘            │           │              │
                                  │           ├─► both players' sockets
                                  │           │   (co-located)
                                  │           ▼
                                  │  append to event log ────┼──► durable log
                                  │  flag checker (timer     │    (replay on
                                  │   wheel, §7.2)           │     failover)
                                  └──────────┬───────────────┘
                                             ▼
                                    on game end: Postgres
                                    (result, PGN, rating update)
```

---

## 7. Deep dives

### 7.1 Server-authoritative state, and what the client is allowed to do

The server holds the position and validates every move against the rules. The client's move is a
*request*, never an assertion.

The rules that follow, and the reasons:

**Validate everything, including things the client's UI already prevents.** A modified client does not run
your UI. Legality, turn order, and game status are all checked server-side, every time.

**The client may predict, but must reconcile.** Rendering the move immediately on the local board makes the
game feel responsive; the client must be prepared for the server to reject it and roll back. In chess this
is nearly always safe because the client can validate legality itself — it has full information. **This is
the crucial difference from an action game**, where the server's world state is not fully known to the
client and mispredictions are frequent and visible. Chess's full information is what makes optimistic local
application almost always correct, and it is worth saying because it explains why chess needs none of the
lag-compensation machinery a shooter does.

**Never send information the client should not have.** In chess this is nearly vacuous — the position is
public — which is exactly why it is worth flagging as a design habit that a variant with hidden
information (fog of war, card games) would require. Sending the full state and hiding it in the UI is the
classic exploitable mistake.

**The `positionHash` in every response** (§4) is the cheap detector for divergence, whatever its cause.

### 7.2 The clock, which is game state

The part most candidates skip, and the part that decides games.

**The representation** (§5): store `remaining_ms` and `last_move_at_ms`; compute elapsed on demand. No
per-game timer, no drift, exact arithmetic.

```
on move from the player to move:
    now      = monotonic_now()
    elapsed  = now - last_move_at_ms
    clock[mover] -= elapsed
    if clock[mover] <= 0: flag(mover); return
    clock[mover] += increment_ms
    apply(move)
    last_move_at_ms = now
```

**Flag detection when nobody moves.** A player who runs out of time while thinking must lose without either
client doing anything. Polling 100,000 games is wasteful; instead register each game in a **timer wheel**
(Chapter 92) keyed on the instant the side to move would flag, and reschedule on every move. One timer
entry per game, updated twice per move, and no scanning.

**Now the hard part: who pays for network latency?** A player presses at 4.0 seconds remaining and the
move arrives at the server 150 ms later. Three policies:

1. **Charge the sender everything.** Server timestamps on receipt. Simple, and it systematically penalizes
   players with worse connections. In a bullet game a 200 ms link costs several seconds over a game.
2. **Charge the sender nothing.** Trust a client-supplied `clientSentAt`. Trivially exploitable — a
   modified client claims every move took zero time and never flags.
3. **Compensate using measured round-trip time (the choice).** The server continuously measures RTT via
   ping/pong. On receiving a move, it credits back up to `min(measured_rtt / 2, cap)` milliseconds. The
   cap — a few hundred milliseconds — bounds the exploit: a player who artificially inflates their measured
   RTT gains at most the cap, and inflating RTT also delays their opponent's moves reaching them, which
   costs more than it gains.

This is what real chess servers do, and the reason to describe the mechanism rather than the intent is that
the cap is the part that makes it safe. Compensation without a cap is option 2 wearing a disguise.

**The server's decision is final and must be explicable.** Record the RTT measurement and the compensation
applied in the event log, so a disputed flag can be explained rather than merely asserted.

**Displaying the clock** is a separate problem. The client interpolates locally between server updates and
snaps to the server's value on each move. It should be *pessimistic* — round the player's own remaining
time down and the opponent's up — so that the client never shows more time than the player has. Optimistic
rounding produces the worst possible experience: a clock that reads 0.4 seconds when the server has already
flagged you.

### 7.3 Matchmaking under a widening window

Pair players of similar rating, quickly. These conflict directly: a narrow rating window means a good match
and a long wait, and a wide one means the reverse.

**The standard solution is a window that widens with waiting time:**

```
acceptable_delta(t) = base + rate × t
  base = 50 rating points
  rate = 25 points/second

  t=0s   → ±50     (a near-equal opponent, if one is available)
  t=10s  → ±300
  t=30s  → ±800    (essentially anyone)
```

A player who can be matched well is matched immediately; one in a sparse rating band waits, and the wait
buys progressively worse matches. The parameters are the product's decision about how much match quality a
second of waiting is worth, and they should be tuned against measured pool density per band, not chosen
once.

**Run it as a periodic pass, not per-arrival.** Every ~250 ms, sweep the pool sorted by rating and pair
adjacent compatible players. This finds better global pairings than greedily matching each arriving player
against the current pool, and it is what makes the widening window coherent — a batch can see that two
waiting players are each other's best option.

**Symmetry matters.** Both players must find each other acceptable. With a widening window, the longer-
waiting player has the wider tolerance, so the pairing condition is the *intersection*, and using only one
player's window creates matches that one side would have rejected.

**Partition the pool by time control**, and only by time control. A blitz player and a classical player are
never candidates for each other, so these are independent pools — which conveniently makes each pool small
enough to scan.

**Rating itself** should carry an uncertainty (Glicko's rating deviation, or a Bayesian equivalent) rather
than being a single number. A new player's rating is unknown, so the window around it should be wide and
their result should move it a lot; an established player's should be narrow and stable. Using a bare Elo
number means new players are matched confidently against a rating that means nothing yet, and their first
several games are mismatches in one direction or the other.

### 7.4 Reconnection, abandonment, and the resume path

Mobile clients disconnect constantly. A dropped connection must not lose a game.

**Disconnection does not pause the clock.** This is a rule, not an implementation detail: pausing on
disconnect creates an obvious exploit — a player in a losing position with low time simply disconnects.
The clock runs, and a player who cannot reconnect before flagging loses on time. Harsh and correct.

**Resume is a state resynchronization, not a replay.** On reconnect the client sends its `gameId` and last
known `moveSeq`; the server responds with the full current position, both clocks, and any moves the client
missed. Sending the full position rather than a delta is 500 bytes and removes an entire class of bugs
where a client applies a delta to a state that was already wrong.

**Route the reconnecting client to the server owning the game.** A lookup from `gameId` to the owning
server, held in a shared registry, exactly as Chapter 30 maps users to connection servers.

**Abandonment is a product decision that needs a technical mechanism.** If a player disconnects and does
not return, the opponent should not have to wait out the full clock in a won position. The usual rule is
that after a disconnection lasting some threshold, the opponent may claim a win. Implement as a timer
armed on disconnect and disarmed on reconnect — which means disconnection is an *event in the game log*,
not merely a socket state, and that is the design consequence worth naming.

**Failover** reuses the same machinery. When a game server dies, its games are reassigned, the replacement
replays each game's event log, and both clients reconnect through the registry and resynchronize. Because
the reconnect path already exists for flaky networks, failover requires almost no additional code — which
is the payoff for treating reconnection as normal rather than exceptional.

### 7.5 Turn-based versus real-time, which is where this generalizes

Chess is turn-based, and almost everything above depends on that. The comparison is the most transferable
content in the chapter.

| | Turn-based (chess) | Real-time (action game) |
|---|---|---|
| Update rate | ~0.1 msg/sec/player | 20–60 ticks/sec/player |
| Messages/sec at 200k players | ~13,000 | ~10,000,000 |
| State size | 500 B | KB to MB per session |
| Server model | Event-driven, idle between moves | Fixed-rate simulation loop |
| Client prediction | Optional; full information makes it nearly always right | Mandatory, and mispredictions are constant |
| Reconciliation | Rare; send the whole state | Continuous rollback and replay of local input |
| Latency compensation | Clock credit (§7.2) | Lag compensation: server rewinds world state to when the client fired |
| Transport | WebSocket over TCP — ordering matters, loss is intolerable | UDP — a dropped position update is superseded by the next one; retransmitting it is worse than losing it |
| Bandwidth per player | Bytes/sec | 10–100 KB/sec |

**The TCP/UDP line is the crispest illustration.** In chess, every move must arrive and must arrive in
order, so TCP's retransmission is exactly right. In an action game, a position update that is 100 ms late
is worthless — the next one already superseded it — so TCP's insistence on delivering it in order
*head-of-line blocks* the useful data behind the useless. That is why real-time games use UDP and rebuild
whatever reliability they need selectively.

**The message-rate row is the other one to notice**: three orders of magnitude, from the same player count.
A candidate who says "a game server holds 50,000 connections" without distinguishing these two regimes is
quoting a number that is right for one and off by a thousand for the other.

### 7.6 Cheating: detection, not prevention

In chess, cheating is running an engine. The server cannot prevent it — the position is public information
and a player can type it into a program on another device. **There is no technical prevention, and claiming
otherwise is the wrong answer.**

What the system can do:

**Make detection possible by design.** Record per-move timing, the full move list, and the client's
reported state in the event log. Engine-assisted play has statistical signatures: move quality that
correlates suspiciously well with a strong engine's top choice, and timing that does not vary with position
complexity the way human thinking does. A human spends longer on hard positions; someone reading an engine
output does not.

**Run detection offline, in batch, over completed games.** It requires engine analysis of every position,
which is far too expensive to do inline, and it benefits from seeing a player's full history rather than
one game.

**Treat it as a classification problem with asymmetric costs.** A false positive bans an innocent player,
which is a serious harm; a false negative lets a cheater continue, which is a lesser one. Set the threshold
accordingly, require human review above it, and provide an appeals path. The interesting engineering
statement is not the model — it is that the *logging must be designed for it in advance*, because you
cannot retroactively analyze timing data you did not record.

**Do not put anti-cheat logic on the critical path.** It is expensive, uncertain, and its errors are
costly; the move handler must not wait for it.

---

## 8. Failure modes and operations

| Failure | Effect | Mitigation |
|---|---|---|
| Game server crashes | 25,000 games interrupted | Reassign; replay event logs; clients reconnect through the registry and resynchronize — the same path as a dropped connection |
| Event log write fails | Cannot durably record a move | Reject the move rather than acknowledge it; an acknowledged move that vanishes is worse than a rejected one |
| Client-server position divergence | Two players see different boards | `positionHash` on every response; client resynchronizes on mismatch |
| Both players disconnect | Game hangs consuming memory | Abandonment timer; adjudicate or abort after a threshold |
| Clock steps (NTP) | Clock accounting corrupted mid-game | Monotonic clock only for elapsed-time computation (Chapter 11, §7.2) |
| Matchmaker down | No new games; existing games unaffected | Deliberate isolation — the two are separate services for exactly this reason |
| Rating update fails after game end | Result recorded, rating stale | Outbox pattern (Chapter 01, §9); the game result is the source of truth and rating is derived |
| Thundering reconnect after a network blip | Thousands of resync requests at once | Jittered client reconnect backoff; resync is 500 bytes so the load is bounded |

**Monitoring:** move round-trip p99 measured client-side, not server-side (the server cannot see its own
network latency); position-hash mismatch rate, which should be zero and whose non-zero value is always a
bug worth investigating; flag rate split by measured RTT band, which is the direct test of whether §7.2's
compensation is fair; matchmaking wait-time distribution and average rating delta of formed matches (the
two halves of the §7.3 trade-off, which must be read together); disconnection and successful-resume rates;
and abandoned-game rate.

---

## 9. Common mistakes

1. **Trusting the client** for move legality, game result, or elapsed time. Everything a client sends is a
   request.
2. **Treating the clock as a display concern.** It is game state, it decides outcomes, and the latency
   question has no free answer.
3. **Running a timer per game to decrement clocks.** Store `remaining + last_move_at` and compute on
   demand; use a timer wheel for flag detection.
4. **Using the wall clock** for elapsed time, so an NTP correction awards or destroys time.
5. **Pausing the clock on disconnect**, which is an obvious and immediately exploited resignation escape.
6. **Uncapped latency compensation**, which is equivalent to trusting client timestamps.
7. **A fixed matchmaking window**, which either makes players wait indefinitely in sparse rating bands or
   produces bad matches for everyone.
8. **Using a single rating number** with no uncertainty, so new players are matched confidently against a
   number that means nothing.
9. **Not designing the event log for cheat detection**, and discovering after launch that the timing data
   needed to detect engine use was never recorded.
10. **Quoting real-time game architecture for a turn-based game** — the tick loop, UDP, rollback
    netcode — none of which chess needs, and all of which signals pattern-matching rather than analysis.

---

## 10. Variants

**Real-time action game.** The §7.5 table is the summary. The architecture becomes a fixed-rate simulation
loop over UDP with client-side prediction, server reconciliation, and lag compensation that rewinds the
authoritative world state to the moment the client acted. Three orders of magnitude more traffic, a
completely different transport, and the same server-authoritative principle.

**Asynchronous / correspondence chess.** Moves arrive days apart. Nothing stays in memory, there is no
connection to hold, and the entire design collapses into a database with notifications (Chapter 32). Worth
mentioning because it shows the in-memory design was bought by the real-time requirement, not by the game.

**Games with hidden information (poker, card games).** The server must send each client only what that
client may see, which makes the "never send information the client should not have" rule from §7.1 load-
bearing rather than vacuous. Adds a need for verifiable randomness — players must be able to trust the
shuffle — which is a genuinely different problem with cryptographic answers.

**Massively multiplayer worlds.** Thousands of players in one shared space, so the two-participant
simplicity vanishes and interest management (sending each client only nearby entities) becomes the central
problem — which is Chapter 50's spatial indexing applied to a simulation.

**Tournaments.** Adds scheduling, pairing across many rounds, and simultaneous starts for thousands of
games — Chapter 92's scheduled-simultaneity problem layered on this chapter's game serving.

---

## 11. Further reading

- Chapters 82 and 83, for single-owner serialization at other latency scales; Chapter 30, for connection
  fleets and the user-to-server registry; Chapter 92, for the timer wheel
- Glickman, "The Glicko System" — for why a rating needs an uncertainty and how §7.3's window should use it
- Valve's Source Engine networking documentation on prediction, interpolation, and lag compensation — the
  clearest public explanation of the real-time column in §7.5
- Gaffer On Games, "Networked Physics" series, for the same material developed from first principles
