# Reinforcement Learning System Design

Status: design, before implementation. Written 2026-10-06. Read
[current-game-audit.md](current-game-audit.md) first: every decision here is
grounded in what the simulator actually provides and in the measurements
recorded there.

**Research question.** How can reinforcement learning learn both
high-precision continuous shot execution and strategic multi-shot
decision-making in billiards?

**Working title.** *Learning Precision and Strategy in Billiards: Continuous
Control, Goal-Conditioned Reinforcement Learning and Planning.*

## 1. Principles

1. **One physics.** The game, the RL environments, the baselines and the
   evaluation all call the same C++ simulator (`bs_sim`). Python never
   reimplements physics. No GUI automation.
2. **Two layers, separated from the start.** Shot *execution* (continuous
   control: where and how hard to hit) and shot *selection* (strategy: which
   ball, which pocket, where the cue ball should go) are different problems.
   They get separate interfaces now, even while only execution is learned.
3. **Small first.** One cue ball, one object ball, one shot per episode.
   Complexity is added only after the simpler task is measured and understood.
4. **Baselines before claims.** Every learned policy is compared, on the same
   fixed evaluation states, with a random policy, a geometry solver, and a
   simulator-search upper bound.
5. **Reproducible by construction.** Configs, seeds, environment versions,
   git commits and platforms are recorded with every result.

## 2. Architecture

```text
                         ┌──────────────────────────────────┐
                         │ bs_sim (C++): simulateShot        │  unchanged
                         └───────────────┬──────────────────┘
                                         │
                ┌────────────────────────┴───────────────────────┐
                │ src/rl/ (C++, in bs_game, headless)             │  new, small
                │  shot_lab:   layout + strike → ShotOutcome      │
                │              batches across threads             │
                │  geometry:   pocket mouths, ghost ball, cut,    │
                │              blocking, baseline aim             │
                │  layouts:    seeded generators, difficulty      │
                └────────────────────────┬───────────────────────┘
                                         │ pybind11 (billiards_sim)
                ┌────────────────────────┴───────────────────────┐
                │ ml/billiards_rl/ (Python)                       │  new
                │  envs/  rewards/  curriculum/  baselines/       │
                │  training/  evaluation/  configs/               │
                └────────────────────────┬───────────────────────┘
                                         │
                       Gymnasium API → Stable-Baselines3 (SAC, TD3, PPO, HER)

  Later:  StrategicPolicy π_H(s) → goal g ──► ShotPolicy π_L(s, g) → strike
          (search / MCTS / self-play)          (the policies learned here)
          Full games reuse Ai::Environment (feat/m9-environment).
```

**Why a C++ `shot_lab` and not the existing `Ai::Environment`?** The M9
environment plays whole frames through `MatchSession` (ECS registry, racking,
referee). That is the right tool for full games (Milestone 13) but adds
overhead and constraints (15 balls, legal racks) to a one-shot problem. The
shot lab is a thin layer over `simulateShot` that takes any layout.

**Why keep the geometry in C++?** The baseline, the layout generator (which
must know which shots are makeable) and later the classical AI all need the
same ghost-ball and pocket-mouth maths. One tested implementation avoids
three slightly different ones.

## 3. Coordinates and units

The RL layer uses the **simulator frame**: metres, origin at a corner of the
playing surface, $x$ across the table ($W = 1.27$ m), $y$ along it
($L = 2.54$ m). Observations are normalised to $[-1, 1]$:

$$
x' = \frac{2x}{W} - 1, \qquad y' = \frac{2y}{L} - 1 .
$$

Angles are directions of travel in the table plane, counter-clockwise from
$+x$, as in `Sim::CueStrike::phiDegrees`.

## 4. The simulation API (C++ and Python)

### `ShotOutcome`

What one shot did, read from the trajectory's events:

| Field | Meaning |
|---|---|
| `pots` | (ball, pocket) in the order they dropped |
| `scratch` | the cue ball was pocketed |
| `first_contact` | first ball the cue ball touched, $-1$ if none |
| `cushions_before_pot` | cushion contacts of each potted ball before it dropped |
| `final_positions`, `active` | the table at rest |
| `events`, `complete` | event count; false if the event limit was reached |
| `trajectory` (optional) | sampled positions for plots |

### Functions

```python
import billiards_sim as bs

table = bs.Table.standard()                     # 9 ft table, pocket mouths
out = bs.simulate(table, layout, strike)        # one shot → ShotOutcome
outs = bs.simulate_batch(table, layouts, strikes, threads=0)   # numpy in/out,
                                                # GIL released, all cores
```

`layout` is an array of shape (16, 2) of positions plus a (16,) mask of
balls on the table; `strike` is (speed, phi, theta, a, b) in physical units.
The batch call is what makes vectorised environments and simulator-search
baselines fast: at about 17k shots/s per core, Python call overhead, not
physics, is the bottleneck.

## 5. `SingleShotEnv` (Milestone 3)

**Task.** One cue ball, one object ball, six pockets. One action is one shot.
The goal is to pot the object ball.

**Episode.** `reset(seed)` draws a layout from the configured generator. The
agent chooses one action, the shot is simulated to rest, the reward is
computed, and the episode terminates (`terminated=True`). One episode is one
physical shot, a contextual bandit with a continuous action. That is
deliberate: it isolates execution precision.

### Observation

Phase 1, `obs_mode="positions"`: four numbers,

$$
s = \left[x'_c,\ y'_c,\ x'_o,\ y'_o\right] \in [-1, 1]^4 .
$$

Pocket positions are fixed for a given table, so they are left out. They
would add twelve constants the network must learn to ignore. If table
variants are introduced later, they come back in.

Ablation, `obs_mode="positions+geometry"`: for each pocket $k$, the cut angle
$\theta_k$ (as $\cos\theta_k$), the distances $d_c$ (cue to ghost ball) and
$d_{o,k}$ (object ball to the pocket mouth), and a blocked flag. These are
derived quantities the network could learn, but which may speed learning
greatly. Whether they help is an experiment, not an assumption.

### Action

The action is normalised to $[-1, 1]^n$ and mapped to physical units inside
the environment.

**Phase 1a, `action_frame="absolute"`:** $a = (a_\varphi, a_v)$,

$$
\varphi = 180°\, a_\varphi, \qquad v = v_\min + \tfrac{1}{2}(a_v + 1)(v_\max - v_\min),
\quad v \in [0.5, 7]\ \mathrm{m/s}.
$$

This is the brief's formulation and the honest hard version. The audit
measured windows of 0.2–0.6° of $\varphi$: about $10^{-3}$ of the action
range. A random action pots a typical cut 0.19% of the time.

**Phase 1b, `action_frame="ghost_relative"`:** the environment exposes the
geometric aim for a chosen pocket $k$, and the action is a correction around
it,

$$
\varphi = \varphi_\text{ghost}(k) + \Delta\, a_\varphi, \qquad \Delta = 4°.
$$

Here the policy learns what geometry gets wrong: throw, jaw clearance, speed.
The pocket $k$ is either fixed by the layout generator (one designated
pocket) or, in Milestone 8, part of the goal. This frame is the bridge to the
hierarchical design ($\pi_L$ refines a strategic choice).

Comparing 1a and 1b, with everything else equal, is one of the first
experiments. It quantifies how much of the difficulty is the needle-like
search, as opposed to learning physics.

**Later:** spin $(a, b) \in [-0.525, 0.525]^2$ (fractions of $R$, the game's
range), then elevation. The simulator supports both reliably. They are held
back until position play needs them, because they enlarge the search space.

### Reward (`rewards/`, separate from mechanics)

Default (`reward="sparse"`), exactly as the brief proposes:

| Outcome | Reward |
|---|---|
| Object ball potted, cue ball not potted | $+1.0$ |
| Cue ball potted (scratch), whatever else | $-1.0$ |
| Anything else | $-0.1$ |

Rewards are functions of `ShotOutcome` and a config, never computed inside the
simulator call. Shaped variants (distance of the object ball to the pocket
after a miss, legal contact) are separate, named reward functions, each
justified by an experiment that shows it helps on the fixed evaluation suite.

### `info`

`potted`, `pocket`, `scratch`, `first_contact`, `object_to_pocket_after`
(m), `events`, `layout_id`, `difficulty`. Metrics are computed from `info`,
not from the reward.

## 6. Layouts, difficulty and the curriculum

A **layout generator** draws positions with a seeded generator (NumPy
`Generator`, one per environment). Every layout is checked for overlaps and
clearance from cushions and pockets. A layout's **difficulty** is measured
geometrically for its easiest unblocked pocket, by the cut angle $\theta$ and
the distances $d_c$ and $d_o$, using the same expression the classical AI's
"ease" approximates. The aim error amplification derived in
`docs/design/AI.md` is

$$
\delta\psi \approx \delta\varphi\left(1 + \frac{d_c}{2R\cos\theta}\right).
$$

Bands (tunable in config):

| Band | Cut angle $\theta$ | $d_c$ | $d_o$ |
|---|---|---|---|
| easy | $< 15°$ | $< 0.6$ m | $< 0.6$ m |
| medium | $< 45°$ | $< 1.2$ m | $< 1.2$ m |
| hard | $< 70°$ | any | any |

The **curriculum** (Milestone 7) follows the brief's stages: aligned and close,
then wider angles, random cue ball, random object ball, long shots, then banks
and obstacles. It advances when the rolling success rate exceeds a threshold
(default 0.8 over the last 2,000 episodes). Stages are data in the config,
and the curriculum is a Gymnasium wrapper, so it can be switched off for the
ablation.

## 7. Baselines (Milestone 4)

1. **Random**: uniform over the action box. It gives the floor and checks the
   reward pipeline.
2. **Geometry solver**: for each pocket, aim at the **mouth** (midpoint of the
   jaw tips, set back by a ball radius), not the drop circle; the audit showed
   that aiming at the drop circle misses even straight shots. Skip pockets
   whose path is blocked or whose cut exceeds 75°. Choose the pocket with the
   largest ease, $\cos^2\theta\, e^{-(d_c + d_o)/2.2}$. Set the speed by
   distance and cut. This is deterministic, with no simulator calls.
3. **Simulator search (upper bound)**: the geometry aim, plus a 1-D search of
   $\varphi$ within ±2° and three speeds, each evaluated by `simulate_batch`.
   It uses the simulator as a perfect model, so it is not a policy that
   competes on equal terms. It shows what is achievable and how far a
   model-free policy is from it.
4. **Classical AI** (later, multi-shot tasks): the game's planner.

RL is never assumed to beat these. If the geometry solver plus search wins,
that is a result worth reporting, and the comparison under execution noise
(Section 10) is where learned policies can differ.

## 8. Algorithms (Milestones 5–9)

**Library:** Stable-Baselines3 (PyTorch, already a dependency), which provides
SAC, TD3, PPO and `HerReplayBuffer`, plus `gymnasium`. No algorithm is written
from scratch at first. Custom code is reserved for the research parts (elite
sampling, SCOOT-style methods), where it is the contribution.

| Milestone | Method | Why |
|---|---|---|
| 5 | SAC | Off-policy and sample-efficient; entropy keeps exploring a needle-shaped reward landscape |
| 6 | TD3, PPO | Deterministic off-policy, and an on-policy baseline; all three get a fair tuning budget |
| 7 | Curriculum | Dense success early, so the critic gets signal |
| 8 | Goal-conditioned SAC + HER | Relabelling "potted in pocket $j$" turns misses into successes for another goal |
| 9 | Elite-sample / AWR, SCOOT-inspired | Regress on the best samples near narrow optima; studied first, then compared under control |

**Goal-conditioned form (Milestone 8).** The observation becomes a dict with
`observation` (positions), `desired_goal` (target pocket, one-hot, later with
a cue-ball target region) and `achieved_goal` (the pocket the ball dropped
into, or none), with `compute_reward(achieved, desired, info)` vectorised, as
SB3's HER requires. The environment is designed now so this is an additional
wrapper, not a rewrite.

**Hyperparameters** live in YAML (`configs/sac_single_ball.yaml`, ...), with
SB3's documented defaults as the starting point for every algorithm, and the
same tuning budget for each.

## 9. Evaluation protocol

- **Fixed suites**, generated once by a script from fixed seeds and committed
  under `ml/eval_states/{easy,medium,hard}.json` (1,000 layouts each). Each
  carries the environment version, the table spec and, per layout, the
  geometric difficulty. Training never samples from these exact layouts.
- **Policies are evaluated deterministically** (the mean action for
  stochastic policies) on every layout of every suite. They are also
  evaluated with execution noise (Section 10).
- **Seeds:** each configuration is trained with 5 seeds; results report mean,
  standard deviation and a 95% interval of the mean.
- **Metrics:** pocket success rate, scratch rate, legal-contact rate, mean
  distance of the object ball to the nearest pocket after a miss, mean
  reward. Sample efficiency is the number of environment steps to reach 50%
  and 80% success on *medium*. Later: cue-ball position error, rack win rate,
  balls per turn, run length, safety success.
- **Statistical comparisons** use the same layouts for all methods (paired),
  with a McNemar test on per-layout success.

## 10. Robustness

Execution noise is a wrapper that perturbs the physical action before
simulation:

$$
\varphi' = \varphi + \varepsilon_\varphi, \quad v' = v\,(1 + \varepsilon_v),
\qquad \varepsilon_\varphi \sim \mathcal N(0, \sigma_\varphi^2),\ \varepsilon_v \sim \mathcal N(0, \sigma_v^2).
$$

Success is reported against $\sigma_\varphi \in \{0, 0.05°, 0.1°, 0.2°, 0.4°\}$.
The classical AI's profiles span 0.16°–1.1°, which ties the experiment to the
game's opponents. Perturbing positions and friction ($\mu_s$, $e_b$) are
further wrappers. With a 0.26° window, $\sigma_\varphi = 0.2°$ should already
cost a precise policy heavily. That is the point where policies trained under
noise should prefer robust shots, and where RL may beat the noiseless geometry.

## 11. Experiment tracking

Local first, with no external accounts. Each run writes:

```text
ml/runs/<date>-<algo>-<env>-<seed>/
├── config.yaml        resolved config (algorithm, env, reward, curriculum)
├── meta.json          git commit, dirty flag, env version, platform, seed, start/end
├── progress.csv       training curve (SB3 logger)
├── eval/*.json        per-suite metrics at each evaluation
└── checkpoints/       best and last
```

TensorBoard reads the same logs (SB3 writes them). MLflow or W&B can be added
later behind a flag. `ml/runs/` and checkpoints stay out of git (already in
`.gitignore` on the M9 branch). Write-ups go in `docs/rl/experiments.md` with
the structure the brief specifies: hypothesis, setup, algorithm,
hyperparameters, metrics, results, interpretation, failure modes, next
experiment.

## 12. Tests

C++ (doctest, `tests/test_shot_lab.cpp`): outcomes agree with the
trajectory's events; batch results equal sequential ones; geometry (the
mouth point, the ghost point, the cut angle, blocking) on hand-computed
cases; the generator is deterministic and its layouts legal.

Python (pytest, `ml/tests/`):

- `reset` gives a valid observation of fixed shape and dtype, finite, in
  bounds; the same seed gives the same layout.
- Every action in the box maps to physical bounds.
- Every step terminates (`complete` is true).
- A scripted straight shot pots; a scripted shot into the cue ball's pocket
  scratches; the reward matches the outcome for both.
- The evaluation suites load deterministically and their hash matches.
- Gymnasium's `check_env` passes.

## 13. Performance

Measured: about 17,300 single-ball shots/s on one core (C++, Release). The
plan:

- Expose `simulate_batch` (C++ threads, GIL released).
- Use SB3 vectorised environments: a `DummyVecEnv` first; a batched VecEnv
  calling `simulate_batch` if the profile shows Python overhead dominating.
- Measure shots/s through the Python environment, SB3 updates/s, and policy
  inference time, and record them in `docs/rl/environment.md`.

The simulator is not optimised until a profile says it is the bottleneck.

## 14. Later milestones, designed for now

- **Cue-ball positioning (Milestone 10):** the goal adds a target region for
  the cue ball, rewarded as
  $R = R_\text{pot} - \lambda\, d(\mathbf{p}_\text{cue}, \mathbf{p}_\text{target})$,
  with $\lambda$ tuned and the region given in the goal. Spin enters the
  action space here.
- **Hierarchy (Milestone 11):** $\pi_H(s) \to g = (\text{ball}, \text{pocket}, \text{cue region})$
  and $\pi_L(s, g) \to a$. The goal type defined in Milestone 8 is that
  interface.
- **Search (Milestone 12):** $\pi_L$ proposes $N$ shots per goal for the top
  goals of $\pi_H$; `simulate_batch` evaluates them under noise; a search
  (expectimax over noise, then MCTS) chooses. This keeps the branching factor
  small, as the brief requires, and generalises the classical planner, which
  already does candidate generation plus noisy evaluation.
- **Self-play (Milestone 13):** full frames through `Ai::Environment` (M9
  branch), with the classical AI and earlier policy versions as opponents,
  and an Elo ladder. The M9 value network becomes $\pi_H$'s critic once
  trained on better targets.

## 15. File-level implementation plan

### Milestone 2: headless simulation API

| File | Change |
|---|---|
| (branch) | Bring `feat/m9-environment` up to date with `main` (Python module, `ml/`, `.gitignore` entries); work continues on `feat/rl-shot-lab` from there |
| `src/rl/shot_lab.h/.cpp` | `ShotOutcome`, `simulateOutcome(table, layout, strike)`, `simulateBatch(...)` with `std::thread` |
| `src/rl/geometry.h/.cpp` | `pocketMouths(table)`, `ghostAim(cue, object, mouth)`, `cutAngle`, `pathBlocked`, `geometryShot(layout, pocket)` |
| `src/rl/layouts.h/.cpp` | Seeded single-ball generator with legality checks and difficulty measurement |
| `src/python/billiards_sim_module.cpp` | Bind the above; NumPy arrays in and out; GIL released for batches |
| `tests/test_shot_lab.cpp` | The C++ tests of Section 12 |
| `CMakeLists.txt` | Add `src/rl/*` to `bs_game` and the test |
| `.gitignore` | Remove the committed `.pyc`; ignore `__pycache__/` |

### Milestone 3: `SingleShotEnv`

| File | Change |
|---|---|
| `ml/pyproject.toml` | Add `gymnasium`, `stable-baselines3`, `pyyaml`, `pytest`, `tensorboard` |
| `ml/billiards_rl/envs/single_shot_env.py` | The environment (Section 5): observation and action modes, seeding |
| `ml/billiards_rl/rewards/single_shot.py` | `sparse` reward and later shaped variants |
| `ml/billiards_rl/envs/wrappers.py` | Execution noise; later curriculum and goal wrappers |
| `ml/tests/test_single_shot_env.py` | The Python tests of Section 12, plus `check_env` |
| `docs/rl/environment.md` | The environment's specification and measured throughput |
| `docs/rl/reward-design.md` | The reward, why, and how changes are justified |

### Milestone 4: baselines and evaluation suites

| File | Change |
|---|---|
| `ml/billiards_rl/baselines/{random_policy,geometry,sim_search}.py` | Section 7 |
| `ml/scripts/make_eval_suites.py` → `ml/eval_states/*.json` | Fixed suites with a version hash |
| `ml/billiards_rl/evaluation/evaluate.py` | Runs any policy on a suite and writes metrics JSON |
| `docs/rl/experiments.md` | Experiment 0: the baselines on all suites |

### Milestone 5: SAC

| File | Change |
|---|---|
| `ml/configs/sac_single_ball.yaml` | Algorithm, environment, reward and evaluation config |
| `ml/billiards_rl/training/train.py` | `python -m billiards_rl.training.train --config ... --seed 42` |
| `ml/billiards_rl/training/run_logger.py` | `meta.json`, configs, evaluation callbacks |
| `docs/rl/papers.md` | Notes on SAC, TD3, PPO, HER, AWR, SCOOT, and the billiards literature |

## 16. Decisions to confirm

1. **Library:** Stable-Baselines3 + Gymnasium (proposed).
2. **Tracking:** local run folders + TensorBoard first; MLflow later (proposed).
3. **Branching:** finish integrating `feat/m9-environment` with `main`, then
   work on `feat/rl-*` branches with one milestone each (proposed).
4. **First experiment:** SAC with `action_frame=absolute` versus
   `ghost_relative`, both against the geometry and search baselines
   (proposed).
