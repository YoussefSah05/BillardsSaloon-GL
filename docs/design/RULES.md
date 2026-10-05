# Rules and referee

Billiards Saloon referees by the WPA World Standardized Rules for 8-ball,
9-ball and 10-ball. This page lists what the referee enforces, where the game
simplifies a rule, and where each piece lives in the code. Last revised
2026-10-06.

## Where it lives

| Piece | Code | Tests |
|-------|------|-------|
| Referee: frame state, verdicts, choices | `src/rules/referee.*` (deterministic functions, no ECS or GL) | `tests/test_rules_eight_ball.cpp`, `tests/test_rules_rotation.cpp` |
| What happened on a shot, from the simulator's events | `src/rules/shot_record.*` | `tests/test_shot_record.cpp` |
| WPA racking | `src/rules/racking.*` | `tests/test_racking.cpp` |
| Race to N | `src/rules/match_score.h` | `tests/test_rules_rotation.cpp` |
| Ball in hand, spotting, choices, calls, push-out in play | `MatchSession` | `tests/test_match_rules.cpp` |

A shot flows like this: the simulator computes the whole shot when the cue
is released; `recordShot` reads first contact, cushion contacts and pockets
(with which pocket) from its events; when the balls stop, `judgeShot` returns
a `Verdict` (foul, turn, spotted balls, frame end, choice) and updates the
`FrameState`; the session applies it (spots balls, gives ball in hand) and the
HUD announces it.

## Design: the referee as a transition function

A frame of pool is a state machine. Its state is small: the phase (break,
open table, groups assigned), who shoots, each player's group, consecutive
fouls, ball in hand and where, a pending choice (push-out, re-rack, pass back),
and the result. That is `FrameState`. A shot moves it forward through one
deterministic function (in the code it updates the state in place and
returns the verdict):

$$
\texttt{judgeShot} : (F,\ B,\ S) \longmapsto (F',\ \text{Verdict}),
$$

where $F$ is the frame state before the shot, $B$ the object balls on the
table, and $S$ the **shot record**: what physically happened. The record holds
the first ball contacted, whether any ball reached a cushion after contact,
the balls pocketed and their pockets in order, and the player's call or
push-out. The function reads nothing else: no globals, no clock, no
randomness.

**Why no hidden inputs.** The same referee serves the game, the tests, the AI
and self-play training:

- **Testability.** Each WPA clause becomes a table-driven test: a frame state,
  a scripted shot record, and the expected verdict. No rendering or physics
  needed.
- **Planning.** The AI judges hundreds of simulated outcomes per decision
  ([AI.md](AI.md)). The referee must be cheap and side-effect free, so it can
  be applied to hypothetical shots.
- **Replays and determinism.** Given the start state and the shot, the verdict
  is reproducible, like the trajectory.

**Why read the simulator's events.** Fouls are defined by the order of
physical events ("the first ball contacted", "a ball reaching a cushion
after contact"). An event-based simulator ([PHYSICS.md](PHYSICS.md)) produces
exactly that ordered list with exact times, so the record is extracted, not
estimated from frames of animation. A thin hit that a frame-stepped engine
could miss is still a contact here, and two contacts microseconds apart are
still ordered.

**Choices are states, not dialogs.** A push-out, an illegal 8-ball break or an
uncalled pot in 10-ball gives the *other* player a decision. The verdict sets
a pending choice and its chooser in $F'$, and a second function
(`applyChoice`) resolves it. The HUD, the AI's `planChoice` and the tests all
drive the same transition.

**Three fouls** need memory, which is why $F$ counts consecutive fouls per
player. A legal shot resets the shooter's count, and a third foul in a row
ends the frame in rotation games. The shot clock fits the same model: running
out of time is a foul with an empty shot record (`judgeTimeFoul`).

## Common to every game

- **Fouls:** cue ball pocketed; no ball hit; wrong ball hit first; no ball
  pocketed and no ball reaching a cushion after contact.
- **Ball in hand** after a foul, anywhere on the table, placed with an overlap
  check. Placement stays available until the shot, and the break is played from
  behind the head string.
- **Spotting:** a ball comes back on the foot spot, or as close behind it on the
  long string as fits, else in front of it.
- **Racking** is random apart from the WPA positions (see each game).
- **Match:** race to N frames; alternate break (default) or winner breaks.
- **Shot clock** (optional, as in professional events): 30, 45 or 60 s from
  the shot after the break, one 30 s extension per player per frame. Running
  out is a foul (`judgeTimeFoul`), with ball in hand for the other player.

## 8-ball

- Rack: 8 in the middle, a solid and a stripe on the back corners.
- Break: legal with a ball pocketed or four object balls to a cushion. On an
  illegal break the incoming player chooses: play from here, re-rack and break,
  or have the offender break again.
- Scratch on a legal break: ball in hand behind the head string.
- 8 on the break: spotted; the breaker (or, after a scratch, the incoming
  player) plays on or re-racks.
- The table is always open after the break. On an open table either group may
  be hit first; the first ball pocketed decides the groups.
- The shooter continues after pocketing a ball of their group.
- The 8 must be called (pocket chosen automatically from the aim, changeable
  with Q/E). The frame is lost by pocketing the 8 before the group is cleared,
  on a foul, or in a pocket that was not called.
- No three-foul rule in 8-ball.

**Simplifications:** ordinary balls are not called (any ball of the group
counts, as in "obvious ball" play); on an open table the first ball down
decides the groups where WPA uses the called ball; hitting the 8 first on an
open table is a foul.

## 9-ball

- Rack: diamond, 1 at the apex, 9 in the middle.
- Break: the 1 first and a ball pocketed or four balls to a cushion, otherwise
  a foul. The 9 on a legal break wins; on a foul it is spotted.
- Push-out: the player taking the shot after a legal break may push out (no
  contact or cushion needed; a scratch is still a foul). The opponent then
  plays from there or hands the shot back.
- Lowest ball first; any ball pocketed on a legal shot keeps the turn; the 9
  on any legal shot wins (combinations included); the 9 on a foul is spotted.
- Three fouls in a row lose the frame. The scorebug shows the count, and the
  referee warns a player who is on two.

## 10-ball

- Rack: triangle, 1 at the apex, 10 in the middle, 2 and 3 on the back corners.
- Break as in 9-ball; the 10 on the break is spotted and the breaker continues.
- Every shot after the break is called (ball and pocket; both follow the aim
  until changed with Q/E for the pocket and Z for the ball). Push-outs need no
  call.
- A ball down but not the called ball in the called pocket: the turn passes
  and the incoming player may hand the shot back.
- The 10 wins only when called; otherwise it is spotted.
- Push-out and three fouls as in 9-ball.

## Not yet covered

Jump and massé shots (and their fouls), cue ball off the table, double hits,
"object balls behind the head string" after a
break scratch, and concessions. These come with cue elevation (shot input) and
the shot clock (presentation).
