# Experiments

Every experiment follows the same structure: hypothesis, setup, algorithm,
hyperparameters, metrics, results, interpretation, failure modes, next
experiment. Results are from the fixed suites in `ml/eval_states/` (1,000
layouts each, hashed). Intervals are 95% Wilson intervals over layouts. All
numbers are reproducible with the command given.

Hardware: Apple M3 Pro, macOS, Release build of the simulator.

---

## Experiment 0 — Baselines

**Hypothesis.** Before any learning, three scripted policies bound the
problem:

- a uniformly random policy gives the floor;
- a geometry solver shows what pure geometry achieves;
- a simulator search shows what is achievable at all in this simulator;
- aiming noise separates the difficulty bands.

**Setup.** `SingleShotEnv` (`single_shot-v1`), absolute action frame. The
suites are *easy*, *medium* and *hard* (Section 6 of the design). Each policy
plays every layout once. Aim noise $\varepsilon_\varphi \sim \mathcal N(0, \sigma^2)$
is applied to the executed shot with a fixed seed, identical across policies.

**Policies.**

- *random*: uniform actions.
- *geometry*: the ghost-ball aim at the easiest clear pocket mouth, with the
  friction-model speed. It makes no simulator calls.
- *sim_search*: for the three clearest pockets × three speeds (0.8, 1.0 and
  1.3 times the model speed), it scans the aim over ±2° in 0.05° steps with
  the simulator (729 shots, one batch), then plays the centre of the widest
  contiguous run of pots. It is privileged: it simulates before acting.

**Command.** `uv run python -m scripts.experiment0_baselines` (about 2
minutes).

**Results** (success = object ball potted, no scratch):

| Policy | Aim noise σ | easy | medium | hard |
|---|---|---|---|---|
| random | — | 3.1% ± 1.1% | 1.9% ± 0.9% | 2.0% ± 0.9% |
| geometry | 0° | 82.6% ± 2.3% | 86.1% ± 2.1% | 85.4% ± 2.2% |
| geometry | 0.1° | 82.3% ± 2.4% | 82.0% ± 2.4% | 68.8% ± 2.9% |
| geometry | 0.25° | 80.9% ± 2.4% | 65.1% ± 2.9% | 47.3% ± 3.1% |
| geometry | 0.5° | 72.3% ± 2.8% | 46.6% ± 3.1% | 28.8% ± 2.8% |
| sim_search | 0° | 100.0% ± 0.2% | 99.8% ± 0.3% | 99.5% ± 0.5% |
| sim_search | 0.1° | 99.8% ± 0.3% | 96.5% ± 1.2% | 83.1% ± 2.3% |
| sim_search | 0.25° | 95.7% ± 1.3% | 75.4% ± 2.7% | 54.5% ± 3.1% |
| sim_search | 0.5° | 83.9% ± 2.3% | 51.4% ± 3.1% | 33.6% ± 2.9% |

Scratch rates without noise: random 19–23%; geometry 2.5% (easy), 3.2%
(medium), 7.9% (hard); sim_search 0.0–0.5%. Legal first contact: random
16–20%; geometry and sim_search 97–100%.

**Interpretation.**

1. **The task is hard for undirected search and easy for geometry.** A random
   shot succeeds 2–3% of the time; aiming at the pocket mouth succeeds 83–86%.
   Almost all of the difficulty lies in finding the needle.
2. **Geometry leaves 14–17 points on the table, even with perfect
   execution.** Search reaches 99.5–100% on the same layouts, so for nearly
   every layout a potting shot exists close to the geometric one.
   - The misses come from what geometry ignores: throw, the jaws, and the
     cue ball following the object ball in (scratches rise to 7.9% on hard
     layouts).
   - This is the residual a learned policy can learn, and the ghost-relative
     action frame is built to expose it.
3. **The bands measure sensitivity to error, not intrinsic difficulty.**
   - Without noise, geometry and search do almost equally well in every band.
   - With aim noise, easy layouts lose little (geometry: 82.6% to 72.3% at
     0.5°) and hard ones collapse (85.4% to 28.8%).
   - This is the amplification $1 + d_c/(2R\cos\theta)$ at work.
4. **Choosing robust shots pays under noise.** At $\sigma = 0.25°$, search
   beats geometry by 14.8 points on easy, 10.3 on medium and 7.2 on hard, by
   playing the centre of the widest window, not the geometric line.
   - A policy trained under noise should learn the same preference.
   - This is a concrete target for the robustness experiments.

**Failure modes and caveats.**

- Search is an upper bound only for its own candidate set: three pockets,
  three speeds, ±2°, centre-ball strikes. Spin could widen windows further.
- The geometry speed model was not tuned. Its scratch rate on hard layouts
  suggests it is too fast for some long cuts.
- The suites contain only layouts with a clean line. Layouts with no clean
  pot (difficulty *none*) are not yet evaluated.

**Next.** Experiment 1: SAC on `SingleShotEnv`, absolute versus
ghost-relative action frames, 5 seeds each, evaluated on these suites
against these baselines.

---

## Experiment 1 — SAC: absolute versus ghost-relative action frame

**Hypothesis.** With an absolute aim angle, potting windows are about $10^{-3}$
of the action range, so SAC with default settings will rarely find them and
will learn little. Expressing the aim as a correction of up to ±4° around the
ghost-ball line to the easiest pocket puts the window in the middle of the
action box. SAC should then learn quickly, and may beat the geometry baseline
by learning what geometry ignores (throw, jaws, speed).

**Setup.** `SingleShotEnv` (`single_shot-v1`), `band=any` for training,
`obs_mode=positions` (4 numbers), `reward=sparse`, no noise during training.
Evaluation on the fixed suites with the best checkpoint by success on 300
*medium* layouts, measured every 10k steps. Seeds 1–5 for each frame.

**Algorithm and hyperparameters.** Stable-Baselines3 SAC with its defaults
(`configs/sac_single_ball.yaml`): learning rate $3\cdot10^{-4}$, buffer $10^6$,
batch 256, $\tau = 0.005$, automatic entropy, 1,000 random warm-up steps, one
gradient step per environment step, MLP [256, 256]. 100k steps per run, CPU,
one thread per run, about 380 s per run.

**Command.** `bash scripts/experiment1_sac_frames.sh`, then
`uv run python -m scripts.summarise_runs runs/experiment1`.

**Results** (mean ± sd over 5 seeds; success = potted without a scratch):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° |
|---|---|---|---|---|---|---|
| SAC, absolute | 1.7% ± 0.8 | 1.4% ± 0.4 | 1.0% ± 0.6 | 1.7% ± 0.8 | 1.4% ± 0.3 | 0.9% ± 0.4 |
| SAC, ghost-relative | 86.9% ± 0.8 | 83.6% ± 0.9 | 81.7% ± 0.6 | 82.8% ± 0.8 | 58.7% ± 1.0 | 41.3% ± 0.6 |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% |
| random (Exp. 0) | 3.1% | 1.9% | 2.0% | — | — | — |
| sim_search (Exp. 0) | 100.0% | 99.8% | 99.5% | 95.7% | 75.4% | 54.5% |

Learning curves (mean of 5 seeds, 300 *medium* layouts):

- **absolute:** 2% at 10k steps, 0–1% thereafter.
- **ghost-relative:** 60% at 10k, 78% at 20k, 81% at 30k, then flat at 81–82%
  up to 100k.

**What the ghost-relative policy learned** (seed 1, paired with the geometry
policy on the same layouts, both in the ghost-relative frame):

| Suite | Pots only SAC | Pots only geometry | McNemar $\chi^2$ | SAC aim − geometry aim | SAC speed / geometry speed (median) |
|---|---|---|---|---|---|
| easy | 56 | 13 | 25.6 ($p < 0.001$) | $-0.01° \pm 0.14°$ | 2.4× |
| medium | 65 | 76 | 0.7 (n.s.) | $-0.01° \pm 0.55°$ | 2.0× |
| hard | 54 | 90 | 8.5 ($p \approx 0.004$) | $+0.01° \pm 0.18°$ | 1.6× |

**Interpretation.**

1. **The action parameterisation decides whether SAC learns at all.** In the
   absolute frame, SAC ends slightly *below* a random policy. Entropy-driven
   exploration around a Gaussian mean never sees enough successes for the
   critic to form a gradient towards windows that occupy a thousandth of the
   action range. This confirms the audit's prediction, and it is the
   motivation for HER, curricula and elite-sample methods.
2. **In the ghost-relative frame, SAC learns fast and stably but plateaus
   at the geometry baseline.** All 5 seeds reach 81–82% (during-training
   subset) within 30k steps, with about 1 point of variation across seeds. On
   the full suites it is better than geometry on *easy* (significantly),
   equal on *medium*, and worse on *hard* (significantly).
3. **It learned speed, not aim.** Its aim stays on the ghost-ball line
   (mean offset 0.01°); its speed is about 2× the friction model's. That is
   physically sensible: collision-induced throw shrinks as the impact gets
   faster, so firmer shots go where geometry says. It pays on short, straight
   shots (*easy*) and costs on long, thin cuts (*hard*), where more pace
   means more cue-ball travel and more scratches, and where the aim needs
   corrections the policy never learned.
4. **Under noise the learned policy is less robust than geometry** (*hard*,
   σ = 0.25°: 41.3% against 47.3%). It was trained without noise and has no
   reason to prefer wide windows.
5. **The residual between geometry and search (14–17 points) is mostly
   unclaimed.** The policy sees only positions, so per-layout aim corrections
   (throw depends on cut angle and speed) must be inferred from 4 numbers
   through the critic's narrow peaks. This is the gap the next experiments
   target.

**Failure modes.**

- The best checkpoint is chosen on 300 *medium* layouts. Those layouts are
  also in the final *medium* suite, so the medium number may be slightly
  optimistic. Selection should move to a separate validation suite.
- Comparing geometry run in the ghost-relative frame (85.3% medium, 85.3%
  hard) with Experiment 0's absolute frame (86.1%, 85.4%) shows a small
  difference: the geometry solver's pocket can differ from the layout's
  designated pocket, and corrections beyond ±4° are clipped.
- Default hyperparameters were not tuned. This is deliberate (equal budget
  for all algorithms), but SAC's speed preference might change with a
  different entropy target.

**Next.**

- **Experiment 2:** TD3 and PPO under the same protocol, as the brief's
  milestone 6.
- **Then:**
  - `obs_mode=positions+geometry`, which hands the policy the cut angle and
    distances that throw corrections depend on;
  - training under execution noise, to learn robust shots;
  - a separate validation suite for checkpoint selection.

---

## Experiment 2 — SAC, TD3 and PPO

**Hypothesis.** The failure of the absolute frame in Experiment 1 is a
property of the task (a needle-shaped reward), not of SAC: TD3 and PPO will
fail there too. In the ghost-relative frame, the off-policy methods (SAC,
TD3) will be more sample-efficient than on-policy PPO at an equal number of
environment steps.

**Setup.** As in Experiment 1, with one protocol change. Checkpoints are
selected on a separate **validation suite** (600 layouts: 200 per band, seed
1004), never on a test suite; this fixes the selection caveat of Experiment 1.
Each algorithm uses Stable-Baselines3's defaults: the same budget and no
tuning for any of them.

- **SAC:** as in Experiment 1.
- **TD3:** learning rate $10^{-3}$, MLP [400, 300], policy delay 2, target
  smoothing 0.2. Gaussian exploration noise $\sigma = 0.1$ (SB3 documents
  this choice but sets no default).
- **PPO:** learning rate $3\cdot10^{-4}$, 2,048 steps per update, batch 64,
  10 epochs, clip 0.2, MLP [64, 64].

Both frames, seeds 1–5, 100k environment steps per run: 30 runs.

**Command.** `bash scripts/experiment2_algorithms.sh`, then
`uv run python -m scripts.summarise_runs runs/experiment2` and
`uv run python -m scripts.compare_to_baseline runs/experiment2`.

**Results** (mean ± sd over 5 seeds):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° | Time per run |
|---|---|---|---|---|---|---|---|
| SAC, absolute | 1.9% ± 0.7 | 1.4% ± 0.5 | 1.0% ± 0.6 | 1.9% | 1.3% | 1.0% | 428 s |
| TD3, absolute | 1.1% ± 0.8 | 0.7% ± 0.7 | 0.2% ± 0.2 | 0.9% | 0.6% | 0.4% | 439 s |
| PPO, absolute | 1.5% ± 0.5 | 1.1% ± 0.5 | 0.9% ± 0.2 | 1.6% | 1.0% | 0.8% | 27 s |
| SAC, ghost-relative | 86.9% ± 0.7 | 83.3% ± 0.8 | 82.2% ± 0.9 | 82.5% ± 0.9 | 58.6% ± 0.9 | 41.5% ± 0.5 | 438 s |
| TD3, ghost-relative | 86.0% ± 1.1 | 81.2% ± 2.3 | 79.3% ± 2.0 | 81.2% ± 0.8 | 58.3% ± 1.8 | 43.2% ± 1.0 | 394 s |
| PPO, ghost-relative | 86.5% ± 0.5 | 78.6% ± 0.6 | 73.5% ± 1.7 | 82.7% ± 0.6 | 55.8% ± 1.5 | 41.5% ± 0.5 | 37 s |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% | — |

**Paired with the geometry baseline** (no noise; exact McNemar test per seed,
$p < 0.05$):

| Policy | easy | medium | hard |
|---|---|---|---|
| SAC, ghost-relative | +4.3, better in 5/5 seeds | −2.8, worse in 4/5 | −3.2, worse in 4/5 |
| TD3, ghost-relative | +3.4, better in 5/5 | −4.9, worse in 4/5 | −6.1, worse in 5/5 |
| PPO, ghost-relative | +3.9, better in 5/5 | −7.5, worse in 5/5 | −11.9, worse in 5/5 |
| any algorithm, absolute | worse in 5/5 | worse in 5/5 | worse in 5/5 |

Learning curves on the validation suite (mean of 5 seeds), ghost-relative:

- **SAC:** 62% at 10k steps, 80% at 20k, 85% at 100k.
- **TD3:** 71% at 10k, 79% at 20k, 84% at 100k.
- **PPO:** 59% at 10k, 72% at 40k, 81% at 100k, still rising.

**Interpretation.**

1. **The absolute frame fails for every algorithm.** Off-policy or
   on-policy, stochastic or deterministic, all end below a random policy. The
   needle defeats undirected exploration regardless of the update rule. It is
   a property of the problem's parameterisation, which is the case for the
   brief's later milestones: goal-conditioning, HER, curricula and
   elite-sample methods.
2. **In the ghost-relative frame, all three learn the same thing.** On short
   straight shots (*easy*), each beats geometry in every seed, by firmer
   speed (Experiment 1). On *medium* and *hard*, none reaches geometry. The
   ranking SAC ≥ TD3 > PPO follows sample efficiency at equal steps.
3. **SAC is the most consistent** (seed sd 0.7–0.9 points, against 1.1–2.3
   for TD3).
4. **Equal steps is not equal compute.** PPO took 37 s per run against
   400–440 s for the off-policy methods (one gradient step per environment
   step on the CPU). Its curve is still rising at 100k steps. With equal
   wall-clock time it would see about 10 times more data, which is a fair
   comparison to run before ranking it last.
5. **Selecting on a separate validation suite changed SAC's numbers by under
   half a point** compared with Experiment 1. The earlier selection on
   *medium* layouts did not inflate the results.
6. **Under aiming noise, no learned policy matches geometry on medium or
   hard** (σ = 0.25°: 56–59% against 65%, 42–43% against 47%). No policy was
   trained with noise.

**Failure modes.**

- The geometry baseline in the paired test plays in the absolute frame, as in
  Experiment 0. The learned policies cannot correct more than ±4° from their
  reference pocket. That never limits a pot in practice, since search found
  wide windows within ±2°.
- All conclusions are for 100k steps and SB3 defaults.

**Next experiments.**

- **Observation:** `positions+geometry`, the cut angle and distances that
  throw and speed depend on, instead of inferring them from four coordinates.
- **Training under execution noise** ($\sigma_\varphi = 0.25°$), to see
  whether policies learn to prefer wide windows, as search does.
- **PPO at equal wall-clock time** (about 1M steps).
- **Then the brief's Milestone 7:** a curriculum, and ultimately the
  goal-conditioned environment with HER.

---

## Experiment 3 — Geometry observations, training under noise, PPO at equal compute

**Hypotheses.**

- **(a)** Giving the policy each pocket's cut angle, distances and blocking
  (the quantities throw and speed depend on) lets it learn per-layout
  corrections that it could not infer from four coordinates.
- **(b)** Training under aiming noise ($\sigma_\varphi = 0.25°$) teaches the
  policy to prefer shots with wide potting windows, as search does, so it
  becomes more robust to noise at some cost without noise.
- **(c)** PPO, ranked last at equal steps in Experiment 2, catches up at equal
  wall-clock time.

**Setup.** As in Experiment 2: ghost-relative frame, validation-suite
selection, 5 seeds per group, SB3 defaults.

- **SAC 2×2:** observation (`positions` or `positions+geometry`) × training
  noise (0 or 0.25°). The positions / no-noise cell is Experiment 2's
  `sac-ghost_relative` group (same protocol).
- Noise-trained runs select checkpoints under the noise they train with.
- **PPO:** 1M steps, positions, no noise, about the wall-clock time of 100k
  SAC steps (319 s per run against about 380–440 s).

**Command.** `bash scripts/experiment3_observation_noise.sh`, then
`scripts.summarise_runs` and `scripts.compare_to_baseline` (with and without
`--noise 0.25`) on `runs/experiment3`.

**Results** (mean ± sd over 5 seeds; *paired* = exact McNemar test per seed
against geometry, $p < 0.05$):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° |
|---|---|---|---|---|---|---|
| SAC, positions, clean (Exp. 2) | 86.9% ± 0.7 | 83.3% ± 0.8 | 82.2% ± 0.9 | 82.5% ± 0.9 | 58.6% ± 0.9 | 41.5% ± 0.5 |
| SAC, geometry, clean | **88.4% ± 1.2** | 85.4% ± 0.9 | 82.8% ± 1.5 | **83.5% ± 0.7** | 60.5% ± 0.5 | 42.8% ± 1.0 |
| SAC, positions, noisy | 85.3% ± 0.3 | 79.0% ± 1.2 | 67.0% ± 3.3 | 82.1% ± 0.6 | 59.7% ± 1.1 | 40.5% ± 1.0 |
| SAC, geometry, noisy | 86.1% ± 0.6 | 81.3% ± 1.5 | 70.2% ± 5.1 | 82.9% ± 0.7 | 61.4% ± 1.3 | 41.3% ± 1.7 |
| PPO, positions, clean, 1M steps | 82.2% ± 0.7 | **87.2% ± 1.0** | **85.2% ± 0.5** | 78.4% ± 0.7 | 64.1% ± 0.7 | 45.1% ± 0.6 |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | **65.1%** | **47.3%** |
| sim_search (Exp. 0) | 100.0% | 99.8% | 99.5% | 95.7% | 75.4% | 54.5% |

Paired with geometry, without noise:

- **SAC with geometry observations:** better on *easy* in 5/5 seeds (+5.8);
  no significant difference on *medium* (−0.7, 0/5 seeds either way); worse
  on *hard* in 3/5 seeds (−2.6).
- **PPO at 1M steps:** no significant difference anywhere except *medium*
  (+1.1, better in 2/5 seeds).
- **Noise-trained SAC:** worse on *medium* and *hard* in 5/5 seeds (−4.8 to
  −18.4).

Paired with geometry at σ = 0.25°: only *easy* favours any learned policy
(SAC with geometry, +2.6, 3/5 seeds); on *medium* and *hard* every learned
policy is worse in most seeds.

Learning curves on the validation suite:

- **SAC with geometry, clean:** 84% at 30k steps, 87% at 100k, still rising
  slowly.
- **Noise-trained SAC:** 63–64% at 100k, measured under noise.
- **PPO:** 81% at 100k, 84% at 250k, 85% from 600k to 1M.

**Diagnosis: where does search's robustness come from?** The search baseline
was rerun at σ = 0.25° with parts of its choice removed:

| Search variant | easy | medium | hard |
|---|---|---|---|
| Full (3 pockets × 3 speeds × aim) | 95.7% | 75.4% | 54.5% |
| 1 pocket × 3 speeds × aim | 92.4% | 74.0% | 52.6% |
| 3 pockets × 1 speed × aim | 95.9% | 74.7% | 53.0% |
| 1 pocket × 1 speed × aim (aim centring only) | 92.2% | 72.1% | 50.7% |
| geometry (no search) | 80.9% | 65.1% | 47.3% |

**Interpretation.**

1. **(a) is partly supported.** Geometry observations improve SAC on *easy*
   (+1.5 points over positions) and *medium* (+2.1). They reach geometry's
   level on *medium* and stay just below it on *hard*. Under noise they help a
   little everywhere (+1 to +2 points). SAC with geometry is the best learned
   policy on *easy*, with and without noise.
2. **(b) is rejected.** Training under noise cost 5–15 points without noise
   and bought no robustness: at σ = 0.25° the noise-trained policies are
   within about a point of the clean-trained ones, below geometry on
   *medium* and *hard*. Variance across seeds grew on *hard* (sd 3–5 points).
   Noise turns each training reward into a sample of a probability, which in
   principle smooths the landscape, but with 100k samples the critic does not
   resolve the differences between aims that matter.
3. **(c) is supported, with a twist.** At equal wall-clock time, PPO reaches
   geometry's level on *medium* and *hard* (87.2% and 85.2%), the only
   learned policy to do so. It is no longer better on *easy*. It converged to
   the geometry solution itself, firm-speed advantage included, and plateaus
   there from 600k steps.
4. **The ablation explains the gap that remains.** Search beats geometry
   under noise mainly by **centring the aim in the potting window**: with one
   pocket and one speed it still gains 11, 7 and 3 points on easy, medium and
   hard. Choosing the pocket or the speed adds only 1–4 points.
   - Because of throw and the jaws, the centre of the window is offset from
     the ghost-ball line, by a different amount on every layout.
   - That correction is within the policies' ±4° range, yet no policy learned
     it. Their aims stay within a few hundredths of a degree of the
     ghost-ball line on average (Experiment 1).
   - The bottleneck is not observation, algorithm or exploration range. It is
     learning a precise, layout-dependent offset of about 0.1–0.5° from a
     sparse, binary reward.

**Failure modes.**

- 100k steps for SAC; longer training might reduce the gap (SAC with
  geometry was still improving slowly).
- The search ablation describes search's candidate set, not the policies.
  That aim centring is learnable is an argument, not yet a demonstration.

**Next.** The diagnosis points to methods that turn a hit into a precise
target, rather than a scalar reward the critic must interpolate:

- **Elite-sample / advantage-weighted regression** (Milestone 9, brought
  forward): regress the policy onto the aims that potted, weighted by
  outcome.
- **Shaped targets that carry precision information.** The object ball's
  miss distance, signed by side of the pocket, tells which way to correct
  (`reward-design.md`, with an ablation).
- **The brief's curriculum (Milestone 7) and HER (Milestone 8),** which
  address the absolute frame's exploration failure more than this precision
  gap.
