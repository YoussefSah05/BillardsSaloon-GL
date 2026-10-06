# RL Environment Reference

Status: Milestones 2 (headless shot API) and 3 (`SingleShotEnv`) done;
baselines and evaluation suites (Milestone 4) next. Last revised 2026-10-06. Design and rationale:
[rl-system-design.md](rl-system-design.md).

## Shot API (`billiards_sim.rl`, C++ in `src/rl/`)

All quantities are in the simulator frame: metres, origin at a corner of the
playing surface, $x$ across the table ($W = 1.27$ m), $y$ along it
($L = 2.54$ m). Ball 0 is the cue ball. A strike is
$(v, \varphi, \theta, a, b)$: cue speed in m/s, direction of travel in degrees
counter-clockwise from $+x$, cue elevation in degrees, and tip offsets as
fractions of $R$ ($a > 0$ is left english, $b > 0$ is follow).

| Function | Returns |
|---|---|
| `table_info()` | `width`, `length`, `ball_radius`, `pockets` (6×2), `pocket_radius`, `mouths` (6×2), `mouth_inward`, `mouth_half_width` |
| `simulate(positions, strike)` | `pocket_of` (n), `cushions_before_pot` (n), `final_positions` (n×2), `pot_order`, `scratch`, `first_contact`, `events`, `complete` |
| `simulate_batch(positions, strikes, threads=0)` | the same as arrays with a leading batch axis; threads 0 = all cores; the GIL is released |
| `ghost_aims(positions, object_index=1)` | per pocket: `phi`, `cut`, `cue_distance`, `object_distance`, `ease`, `blocked` |
| `geometry_shot(positions, object_index=1, pocket=-1, max_cut=75)` | the geometry baseline's `strike`, `pocket`, `ease` and `ghost`; `None` if no clear line |
| `LayoutGenerator(seed)` | `.single_ball(band)` and `.analyse(positions)` → `positions`, `difficulty`, `pocket`, `ghost`; `.legal_spot(x, y, others)` |

**Pocket mouths.** Each pocket's mouth is the midpoint between its two jaw
tips (the circular cushions nearest the pocket), with
$\text{half width} = \tfrac12\lVert \mathbf{j}_1 - \mathbf{j}_2\rVert - \bar r_\text{jaw}$:
58 mm for corners and 68 mm for sides. Aiming at the mouth, not at the drop
circle, is what makes the geometry baseline work (audit, risk 2).

**Layouts.** Both balls rest at least 5 mm inside the cushion lines, 10 mm
apart, and 30 mm clear of every pocket's drop circle. Difficulty bands are
exclusive, from the easiest unblocked line: *easy* (cut $< 15°$, both
distances $< 0.6$ m), *medium* (cut $< 45°$, distances $< 1.2$ m), *hard*
(cut $< 70°$), *none*. Uniform numbers come from the generator's raw 64-bit
output, $u = \lfloor x / 2^{11} \rfloor \cdot 2^{-53}$, so a seed gives the same
layouts with every standard library (`std::uniform_real_distribution` does
not).

**Geometry baseline speed.** The baseline works backwards from an arrival
speed $v_a = 0.6$ m/s at the mouth. A rolling ball slows at $\mu_r g$, so
$v_\text{start}^2 = v_\text{end}^2 + 2\mu_r g\, d$. A ball struck at the
centre slides first and rolls at $5/7$ of its launch speed. A rolling cue
ball passes $\tfrac12(1 + e_b)\cos\theta$ of its speed to the object ball. A
centre-ball strike gives the ball $2V/(1 + m/M)$:

$$
V = \frac{1 + m/M}{2}\cdot\frac{7}{5}\sqrt{\left(\frac{\tfrac75 \sqrt{v_a^2 + 2\mu_r g\, d_o}}{\tfrac12(1+e_b)\cos\theta}\right)^2 + 2\mu_r g\, d_c}\ ,
$$

clamped to the game's range of 0.5–7 m/s.

## Measurements

Apple M3 Pro (6 performance + 6 efficiency cores), Release build.

| Quantity | Value |
|---|---|
| Random single-ball shots, C++, one thread | 17,700 shots/s |
| Through Python, one call per shot | 16,900 shots/s |
| `simulate_batch`, all 12 cores | 113,000 shots/s |
| Mean events per random shot | 10.3 |
| Incomplete simulations (event limit hit) | 0 of 70,000 |

Python call overhead is about 5%, not the factor the audit feared (risk 8).
Batching mainly buys parallelism.

**Baselines on generated layouts** (300 layouts per band, seed 7;
`tests/test_shot_lab.cpp`). These are success rates: the object ball potted
and the cue ball not.

| Policy | easy | medium | hard |
|---|---|---|---|
| Random strike (any layout) | 2.1% | | |
| Geometry baseline, exact execution | 86% | 89% | 84% |
| Geometry baseline, aim error $\sigma_\varphi = 0.25°$ | 81% | 66% | 43% |

Two findings shape the next milestones:

1. **Exact execution hides difficulty.** Aimed at the mouth, the geometry
   baseline pots 84–89% in every band; its misses come from throw and the
   jaws, not from the shot's difficulty. A learned policy in a noiseless
   simulator competes with that.
2. **Difficulty is sensitivity to error.** With an aim error of
   $\sigma_\varphi = 0.25°$, success falls to 81% on easy layouts, 66% on
   medium and 43% on hard, as the amplification
   $\delta\psi \approx \delta\varphi\,(1 + d_c / (2R\cos\theta))$ predicts.
   The robustness experiments (design, Section 10) are where policies should
   differ most.

These numbers are from the baseline test's own layouts; the fixed evaluation
suites of Milestone 4 replace them for reporting.

## `SingleShotEnv` (`ml/billiards_rl/envs/single_shot_env.py`)

One cue ball, one object ball, one shot; `step` always terminates. It passes
Gymnasium's `check_env`.

| Parameter | Values | Meaning |
|---|---|---|
| `band` | `easy`, `medium`, `hard`, `any` | difficulty of generated layouts |
| `obs_mode` | `positions` (4) | $[x'_c, y'_c, x'_o, y'_o]$, with $x' = 2x/W - 1$ and $y' = 2y/L - 1$ |
| | `positions+geometry` (28) | plus, per pocket: $\cos\theta_k$, $d_{c,k}/L$, $d_{o,k}/L$, blocked ($\pm1$) |
| `action_frame` | `absolute` | $\varphi = 180°\,a_0$ |
| | `ghost_relative` | $\varphi = \varphi_\text{ghost}(k^\star) + \Delta\,a_0$, $\Delta = 4°$ (`max_delta_deg`) |
| (both) | | $v = v_\min + \tfrac12(a_1 + 1)(v_\max - v_\min)$, 0.5–7 m/s |
| `reward` | `sparse`, `sparse_target` | see [reward-design.md](reward-design.md) |
| `noise` | $(\sigma_\varphi, \sigma_v)$ | Gaussian aim error (degrees) and relative speed error on the *executed* shot |
| `layouts` | list of (2, 2) arrays | a fixed cycle of states instead of generation (evaluation) |

The target pocket $k^\star$ is the layout's easiest clean pocket. If no clean
line exists, it is the clear pocket of greatest ease; if there is none, the
reference aim is a full hit on the object ball. `reset(seed=...)` seeds
Gymnasium's `np_random`, which seeds the C++ generator, so one seed fixes the
whole layout sequence. `reset(options={"positions": p})` plays a given layout.

`info` carries `success` (potted, no scratch), `potted`, `pocket`, `scratch`,
`first_contact`, `legal_contact`, `object_to_pocket_after` (m, distance from
the object ball to the nearest mouth after a miss), `events`, `complete`,
`difficulty`, `target_pocket`, `phi` and `speed` (the intended shot).

Helpers: `strike_for(action)` → the physical strike;
`action_for(phi, speed)` → the action that plays it (used by baselines).

**Measured** (one process, random actions, step + reset each episode):

| Configuration | Steps/s | Random success |
|---|---|---|
| `positions`, `absolute` | 11,900 | 1.9% |
| `positions+geometry`, `ghost_relative` | 6,800 | 14.3% |

The ghost-relative frame makes a random policy succeed about 8 times more
often: the needle is still narrow, but it is in the middle of the action box,
not one of a thousand places around the circle.

## Tests

- C++: `tests/test_shot_lab.cpp`: mouths, ghost geometry, the speed model,
  outcome extraction (pot, scratch, first contact), batch equal to
  sequential, the generator's determinism, legality and bands, the baseline.
- Python: `ml/tests/test_shot_api.py` (`uv run pytest`): shapes, determinism,
  batch equal to single shots, scratch detection, the baseline on easy
  layouts, ghost aims, rejection of bad shapes.
- Python: `ml/tests/test_single_shot_env.py`: `check_env` for every mode,
  observation shape, dtype, bounds and finiteness, seeding, action bounds,
  `action_for` inverting `strike_for`, termination, reward consistency, the
  geometry shot through both frames, fixed and given layouts, execution noise,
  and the reward definitions.
