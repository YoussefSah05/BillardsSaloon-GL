# Current Game Architecture — Audit for Reinforcement Learning

Status: audit before any RL implementation. Written 2026-10-06 against `main`
at the hall-lighting merge, plus the unmerged branch `feat/m9-environment`.
All numbers below were measured on the development Mac (Apple Silicon, one
core, Release build) with a throwaway probe program linked against `bs_sim`.

The question this document answers: **can the existing simulator serve as a
reinforcement-learning environment, and what is missing?** In short, it can.
The physics is already a headless, deterministic, double-precision library
that simulates one whole shot per call, about 17,000 single-ball shots per
second per core. What is missing is a thin, shot-level API exposed to Python
and the RL scaffolding around it, not a refactor of the game.

## Game engine

| Aspect | What exists |
|---|---|
| Language | C++20 (game, simulator, rules, classical AI); Python 3.12 for ML (`ml/`, uv project, on `feat/m9-environment`) |
| Framework | No engine. OpenGL 4.1, GLFW, GLM, RmlUi (UI), miniaudio (audio) |
| Build | CMake with FetchContent; static libraries `bs_sim` (physics) and `bs_game` (rules, gameplay, AI, scene data), the app on top |
| Game loop | Fixed-step update in `Application::run` (`src/app/application.cpp`), rendering separate (`render(alpha)`) |
| Tests | doctest, `tests/` (145 cases), CI on macOS, Ubuntu and Windows |

The layering already matches the target architecture in the brief:

```text
bs_sim  (physics: no GL, no game state)
   ▲
bs_game (rules, MatchSession, classical AI, Environment on the M9 branch)
   ▲                         ▲
Septhuit (app:        billiards_sim (Python module, pybind11,
 render, UI, audio)           M9 branch, built with -DBS_BUILD_PYTHON=ON)
```

## Physics architecture

The simulator (`src/sim/`, theory in `docs/design/PHYSICS.md`) is
**event-based**, not time-stepped. It is a port of pooltool's algorithm: each
ball follows closed-form motion (sliding, rolling, spinning) between events,
and the next event (ball–ball, ball–cushion, pocket, motion transition) is
found by solving polynomial equations exactly. There is no timestep, so there
is no tunnelling and no dependence on frame rate.

| Element | Where |
|---|---|
| Ball state | `Sim::BallState` (`src/sim/motion.h`): position `r`, velocity `v`, angular velocity `w` (all `glm::dvec3`), motion state `s` ∈ {Stationary, Spinning, Sliding, Rolling, Pocketed} |
| Ball parameters | `Sim::BallParams`: mass 0.170 kg, $R = 28.575$ mm, $\mu_s = 0.2$, $\mu_r = 0.01$, spinning friction, $e_b = 0.95$, $e_c = 0.85$, cushion friction 0.2 |
| Table | `Sim::Table` from `buildPocketTable(PocketTableSpec)` (`src/sim/table.h`): 2.54 × 1.27 m (9 ft), 18 linear cushion segments, 12 circular jaw tips, 6 pockets (drop circles of radius 62–64.5 mm) |
| Motion between events | `src/sim/motion.cpp`: polynomial trajectories, slide/roll/spin transition times |
| Event detection | `src/sim/events.cpp`, `src/sim/roots.cpp`: quartic ball–ball times, cushion and pocket times, a safeguarded root finder |
| Ball–ball collision | `resolveBallBall`: frictional inelastic (Alciatore), so cut-induced throw exists |
| Ball–cushion | `resolveCushion`: Han (2005) at the cushion nose height |
| Pocket detection | the ball's centre entering the pocket's drop circle (`Sim::Pocket`); event type `Pocket`, `other` = pocket index |
| Cue impulse | `resolveStrike` with `Sim::CueStrike{speed, phiDegrees, thetaDegrees, a, b}`: instantaneous point contact, squirt from the tip's end mass |
| Spin | Full 3-D angular velocity: follow/draw ($b$), side ($a$), elevation $\theta$ for swerve and massé |
| Timestep | None (event-based). One call simulates the whole shot to rest |
| Entry point | `Sim::simulateShot(table, balls, cueBall, strike, params, cue, limits)` → `ShotTrajectory` (`src/sim/simulate.h`) |

`ShotTrajectory` holds every event (type, time, balls, cushion or pocket
index) and the state of all balls after each one. `finalState()` is the table
at rest, and `stateAt(t)` is exact at any time (used for playback and
trajectory plots).

**Validation that already exists.** Golden shots against pooltool 0.6.0 agree
to 0.43 mm (`tests/test_sim_golden.cpp`, `tools/golden/`). Analytic motion
checks, root-finder and event tests are in `tests/test_sim_*.cpp`.

## State representation

There are three representations, from lowest to highest level:

1. **Simulator** (`Sim::BallState`, metres, origin at a table corner; $x$
   across the table, $y$ along it, $z$ up). This is what an RL environment
   should use.
2. **Game** (`MatchSession`, ECS registry): balls as entities in game space
   (centre of the table at the origin; game $x$ along the table = sim $y$).
   Conversions are in `src/gameplay/sim_bridge.h` (`toSimPosition`,
   `toGamePosition`).
3. **AI view** (`Ai::AiTable`, `src/ai/ai_table.h`): positions, potted flags,
   numbers, pockets, the referee's `FrameState`. On the M9 branch,
   `Ai::encodeTable` turns it into a 144-float feature vector (16 ball slots × 8
   features + 16 context values).

Everything the brief lists is available directly: cue and object ball
positions, velocities, active (not `Pocketed`) flags, pocket positions
(`table.pockets[i].center`), and, through the rules layer, the current player
and the legal balls (`Rules::legalFirstContacts`).

## Shot execution

The simulator's action is `Sim::CueStrike`:

| Field | Meaning | Range used by the game |
|---|---|---|
| `speed` | cue speed at impact, m/s (the ball leaves at about 1.5×) | 0.5 – 7.0 |
| `phiDegrees` | direction of travel in the table plane | 0 – 360 |
| `thetaDegrees` | cue elevation | 0 – 60 |
| `a` | horizontal tip offset as a fraction of $R$ (+ = left english) | ±0.525 (0.75 × 0.7) |
| `b` | vertical tip offset as a fraction of $R$ (+ = follow) | ±0.525 |

Players and the classical AI produce a `ShotInput` (aim angle in game space,
`power01`, `strikeRight01`, `strikeForward01`, elevation, call, push-out),
mapped to a `CueStrike` by `toCueStrike` (`src/gameplay/shot_input.h`): speed
$= 0.5 + 6.5 \cdot \text{power}$, tip offsets $\times 0.7$, and the sign of
side spin flipped. An RL environment can use either. Using `CueStrike`
directly avoids the game-space conversion.

## Rule system

`src/rules/` is a WPA referee made of deterministic functions, with no
rendering:

- `recordShot(trajectory, ballNumbers)` → `ShotRecord`: the first object ball
  contacted, pots in order (with pocket), whether a ball reached a cushion
  after contact, and the object balls that reached a cushion.
- `judgeShot(frameState, ballsOnTable, record)` → `Verdict`: foul type, turn
  kept or passed, frame over, winner, balls to spot. It updates `FrameState`.
- 8-ball, 9-ball and 10-ball, push-out, calls, three-foul rule, ball in hand.

For single-shot RL, the events in `ShotTrajectory` are enough: pocket events
give success, scratch and the pocket; the first `BallBall` event with the cue
ball gives the first contact. The full referee becomes relevant for
multi-shot and full-game environments.

## Rendering

Rendering (`src/render/`) reads ECS components and never drives physics. A
shot is simulated completely when the cue is released (`MatchSession`), then
**played back** visually by evaluating `stateAt(t)`. **Physics and rendering
are not coupled**: `bs_sim` and `bs_game` link no OpenGL, and the tests run
headless.

## What can already be reused for RL

| Capability the brief requires | Status | How |
|---|---|---|
| Reset the table programmatically | ✅ | Build a `std::vector<BallState>`; any layout, any number of balls |
| Set arbitrary ball positions | ✅ | Same; `MatchSession::setLayout` also exists for game-level tests |
| Apply a shot programmatically | ✅ | `simulateShot(..., CueStrike, ...)` |
| Advance simulation without rendering | ✅ | The simulator has no rendering |
| Faster than real time | ✅ | ≈17,300 single-ball shots/s on one core (10.3 events per shot); a real shot lasts several seconds |
| Detect when balls have stopped | ✅ | `simulateShot` returns at rest; `trajectory.complete` is false only if the event limit (20,000) is hit, which happened 0 times in 20,000 random shots |
| Detect pocket events | ✅ | `EventType::Pocket`, with ball and pocket index |
| Detect scratches | ✅ | The cue ball's final state is `Pocketed` |
| Detect legal first contact | ✅ | The first `BallBall` event involving ball 0; or `recordShot` |
| Determinism | ✅ | Same inputs, bitwise-identical final state (checked) |
| Python access | 🟡 M9 branch | `billiards_sim` exposes a whole-frame `Environment`, not single shots |
| Geometry baseline | 🟡 | `src/ai/planner.cpp` has ghost-ball candidates (pot lines, ease, blocking), but inside the full planner |
| Full-game environment | 🟡 M9 branch | `Ai::Environment`: rack, decisions (break, place, shot, choice), referee, classical opponent |
| Strong classical opponent | ✅ | Two-stage search under execution noise, five profiles (`docs/design/AI.md`) |
| Value-network inference in C++ | 🟡 M9 branch | ONNX Runtime `ValueModel`; trained model is weak (see below) |

## What must be decoupled

Very little. The simulator is already decoupled from rendering and from game
state. Two narrower decouplings are needed:

1. **A shot-level API independent of `MatchSession`.** The M9 `Environment`
   plays a full frame through `MatchSession`, which owns an ECS registry,
   rules state and racking. That is right for full games but heavy for one
   shot on a custom layout. A single-shot environment should call
   `simulateShot` directly.
2. **The geometry helpers in `planner.cpp`.** `potLine` and the ghost-ball
   maths live in an anonymous namespace inside the planner. A baseline needs
   them as a small public function: which pocket, which ghost point, which cut
   angle.

## Missing RL capabilities

- A Python binding for **single shots and batches of shots**: arbitrary
  layout in, outcome and final state out, with the GIL released so batches
  can use all cores.
- A **Gymnasium** environment (`reset`/`step`, `observation_space`,
  `action_space`, seeding), with reward definitions kept separate from
  environment mechanics.
- **Layout generators** with seeds (single ball, difficulty bands) and fixed,
  versioned **evaluation suites** (`easy/medium/hard`).
- **Baselines**: random policy and a geometry solver aimed at the pocket
  mouth.
- **Training scaffolding**: config files, seeds, checkpoints, a local
  experiment log, evaluation scripts. `gymnasium` and `stable-baselines3` are
  not yet dependencies of `ml/`.
- **Environment tests** in Python (`pytest`): shapes, bounds, NaNs,
  determinism, reward consistency.

## Risks

**1. The success region is very narrow (measured).** Into the intended corner
pocket, cleanly (no cushion first, no scratch), an object ball 0.6 m from the
pocket gives these windows of the aim angle $\varphi$:

| Shot | Width of the potting window in $\varphi$ |
|---|---|
| Straight, cue ball 0.5 m behind | 0.62° |
| Straight, cue ball 1.2 m behind | 0.21° |
| 30° cut, cue ball 0.8 m away | 0.26° |
| 60° cut, cue ball 0.8 m away | none within ±1° of the ghost line |

A uniformly random action ($\varphi \in [0°, 360°)$, speed 0.5–7 m/s) pots
the 30° cut **0.19%** of the time. Over random single-ball layouts, random
shots pot the object ball (anywhere) 2.3% of the time and scratch 21.7% of the
time. This is the sparse, needle-like reward the brief anticipates. It
justifies HER, curricula, elite-sample methods, and candidate-relative action
parameterisations, and it means an untuned SAC on raw angles is likely to
fail at first.

**2. Aiming at the drop circle is wrong.** For the straight shot above, the
potting window ($-0.68°$ to $-0.06°$ from the line through the pocket's
centre) does not contain that line: the line to the drop circle's centre
clips the jaw. A geometry baseline must aim at the pocket's **mouth** (the
midpoint between the jaw tips), not at `Pocket::center`. The classical AI
aims at the drop circle today, which is a probable cause of avoidable misses.

**3. Throw shifts the window.** The frictional collision model throws the
object ball off the ghost-ball line (the 30° cut's window is off-centre, and
60° cuts need more than 1° of correction). Good: it is physically real. But a
pure geometry baseline will underperform on cuts, and RL has something real to
learn there.

**4. The action space has a discontinuous, multimodal reward.** Several
disjoint aim windows can pot a ball (direct, off the cushions, the other side
of a jaw). Gaussian policies (SAC/TD3/PPO) may settle on one and must not be
evaluated only on mean reward.

**5. No airborne motion.** The strike discards vertical velocity, so there are
no jump shots and balls never leave the table. For pocketing and position
play this does not matter. Elevated shots (swerve, massé) are modelled as
planar curves.

**6. Floating-point determinism across platforms.** Bitwise determinism holds
on one machine and build. Across compilers or `-ffast-math` it may not. Keep
evaluation suites tolerant (success is binary per state) and record the
platform with every result.

**7. Pocket model simplicity.** A ball drops when its centre enters a drop
circle. There are no rattles inside the pocket, no pocket-back geometry and no
rail-height bounce. That is accurate enough for learning and for the game, but
results should be described as being "in this simulator".

**8. Python-call overhead will dominate.** At 17k shots/s in C++, a
Python–C++ round trip per step plus Stable-Baselines3's own overhead will
limit training throughput, not physics. Batch and vectorised APIs matter more
than optimising the simulator.

**9. The existing learned value network is weak.** On the M9 branch, a value
network trained on 130k self-play positions reaches a validation log loss of
0.682, against 0.685 for logistic regression and 0.692 for the base rate.
Frame outcomes between equal players are noisy targets. This is consistent
with the brief's advice to start from shot execution, not game outcomes.

**10. Repository hygiene.** A compiled Python cache file
(`tools/golden/__pycache__/generate_golden_shots.cpython-312.pyc`) is
committed, and `.gitignore` on `main` has no Python entries (the M9 branch
adds them).
