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

---

## Experiment 4 — Curriculum (Milestone 7)

**Hypothesis.** In the absolute frame, every algorithm failed because
successes are too rare for the critic to form a gradient (Experiments 1–2).
Starting from shots where success is common (aligned, close) lets the policy
learn the smooth part of the mapping, roughly "aim from the cue ball through
the object ball towards a pocket", then sharpen it as shots get harder. In
the ghost-relative frame, which already learns, a curriculum should mainly
speed learning.

**Setup.** The curriculum wrapper (`billiards_rl/curriculum/`) generates
layouts within shot limits, built constructively: a random pocket, the
object ball in front of its mouth, the cue ball behind the ghost ball at a
random cut. It advances when success over the last 2,000 episodes of the
stage exceeds 0.8, and never moves back. Stages, following the brief:

| Stage | Max cut | Max cue distance | Max object distance |
|---|---|---|---|
| 0: aligned and close | 5° | 0.4 m | 0.4 m |
| 1: wider angles | 25° | 0.5 m | 0.5 m |
| 2: cue ball anywhere | 45° | 1.5 m | 0.6 m |
| 3: object ball anywhere | 60° | 2.0 m | 1.5 m |
| 4: long shots | 70° | 3.0 m | 3.0 m |
| 5: full training distribution | — | — | — |

The brief's last stage (banks, obstacles) has no counterpart yet: one object
ball, and no bank-shot goals.

**Runs** (SAC, SB3 defaults, positions, 5 seeds each, validation-suite
selection):

- absolute frame, 300k steps, with and without the curriculum (equal
  budget);
- ghost-relative frame, 100k steps, with the curriculum, compared with
  Experiment 2's `sac-ghost_relative`.

**Command.** `bash scripts/experiment4_curriculum.sh`, then `summarise_runs`
and `compare_to_baseline` on `runs/experiment4`.

**Results:**

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° |
|---|---|---|---|---|---|---|
| SAC absolute, 300k, no curriculum | 1.9% ± 0.7 | 1.4% ± 0.5 | 1.0% ± 0.5 | 1.9% | 1.3% | 0.9% |
| SAC absolute, 300k, curriculum | **19.6% ± 2.1** | 4.2% ± 0.7 | 1.8% ± 0.8 | 19.1% | 4.0% | 2.1% |
| SAC ghost-relative, 100k (Exp. 2) | 86.9% ± 0.7 | 83.3% ± 0.8 | 82.2% ± 0.9 | 82.5% | 58.6% | 41.5% |
| SAC ghost-relative, 100k, curriculum | **88.7% ± 0.3** | 84.3% ± 1.1 | 81.1% ± 1.5 | 84.0% | 61.6% | 42.6% |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% |

**Curriculum progress.**

- **Absolute frame:** all 5 seeds stayed at stage 0 for all 300k episodes.
  Their rolling stage-0 success rose from 6–9% to 62–68% and was still rising
  slowly; it never crossed the 0.8 gate.
- **Ghost-relative frame:** every seed reached stage 4. Advances came at
  about 5.5k, 8.5k and 14k episodes, then 61–85k for stage 4. No seed reached
  stage 5 within 100k.

**Paired with geometry** (exact McNemar per seed, $p < 0.05$),
ghost-relative with curriculum:

- *easy:* +6.1, better in 5/5 seeds;
- *medium:* −1.8, worse in 2/5;
- *hard:* −4.3, worse in 4/5.

**Interpretation.**

1. **The curriculum is the first intervention that makes the absolute frame
   learn.** Easy-suite success goes from 1.9% to 19.6%. Learning curves on
   the validation suite rise from 3% to 11%, where every earlier
   absolute-frame run stayed at 0–2%. On stage-0 shots the policy learned to
   pot two times in three: it learned the coarse "aim through the object ball
   towards a pocket" mapping that undirected exploration never found.
2. **The 0.8 gate was too strict for the absolute frame.** Success on the
   easiest stage plateaued below the threshold, so the policy never saw
   harder shots. That explains why the gains shrink from easy (19.6%) to
   medium (4.2%) and hard (1.8%). The gate, not the idea, limited this run.
3. **In the ghost-relative frame, the curriculum gives a small, consistent
   gain on easier layouts** (easy +1.8, medium +1.0 over no curriculum),
   with the lowest seed variance of any group (sd 0.3 on easy). It slightly
   hurts *hard* (−1.1), since the policy spent its steps on easier shots and
   never reached the full distribution. It does not close the precision gap
   diagnosed in Experiment 3.

**Failure modes.**

- Stage limits are measured to the constructed pocket. The easiest line of a
  constructed layout can be to another pocket, so stages are not perfectly
  pure (by design: the policy may choose either).
- Advancement compares a 2,000-episode rolling window with a fixed
  threshold. A plateau below the threshold freezes the curriculum: there is
  no time-based fallback and no mixing of earlier stages.

**Next.**

- The absolute frame with a lower gate (0.5–0.6), or one that advances on a
  plateau, plus a share of earlier-stage layouts to avoid forgetting.
  Expected to carry the coarse skill to harder shots.
- Milestone 8: the goal-conditioned environment and HER, the brief's other
  answer to sparse rewards.

---

## Experiment 5 — Curriculum gates for the absolute frame

**Hypothesis.** In Experiment 4 the absolute frame's curriculum froze on
stage 0, below the 0.8 gate (62–68%). Letting it advance, by a lower gate or
on a plateau, and reviewing earlier stages so they are not forgotten, carries
the coarse aiming skill to harder shots.

**Setup.** As in Experiment 4: SAC, absolute frame, positions, 300k steps,
5 seeds, validation-suite selection. The curriculum wrapper gains two
options:

- **plateau advance:** advance when the best windowed success has not
  improved by 0.02 for 15,000 episodes;
- **review:** 20% of episodes draw a layout from a random earlier stage.
  These episodes do not count towards advancing.

| Variant | Gate | Plateau advance | Review |
|---|---|---|---|
| Exp. 4 | 0.8 | no | 0% |
| A | 0.5 | no | 20% |
| B | 0.8 | yes (15k episodes, +0.02) | 20% |

**Command.** `bash scripts/experiment5_curriculum_gates.sh`;
`summarise_runs runs/experiment5`.

**Results** (mean ± sd over 5 seeds):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° | Stage reached |
|---|---|---|---|---|---|---|---|
| no curriculum (Exp. 4) | 1.9% ± 0.7 | 1.4% ± 0.5 | 1.0% ± 0.5 | 1.9% | 1.3% | 0.9% | — |
| gate 0.8 (Exp. 4) | 19.6% ± 2.1 | 4.2% ± 0.7 | 1.8% ± 0.8 | 19.1% | 4.0% | 2.1% | 0 |
| A: gate 0.5 + review | **32.9% ± 4.3** | 6.5% ± 1.6 | 2.6% ± 0.8 | 33.3% | 6.0% | 3.1% | 1 (at 74–100k episodes) |
| B: plateau + review | 30.1% ± 2.5 | 6.5% ± 1.4 | 2.6% ± 0.5 | 30.7% | 6.1% | 2.6% | 5, full distribution (at 168–246k) |

Learning curves on the validation suite: both variants rise steadily from
3% at 10k steps to 14–16% at 300k, still rising.

**Interpretation.**

1. **Unfreezing the curriculum helps on easy shots.** Easy success rises from
   19.6% to 30–33%, about 1.6 times Experiment 4. It is the best
   absolute-frame result so far, and 10–17 times the no-curriculum control.
2. **Reaching harder stages did not transfer to harder shots.** Variant B
   walked through every stage to the full distribution, yet does no better
   than A, which stopped at stage 1. Both stay at 6.5% on medium and 2.6% on
   hard.
   - The policy advanced on plateaus, not on mastery: it moved on without
     having learned the stage.
   - The gate is therefore no longer the bottleneck. Learning in the absolute
     frame is simply slow: validation success is still rising at 300k.
3. **Under noise, absolute-frame policies lose nothing** (A: 33.3% at
   σ = 0.25° against 32.9% without). They pot only shots with wide windows,
   so a 0.25° error rarely matters. Precise shots are what they have not
   learned.
4. **Compared with the ghost-relative frame** (82–89%), the absolute frame
   remains far behind after 300k steps and every curriculum tried. Supplying
   the geometric reference aim is worth more than any curriculum here.

**Next.** Experiment 6 tests HER, the brief's other answer to sparse rewards,
on a goal-conditioned environment whose goals (where the object ball ends
up) make every shot informative.

---

## Experiment 6 — Goal conditioning and Hindsight Experience Replay (Milestone 8)

**Hypothesis.** The goal is where the object ball ends up: a pocket mouth,
or the spot where it came to rest. HER relabels each shot as a success for
reaching wherever the ball actually went. That turns every shot into a
training signal for "hit the object ball in this direction", the coarse skill
the absolute frame failed to learn. HER should therefore help the absolute
frame a lot and the ghost-relative frame little.

**Setup.** `GoalShotEnv`: `SingleShotEnv` with Dict observations
(`observation`, `achieved_goal`, `desired_goal`). Goals are
$(x', y', \text{is\_pocket}, \text{scratch})$. A goal is met when the flags
match and the points are within 10 cm; a scratch scores −1, a met goal +1,
anything else −0.1. The desired goal is always the layout's target pocket.

- SAC with SB3's `MultiInputPolicy`.
- HER via SB3's `HerReplayBuffer`: 4 relabelled goals per transition,
  strategy "future". With one-step episodes it relabels each shot with its
  own outcome.
- With and without HER, on the *same* environment and reward, so HER is the
  only difference.
- Absolute frame for 200k steps; ghost-relative for 100k; 5 seeds each.

**Command.** `bash scripts/experiment6_her.sh`; `summarise_runs` and
`compare_to_baseline` on `runs/experiment6`.

**Results** (success = potted in any pocket without a scratch; mean ± sd
over 5 seeds):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° | Target-pocket success (easy / medium / hard) |
|---|---|---|---|---|---|---|---|
| absolute, no HER | 2.1% ± 0.7 | 1.0% ± 0.3 | 0.6% ± 0.4 | 2.4% | 0.9% | 0.7% | 1.5 / 0.5 / 0.3% |
| absolute, HER | 3.9% ± 1.8 | 1.7% ± 0.7 | 1.0% ± 0.6 | 4.0% | 1.9% | 0.9% | 3.2 / 1.1 / 0.5% |
| ghost-relative, no HER | 87.2% ± 0.7 | 84.5% ± 0.4 | 84.9% ± 0.4 | 83.0% | 60.4% | 42.5% | 86.2 / 83.5 / 84.4% |
| ghost-relative, HER | 87.6% ± 0.9 | 85.3% ± 1.4 | 84.8% ± 0.3 | 82.4% | 60.8% | 43.8% | 86.7 / 84.5 / 84.3% |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% | — |

**Paired with geometry** (exact McNemar per seed), ghost-relative:

| Group | easy | medium | hard |
|---|---|---|---|
| HER | +5.0, better in 5/5 seeds | −0.8, worse in 1/5 | −0.6, 0/5 either way |
| no HER | +4.6, better in 5/5 | −1.6, 0/5 either way | −0.5, 0/5 either way |

**Diagnostics** (absolute frame, best checkpoints):

- Legal first contact on *medium*: 19.5% with HER, 13.7% without.
- On *medium* layouts, the policy was asked to send the object ball within
  10 cm of a random spot (an `is_pocket = 0` goal, the kind HER relabels
  with). HER succeeds 1.6% of the time, against 0.9% without.

**Interpretation.**

1. **HER barely helps the absolute frame** (easy 2.1% → 3.9%). The
   diagnostics show it did not even learn the relabelled task: it reaches a
   requested spot 1.6% of the time and touches the object ball first only
   one shot in five.
2. **The cause is in the goal design, not in HER.**
   - About 80% of random shots never touch the object ball, so its
     "achieved" position is just its starting spot.
   - HER relabels those shots as successes for "leave the object ball where
     it is". Most of the hindsight signal rewards *missing* the ball, the
     opposite of the skill it was meant to teach.
   - This is the failure mode `papers.md` warned about (the achieved goal
     must be informative), in a form the design did not anticipate. The fix:
     a shot that never touches the object ball achieves no goal. That is
     Experiment 6b.
3. **In the ghost-relative frame, HER makes no measurable difference,** as
   expected: reward is already dense there (about 60% success from the start).
4. **The goal-conditioned ghost-relative policies reach geometry's level on
   medium and hard,** with no seed significantly worse on hard. That is
   better than `SingleShotEnv`'s SAC (Experiment 2: worse than geometry in 4
   of 5 seeds on both suites).
   - Two changes are confounded here: the policy now *sees* the target
     pocket's mouth (the desired goal is an input), and only the target
     pocket counts as success (`sparse_target`).
   - Separating them is a cheap follow-up. That the policy benefits from
     knowing which pocket its reference aim points at is consistent with
     Experiment 3: more task-relevant input helps.

**Failure modes.** As noted in point 2: the achieved goal for a missed shot
was degenerate.

### Experiment 6b — HER with contact-gated goals

**Hypothesis.** If a shot that never touches the object ball achieves no goal
(`require_contact`: a fifth goal component), HER stops rewarding misses, and
its relabelled successes teach "send the object ball there", which then
transfers to pocket goals.

**Setup.** As Experiment 6, absolute frame, HER, 200k steps, 5 seeds, with
`env.require_contact=true`. Control: Experiment 6's absolute no-HER group.
Without HER, gating changes nothing: desired goals are always pockets, which a
miss already fails.

**Command.** `bash scripts/experiment6b_her_contact.sh`.

**Results:**

| Policy | easy | medium | hard | Target-pocket success, easy | First contact, medium | Random-spot goals met |
|---|---|---|---|---|---|---|
| absolute, no HER (Exp. 6) | 2.1% ± 0.7 | 1.0% ± 0.3 | 0.6% ± 0.4 | 1.5% | 13.7% | 0.9% |
| absolute, HER (Exp. 6) | 3.9% ± 1.8 | 1.7% ± 0.7 | 1.0% ± 0.6 | 3.2% | 19.5% | 1.6% |
| absolute, HER, contact-gated | 2.6% ± 1.2 | 1.2% ± 0.4 | 0.6% ± 0.4 | 2.1% | 16.9% | 0.1% |

The validation curve stays at 1–2% throughout 200k steps.

**Interpretation.**

1. **Removing the degenerate relabels did not help.** The contact-gated
   policy is no better at pocketing and *worse* at the relabelled task (0.1%
   of random spots reached). The degenerate goals were not what held HER
   back.
2. **The hindsight task is as hard as the real one.** Sending the object ball
   within 10 cm of a given spot requires the right direction *and* the right
   distance, through the cushions. In the 2-D action space that is again a
   needle. HER's success in robotics relies on goal reaching being smooth:
   nearby actions reach nearby goals, over many steps. In one-shot precision
   shooting, nearby actions send the ball to very different resting places.
   Relabelling therefore adds many successes, but for goals as hard as
   the original, and the policy generalises from none of them.
3. **What a useful hindsight goal would look like.** It should depend
   smoothly on the action and determine success. The object ball's
   **departure direction** fits:
   - it is one-dimensional;
   - it is set by the contact alone, independent of speed and cushions;
   - potting into pocket $k$ is exactly "depart within pocket $k$'s window of
     directions".

   A direction goal would let HER teach the contact geometry (aim → departure
   direction) from every shot that touches the object ball.

**Next.**

- **Experiment 6c (proposed):** goals as the object ball's departure
  direction, with HER.
- **Experiment 6d (proposed):** separate the two changes behind the
  goal-conditioned ghost-relative gain (target-pocket input versus
  `sparse_target` reward).
- **Then Milestone 9,** elite-sample / advantage-weighted regression, aimed
  at the precision gap diagnosed in Experiment 3.

### Experiment 6c — HER with departure-direction goals

**Hypothesis.** The object ball's departure direction is set by the contact
alone and is smooth in the aim. As a hindsight goal it should let HER teach
the contact geometry (aim → departure direction) from every shot that touches
the object ball, which then transfers to pocket goals.

**Setup.** `DirectionGoalEnv`. Goals are
$(\cos\theta, \sin\theta, \text{potted}, \text{scratch}, \text{no\_contact})$:

- **desired goal:** the direction to the target pocket's mouth, with
  potted = 1;
- **achieved goal:**
  - after a pot, the direction to the mouth the ball entered (pocket goals
    stay exact);
  - after a contact without a pot, the departure direction;
  - after a miss, nothing.

Directions must agree within 1°. The simulator now reports the contacted
ball's velocity right after the first contact. SAC, absolute frame, 200k
steps, 5 seeds, with and without HER.

**Results:**

| Policy | easy | medium | hard | Validation at 200k |
|---|---|---|---|---|
| direction goals, no HER | 2.0% ± 1.0 | 0.9% ± 0.3 | 0.9% ± 0.3 | 1% |
| direction goals, HER | 4.2% ± 1.4 | 1.5% ± 0.6 | 0.6% ± 0.6 | 3% |

**Diagnostic.** On 300 *medium* layouts, each policy was asked for a random
departure direction (potted = 0):

| Policy | Contact with the object ball | Direction error given contact (median) | Within 10° | Within 1° |
|---|---|---|---|---|
| no HER | 15% | 104° | 3% | 0.4% |
| HER | 19% | 90° | 6% | 0.4% |
| uniformly random direction | — | 90° | 5.6% | 0.6% |

**Interpretation.**

1. **Direction goals do not rescue HER either.** Success doubles again from
   a negligible base (2.0% → 4.2% on easy). The HER policy's departure
   directions are no better than random: median error 90°, 6% within 10°.
   It has not learned the contact geometry it was relabelled to learn.
2. **Why: smooth is not the same as easy.** The departure direction depends
   smoothly on the aim, but steeply. Across the roughly 6° of aim (about 2%
   of the action range at a metre) that touches the object ball, the
   departure direction sweeps through about 180°. The policy must first hit
   the ball (it does so one time in five), then place its aim within that
   window to a fraction of a degree, computing the geometry from raw
   coordinates. Hindsight relabelling supplies more successes, but not an
   easier function to fit through a critic.
3. **Across Experiments 1–6c, one picture emerges.** In the absolute frame
   every variant (algorithm, curriculum, gate, HER with three goal types)
   fails or learns slowly. Supplying the geometric reference aim
   (ghost-relative frame) solves most of the task at once. What the
   policies lack is precise geometry, which off-policy RL with a scalar
   reward recovers poorly. This motivates Milestone 9: learning from the
   successful samples themselves (elite or advantage-weighted regression)
   rather than through a critic.

### Experiment 6d — Why the goal-conditioned ghost-relative policy is better

**Question.** In Experiment 6, the goal-conditioned ghost-relative policy
matched geometry on *medium* and *hard*, where `SingleShotEnv`'s SAC was
worse. Two things changed at once: (1) the policy sees the target pocket's
mouth, and (2) only the target pocket counts as success. Which matters?

**Setup.** A 2×2 in the ghost-relative frame, SAC, 100k steps, 5 seeds. The
two new cells use `SingleShotEnv` with
`reward=sparse_target, obs_mode=positions` and with
`reward=sparse, obs_mode=positions+target` (positions plus the target mouth).

**Results** (mean ± sd over 5 seeds):

| Target mouth as input | Target-only reward | easy | medium | hard | Source |
|---|---|---|---|---|---|
| no | no | 86.9% ± 0.7 | 83.3% ± 0.8 | 82.2% ± 0.9 | Exp. 2 |
| no | yes | 86.6% ± 0.4 | 83.6% ± 1.1 | 82.8% ± 1.0 | Exp. 6d |
| yes | no | 87.1% ± 0.3 | 83.0% ± 1.6 | 84.0% ± 1.1 | Exp. 6d |
| yes | yes | 87.2% ± 0.7 | 84.5% ± 0.4 | 84.9% ± 0.4 | Exp. 6 (`GoalShotEnv`, no HER) |

On *hard*, Welch's t-test against the first row:

- input only: +1.8 points, $t \approx 2.8$;
- reward only: +0.6, $t \approx 1.0$, not significant;
- both: +2.7, $t \approx 6$.

**Interpretation.** Most of the gain comes from **telling the policy which
pocket its reference aim points at.** With positions only, the network must
infer that from the coordinates. The target-only reward adds a little on top,
roughly additively. The effects are small (1–3 points) next to the 14–17
point gap to search. The last row also differs in network input format
(SB3's `MultiInputPolicy` over a Dict); that is a minor confound.

---

## Experiment 7 — Elite regression and AWR (Milestone 9)

**Hypothesis.** Experiments 1–6c pointed to precise aim as the bottleneck:
policies seemed unable to centre their aim in the potting window, as search
does. Learning from the successful shots themselves, by regression rather
than through a critic, should put the policy's mean at the centre of the
successes. That would close the gap to geometry on *medium* and *hard* and
improve robustness. It might also make the absolute frame learnable, since
every rare success directly moves the mean.

**Method** (`billiards_rl/agents/elite.py`). A Gaussian policy
$\pi_\theta(a \mid s) = \mathcal N(\mu_\theta(s), \sigma_\theta(s)^2)$
collects 1,000 shots per round, 10% of them uniform for exploration. It is
then fitted by weighted maximum likelihood,
$\max_\theta \sum_i w_i \log \pi_\theta(a_i \mid s_i)$, over all past shots
(200 gradient steps per round):

- **elite** (filtered imitation): $w_i = 1$ for a success, else 0;
- **AWR** (Peng et al. 2019): $w_i = \min(\exp((r_i - V_\phi(s_i))/\beta), 20)$,
  with $\beta = 0.2$ and $V_\phi$ regressed on rewards.

To keep successes unimodal (regression would average aims at different
pockets), the policy sees the target mouth (`positions+target`) and only the
target pocket counts (`sparse_target`). Experiment 6d showed this is also the
best SAC setting, so SAC is rerun in exactly this environment as the control.

**Runs** (5 seeds each): elite and AWR in the ghost-relative frame at 100k
steps; elite in the ghost-relative frame at 1M steps (elite runs are about
15× cheaper per step, so 1M steps take SAC's 100k-step wall-clock time);
elite and AWR in the absolute frame at 1M steps; SAC control at 100k.

**Command.** `bash scripts/experiment7_elite.sh`.

**Results** (mean ± sd over 5 seeds):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° | Time per run |
|---|---|---|---|---|---|---|---|
| SAC, ghost, 100k (control) | 86.9% ± 1.3 | **84.1% ± 0.6** | **84.0% ± 0.5** | 83.2% | 60.4% | 42.4% | 342 s |
| elite, ghost, 100k | 87.3% ± 0.8 | 79.4% ± 1.0 | 75.8% ± 2.7 | 82.4% | 55.8% | 42.2% | 31 s |
| AWR, ghost, 100k | 87.8% ± 0.4 | 79.7% ± 1.9 | 71.0% ± 3.1 | 82.9% | 57.2% | 40.6% | 46 s |
| elite, ghost, 1M | **88.3% ± 0.3** | 81.8% ± 0.8 | 79.1% ± 0.8 | 83.6% | 57.1% | 41.9% | 292 s |
| elite, absolute, 1M | 11.9% ± 3.0 | 7.4% ± 1.8 | 6.0% ± 1.9 | 11.5% | 7.6% | 5.9% | 274 s |
| AWR, absolute, 1M | 4.5% ± 0.4 | 2.4% ± 0.5 | 1.1% ± 0.5 | 4.4% | 2.6% | 0.9% | 375 s |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% | — |

**Paired with geometry** (no noise; seeds significantly better / worse):

| Policy | easy | medium | hard |
|---|---|---|---|
| SAC control | better 5/5 | worse 2/5 | 0/5 either way |
| elite, ghost, 1M | better 5/5 | worse 5/5 (−4.3) | worse 5/5 (−6.3) |
| AWR, ghost, 100k | better 5/5 | worse 5/5 (−6.4) | worse 5/5 (−14.4) |

**Diagnostic: where do the policies aim, and how hard?** On 200 *medium*
layouts, each policy's potting window for the target pocket was scanned in
0.02° steps, at the policy's own speed:

| Policy | Aim inside its window | Distance from window centre, when inside (median) | Window width (median) | Speed (median) |
|---|---|---|---|---|
| geometry | 84% | 0.08° | 0.62° | 1.05 m/s |
| SAC (seed 1) | 82% | 0.05° | 0.49° | 2.01 m/s |
| elite, 1M (seed 1) | 84% | 0.04° | 0.38° | 3.38 m/s |
| elite, 1M (seed 2) | 81% | 0.05° | 0.36° | 4.22 m/s |

**Interpretation.**

1. **The main hypothesis is rejected: aim centring was not the problem.**
   - Every policy that lands in its window is within 0.04–0.08° of the
     window's centre. Elite regression centres best, as designed, but
     centring was not what separated the policies.
   - Experiment 3's ablation attributed search's robustness to "aim centring
     only", but that search kept the geometry *speed*. Its window centre was
     found at a gentle speed, with a wide window.
2. **Speed decides window width, and the learned policies hit too hard.**
   - Geometry plays about 1 m/s, with 0.62° windows. SAC plays 2 m/s
     (0.49°); elite regression plays 3.4–4.2 m/s (0.36–0.38°).
   - Hard shots reduce throw and pot easy layouts reliably, so they dominate
     the successful samples. The policy carries "hit hard" over to long cuts,
     where it narrows the target. That is why elite and AWR lose most on
     *hard*.
3. **Elite regression is the best absolute-frame learner so far** on medium
   (7.4%) and hard (6.0%), with no curriculum, against at most 6.5% and 2.6%
   for SAC with curricula (Experiment 5). Caveat: it sees the target mouth
   and only target pots count, which no absolute-frame SAC run had. AWR
   fails in the absolute frame: with a success rate near 1%, its value
   baseline absorbs almost all of the signal.
4. **Elite methods are about 10× cheaper per step than SAC** (31 s against
   342 s per 100k steps), with less hyperparameter sensitivity, but they
   plateau lower in the ghost-relative frame.

### Experiment 7b — Elite regression under execution noise

**Prediction.** Under aiming noise, only shots with wide windows keep
succeeding, so the successes, and hence the regression, should move to
robust (slower) shots.

**Setup.** Elite, ghost-relative frame, 1M steps, $\sigma_\varphi = 0.25°$
during training (checkpoints selected under the same noise), 5 seeds.

**Results:**

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° |
|---|---|---|---|---|---|---|
| elite, 1M, clean | 88.3% | 81.8% | 79.1% | 83.6% | 57.1% | 41.9% |
| elite, 1M, noisy | 87.7% ± 0.8 | 76.7% ± 2.5 | 64.2% ± 5.4 | 82.7% ± 0.9 | 56.2% ± 1.2 | 39.4% ± 1.1 |

Under noise, the noisy-trained policy is worse than geometry on *medium* and
*hard* in 5/5 seeds. Diagnostic on the same 200 layouts: speed 3.35 and 4.23
m/s for seeds 1 and 2, window widths 0.40° and 0.38°. That is the *same*
speed, per seed, as the clean-trained runs (3.38 and 4.22 m/s).

**Interpretation.** **The prediction is rejected, and the identical speeds
explain why.**

- Each seed's preferred speed is fixed by its first successes and then
  self-reinforces: regressing on one's own successes keeps the policy where
  it already succeeds. This is self-imitation's familiar mode lock-in.
- Under noise, the rarer successes on hard layouts weigh even less against
  the easy ones, which succeed at any speed. So noise sharpened the
  imbalance instead of selecting robust shots.

**What Milestone 9 established.**

- The precision story of Experiments 3–6 needs revising: learned policies
  centre their aim well. They lose to geometry by choosing **speeds** that
  narrow the window, and the training signal comes mostly from easy states.
- Promising directions:
  - **choose speed explicitly for robustness:** make window width (expected
    success under noise) the objective, which the simulator can estimate per
    candidate;
  - **reweight states:** normalise successes by each state's difficulty, so
    hard layouts are not drowned out;
  - **keep speed exploration alive:** separate exploration noise for speed,
    or a minimum spread.

These lead naturally to the brief's next milestones. Cue-ball position play
needs speed control anyway. Candidate generation plus search can score
candidates by simulated robustness.

---

## Experiment 8 — Rewarding robustness directly

**Hypothesis.** Experiment 7 showed that learned policies lose to geometry by
hitting too hard, which narrows the potting window. Rewarding a shot by its
*expected* success under aiming error, rather than by the one outcome
played, pays directly for window width. The policy should learn gentler,
more robust shots.

Experiment 3's noise training targeted the same quantity with *one* noisy
sample per shot: unbiased but high-variance. Averaging 16 replays should make
the signal usable.

**Setup.** `SingleShotEnv` with `smooth_samples=16, smooth_sigma=0.25`: the
reward is the mean `sparse_target` reward of the intended shot replayed 16
times with $\varepsilon_\varphi \sim \mathcal N(0, 0.25°^2)$ (one batch call):

$$
\hat r(s, a) = \frac{1}{16}\sum_{k=1}^{16} r\big(s, a + (\varepsilon_k, 0)\big)
\approx \mathbb E_\varepsilon\left[r(s, a + \varepsilon)\right].
$$

- SAC, ghost-relative frame, target mouth as input, target-only reward,
  100k steps, 5 seeds.
- Checkpoints selected under 0.25° noise, the objective being robustness.
- Controls in the same environment: clean training (Experiment 7's
  `sac-ghost-target-100k`), and one noisy sample per shot (`noise=[0.25, 0]`).

**Command.** `bash scripts/experiment8_robust_reward.sh` (548 s per run, 1.5×
the clean runs, for the replays).

**Results** (mean ± sd over 5 seeds):

| Policy | easy | medium | hard | easy, σ=0.25° | medium, σ=0.25° | hard, σ=0.25° |
|---|---|---|---|---|---|---|
| SAC, clean (Exp. 7) | 86.9% ± 1.3 | 84.1% ± 0.6 | **84.0% ± 0.5** | **83.2%** | 60.4% | 42.4% |
| SAC, one noisy sample | 85.5% ± 0.7 | 83.8% ± 1.7 | 77.5% ± 2.4 | 81.6% ± 0.7 | 62.4% ± 0.9 | 44.2% ± 1.1 |
| SAC, robustness reward | 85.8% ± 0.6 | **86.4% ± 0.7** | 81.3% ± 1.3 | 81.5% ± 0.9 | **64.5% ± 0.4** | **46.3% ± 1.5** |
| geometry (Exp. 0) | 82.6% | 86.1% | 85.4% | 80.9% | 65.1% | 47.3% |

**Paired with geometry** (seeds significantly better / worse):

| Policy | Noise | easy | medium | hard |
|---|---|---|---|---|
| robustness reward | 0.25° | 0/5 either way | 0/5 either way | worse in 2/5 |
| robustness reward | none | better 5/5 | 0/5 either way | worse 5/5 |
| one noisy sample | 0.25° | 0/5 either way | worse 3/5 | worse 3/5 |

**Diagnostic** (200 layouts per suite, best checkpoints, the same window scan
as Experiment 7):

| Suite | Policy | Aim inside its window | Window width (median) | Speed (median) |
|---|---|---|---|---|
| medium | SAC, clean | 82% | 0.49° | 2.02 m/s |
| medium | SAC, robust (seeds 1, 2) | 86%, 87% | 0.58°, 0.60° | 1.34, 1.30 m/s |
| hard | SAC, clean | 80% | 0.28° | 2.02 m/s |
| hard | SAC, robust (seeds 1, 2) | 78%, 80% | 0.34°, 0.36° | 1.49, 1.48 m/s |

**Interpretation.**

1. **The hypothesis is supported.** The robustness reward teaches what it
   pays for:
   - speeds drop from 2.0 to 1.3–1.5 m/s;
   - windows widen by 18–28%;
   - under 0.25° of noise, success rises by about 4 points on *medium* and
     *hard* over clean training (64.5% against 60.4%, 46.3% against 42.4%).
2. **It is the first learned policy to match the geometry baseline under
   noise.** It shows no significant difference on *easy* and *medium*, and is
   worse on *hard* in 2 of 5 seeds. Without noise it reaches 86.4% on
   *medium*, the best learned *medium* result, level with geometry.
3. **Averaging replays is what makes noise training work.** One noisy sample
   per shot (Experiment 3's approach, repeated here in the same environment)
   is worse on every suite. The robustness signal is there, but a single
   Bernoulli outcome per shot is too noisy for the critic to rank shots by
   window width.
4. **The trade-off on hard layouts.** Slower long shots throw more, so their
   windows move further from the ghost-ball line. The robust policy's
   windows are wider, but it lands in them no more often (78–80%), and
   without noise it loses 2.7 points on *hard* to clean SAC. Wider windows
   help only if the aim follows them.
5. **This uses the simulator during training** (16 replays per shot) but
   never at decision time: the trained policy acts in one forward pass.
   Unlike the search baseline, it is a deployable policy.

**Next.** The remaining gap on *hard* (an aim correction that grows as shots
get slower) and the step to position play (Milestone 10) both need control
of where the cue ball goes, so spin and speed are next. Candidate generation
plus search (Milestone 12) can use the same robustness estimate to rank
shots at decision time.

---

## Experiment 9 — Position play (Milestone 10)

**Hypothesis.** With speed and spin in the action and a target for the cue
ball in the observation, SAC learns position play: potting while leaving the
cue ball near a target, beyond the geometry baseline, which ignores position.
Spin (follow, draw, side) should matter: it is how players control where the
cue ball goes.

**Setup.** `PositionShotEnv`:

- **Action** (4): aim correction (±4° around the ghost-ball line), speed,
  side spin, and follow/draw. Tip offsets are up to $0.525R$, the game's
  range.
- **Observation** (8): both balls, the target pocket's mouth, and the
  cue-ball target.
- **Targets are feasible by construction:** the geometric aim, played at a
  random speed (0.7–1.8× the friction model's) and spin until it pots in the
  target pocket without a scratch; the cue ball's resting spot is the
  target.
- **Reward:** −1 for a scratch, −0.1 for no target pot, and for a pot
  $1 + \max(0, 1 - d/0.5\,\mathrm{m})$, with $d$ the cue ball's distance to
  its target.
- **Position success:** a target pot with $d < 0.15$ m.
- **Suites:** `position` (1,000) and `position_validation` (600), with
  bands cycling easy, medium, hard, and targets stored and hashed.
- **Runs:** SAC (SB3 defaults), 200k steps, 5 seeds, with spin and with spin
  locked at zero; checkpoints chosen by mean validation reward.

**Baselines.**

- *geometry:* no spin, the friction-model speed.
- *position search:* aim ±1° × 6 speeds × 5 follow/draw × 3 side settings
  (about 3,700 simulated shots per layout); plays the target pot that leaves
  the cue ball nearest its target.

**Commands.** `bash scripts/experiment9_position.sh`;
`uv run python -m scripts.experiment9_baselines`.

**Results** (position suite; mean ± sd over 5 seeds):

| Policy | Target pot | Position success | Median cue distance after a pot | Target pot, σ=0.25° | Position, σ=0.25° |
|---|---|---|---|---|---|
| geometry | 92.0% | 29.6% | 0.25 m | 70.4% | 23.6% |
| SAC, no spin | **94.3% ± 0.4** | **35.8% ± 1.1** | 0.21 m | 72.2% ± 1.0 | **28.0% ± 0.6** |
| SAC, with spin | 93.4% ± 0.4 | 32.9% ± 1.5 | 0.22 m | 72.6% ± 0.7 | 26.2% ± 1.0 |
| position search | 100.0% | 98.8% | 0.011 m | 67.5% | 59.3% |

Validation reward rose from 0.6–1.1 to 1.37–1.44 over 200k steps and was
still rising slowly. The spin-locked runs started higher, because their
random spin is always zero.

**Diagnostic: does the spin policy use spin purposefully?** On 300 layouts:
its follow/draw output varies (sd 0.30, mean +0.2), but correlates with
whether the target lies beyond the object ball or behind it at only
$r = -0.06$ and $-0.02$ (seeds 1 and 2). It has not learned the basic rule:
follow to go forward, draw to come back.

**Interpretation.**

1. **SAC learns some position play, by speed alone.** Without spin it beats
   geometry by 6 points of position success and 2 of pot rate, and keeps the
   edge under noise (28.0% against 23.6%). Varying the pace moves the cue
   ball along its natural path towards the target.
2. **Spin was not learned.** Adding it made results slightly worse: a larger
   action space for the same budget, with no use made of it. Follow/draw is a
   simple rule for a player, but for the critic it is a 4-D landscape with
   narrow potting ridges. Position on top of potting adds a second, coupled
   precision requirement.
3. **Search shows how much is possible:** 98.8% position success with
   exact execution. The policy is about 63 points below it. That is the
   strongest argument so far for combining a learned proposer with simulated
   search (Milestone 12) rather than asking a single network for the exact
   shot.
4. **Search optimises position, not robustness.** Under noise it pots less
   than geometry or SAC (67.5% against 70–73%), because it picks precise
   shots. A practical player would trade some position for robustness, as
   Experiment 8 did for potting.

**Failure modes and caveats.**

- Targets come from shots along the geometric aim, so they favour
  geometry's natural cue-ball paths. That explains geometry's 30% position
  success with no intent at all, and makes the comparison conservative for
  the learned policies.
- 200k steps; the curves were still rising slowly.

**Next.** Milestone 11 (the hierarchy) and Milestone 12 (candidate shots plus
search). Experiment 9 suggests the search can do what the single network
could not: use spin and pace to place the cue ball, with the robustness
estimate of Experiment 8 to stay reliable under noise.

---

## Experiment 10 — A learned strategic layer (Milestone 11)

**Hypothesis.** On a table with several balls, *which* shot to play matters:
order, and the position each shot leaves. A strategic policy $\pi_H$ that
chooses the goal (ball, pocket), executed by a fixed shot policy $\pi_L$,
learns choices that clear more balls than the classical easiest-first rule.

**Setup.** `RunEnv` (`billiards_rl/hierarchy/`):

- **Rack:** a cue ball and 3 object balls; the player keeps shooting while
  potting, in any order, with no game rules. +1 per ball, −1 for a scratch.
- **Goals:** the (ball, pocket) pairs with a clear ghost-ball line and a cut
  under 75°, the 8 easiest.
- **Executors** ($\pi_L$), fixed:
  - geometry;
  - Experiment 8's robust SAC shot policy, which sees only the cue ball, the
    chosen ball and the target mouth (the interface built in Milestones
    6d–8).
- **$\pi_H$:** a candidate scorer $\ell_j = f_\theta([x_j, g])$ applied to
  each goal's features (cut, distances, ease, positions) plus global
  features, with a masked softmax over valid goals.
  - Permutation-equivariant: it scores goals, not slots.
  - Trained with REINFORCE and a mean baseline: 300 iterations × 64 racks,
    entropy 0.01. The best of 12 checkpoints on `run3_validation` is kept.
  - 5 seeds per executor; trained with exact execution.
- **Baselines:** random goal; easiest-first (the classical heuristic).
- **Suites:** `run3` (1,000 racks) and `run3_validation` (600), hashed.
  Evaluation uses identical noise draws per rack for every strategy, and
  paired per-rack differences with 95% bootstrap intervals.

**Command.** `uv run python -m scripts.experiment10_hierarchy` (about 15
minutes).

**Results** (mean balls potted per rack, out of 3, scratches counted −1; and
racks cleared):

| Executor | Aim noise | Random | Easiest-first | Learned $\pi_H$ (5 seeds) | Learned − easiest |
|---|---|---|---|---|---|
| geometry | 0° | 1.96 | 2.14 (58% cleared) | **2.43** ± 0.07 (69% cleared) | +0.29, better in 5/5 seeds |
| SAC $\pi_L$ | 0° | 1.47 | 2.08 (55% cleared) | **2.26** ± 0.03 (62% cleared) | +0.18, better in 5/5 seeds |
| geometry | 0.25° | 0.72 | **1.16** (17%) | 0.98 ± 0.03 (14%) | −0.19, worse in 5/5 seeds |
| SAC $\pi_L$ | 0.25° | 0.70 | **1.19** (20%) | 1.14 ± 0.02 (19%) | −0.05, worse in 1/5 seeds |

Validation curves rise over training, for example geometry seed 3:
2.07 → 2.55 balls.

**Diagnostic** (geometry executor, seeds 1 and 3; first shot of 500 racks):

- the learned strategy chooses the easiest goal only 45% of the time
  (mean ease rank 0.85–0.90);
- its cuts are similar (median 18–19° against 16°);
- its shots are much **longer**: cue and object ball travel 1.89 m against
  1.32 m (median).

**Interpretation.**

1. **The hierarchy works, and the strategic layer learns real strategy.**
   With exact execution, a policy trained only on balls potted beats
   easiest-first in every seed, for both executors. It clears 69% of racks
   against 58% (geometry) and 62% against 55% (SAC).
   - This is the brief's decomposition: $\pi_L$ unchanged from the
     single-shot milestones, $\pi_H$ trained on top.
   - It needed no change to $\pi_L$: Milestone 6d's target-pocket input was
     the right interface.
2. **The strategy is tuned to the execution it trained with.** It prefers
   longer shots that leave the next shot easier, which pays with exact
   execution. Under 0.25° of aim noise, error grows with distance
   ($\delta\psi \propto 1 + d_c/(2R\cos\theta)$), and the same choices clear
   fewer balls than easiest-first.
   - The SAC executor softens the loss (−0.05): it was trained to be robust,
     and its own shot choices already avoid the narrowest windows.
3. **Execution, not strategy, still dominates the numbers.** Noise of 0.25°
   halves the balls cleared (about 2.1 to 1.2) for every strategy. A strategy
   is only as good as the reliability it assumes.

**Next.** Train $\pi_H$ with the executor's noise, so it learns which goals
are reliable as well as useful. Then Milestone 12: candidate shots plus
search, evaluating the next state explicitly instead of learning it, with
Experiment 8's robustness estimate.

### Experiment 10b — The strategic layer trained under noise

**Hypothesis.** Trained with the executor's noise (0.25°), $\pi_H$ learns
which goals are *reliable* as well as useful, and beats easiest-first under
noise.

**Setup.** As Experiment 10, with `RunEnv(noise=(0.25, 0))` during training
and checkpoint selection under the same noise.
`uv run python -m scripts.experiment10_hierarchy --train-noise 0.25 --out runs/experiment10b`.

**Results** (balls per rack; learned = mean of 5 seeds):

| Executor | Aim noise | Easiest-first | Learned $\pi_H$, trained under noise | Seeds better / worse |
|---|---|---|---|---|
| geometry | 0° | 2.14 | 2.12 ± 0.04 | 0 / 0 |
| geometry | 0.25° | 1.16 | 1.16 ± 0.03 | 0 / 0 |
| SAC $\pi_L$ | 0° | 2.08 | 2.13 ± 0.02 | 2 / 0 |
| SAC $\pi_L$ | 0.25° | 1.19 | 1.22 ± 0.02 | 1 / 0 |

**Interpretation.** Trained under realistic noise, the strategic policy
converges to **about easiest-first**:

- no worse anywhere;
- marginally better with the learned executor (+0.03, 1 seed significant);
- no longer better with exact execution.

With these goals and this execution error, easiest-first is close to
optimal. Experiment 10's gain came from assuming perfect execution, and a
strategy that learns the true reliability rediscovers the heuristic.
Choosing *goals* alone has little left to give. The next lever is choosing
the *shot* within a goal, which is what search does (Experiment 11).

### Experiment 11 — Candidate shots plus sampled expectimax (Milestone 12)

**Hypothesis.** Experiment 10b showed that choosing *goals* has little
left to give. Choosing the *shot* by simulating it under the player's own
execution noise, and valuing the position it leaves, should lift the run
length a lot.

**Method.** `ExpectimaxPlayer` (`ml/billiards_rl/search/expectimax.py`).

1. **Candidates.** For every goal the proposer (π_L) gives a shot. The
   search varies its speed (×0.8, 1, 1.25) and follow/draw
   ($b \in \{-0.3, 0, 0.3\}$), which gives up to 72 candidates per turn.
2. **Chance nodes.** Each candidate is replayed $K = 16$ times with
   Gaussian aim error of the player's $\sigma$. The search draws its own
   samples, not the evaluation's draws.
3. **Value.** The action value is

$$
Q(s, a) = \frac1K \sum_{k=1}^{K} \Big[ r(s, a + \varepsilon_k) + \mathbb 1[\text{turn kept}]\, \max_{g'} \hat p(s'_k, g') \Big],
$$

where $\hat p$ is the next goal's simulated pot rate over 4 replays of its
geometry shot. Depth 1 omits the second term.

With exact execution ($\sigma = 0$) there is a single sample, and the
search's model is then **exact**: it sees the outcome of the shot it will
play. Those rows measure an upper bound on what candidate selection can do,
not skill under uncertainty.

**Setup.**
- `uv run python -m scripts.experiment11_search`.
- The `run3` suite (1,000 racks).
- Paired per rack with easiest-first using the same proposer as executor and
  the same evaluation noise draws.
- 95% bootstrap intervals of the mean difference.
- Cost (smoke test, M3 Pro, one thread): about 0.02 s per rack at depth 1 and
  0.09 s at depth 2.

**Results** (balls per rack; cleared = all 3 potted):

| Proposer | Aim noise | Easiest-first | Search d=1 | Search d=2 | d=2 − easiest [95% CI] | Cleared: easiest → d=2 |
|---|---|---|---|---|---|---|
| SAC $\pi_L$ | 0° | 2.08 | 2.84 | 2.93 | +0.85 [0.78, 0.93] | 55% → 95% |
| SAC $\pi_L$ | 0.25° | 1.19 | 1.59 | **1.84** | **+0.65 [0.58, 0.74]** | 20% → 45% |
| geometry | 0° | 2.14 | 2.91 | 2.99 | +0.85 [0.78, 0.93] | 58% → 99.5% |
| geometry | 0.25° | 1.16 | 1.60 | **1.78** | **+0.62 [0.54, 0.69]** | 17% → 41% |

For reference, the learned $\pi_H$ trained under noise (Experiment 10b)
reaches 1.22 balls with the SAC executor at 0.25°.

**Interpretation.**
- **Search is the largest gain of the project so far.** Under realistic noise
  it adds +0.62 to +0.65 balls per rack (+53% to +55%) over the heuristic, and
  more than doubles the share of cleared racks. Every interval is far from 0.
- **Depth matters under noise.** Depth 2 adds +0.25 (SAC) and +0.18
  (geometry) over depth 1. Valuing the leave pays most when execution is
  imperfect, because the leave decides whether the next shot is easy enough
  to survive the noise.
- **Most of the depth-1 gain** comes from the speed and spin variations and
  from evaluating goals by simulation instead of ease.
- **The proposer matters little once search is on.** SAC and geometry
  proposers end within 0.07 of each other: the search corrects the proposals.
  The learned π_L's value is speed (one forward pass) rather than better
  shots, which matters when the branching factor grows (full racks, 15 balls).
- **Limits.**
  - The leaf value is a hand-made heuristic (best next-shot pot rate), not a
    learned value of the whole rack.
  - The search's noise model equals the true one; a mismatched model is
    untested.
  - Three balls, no rules and no opponent.

  Replacing $\hat p$ with a value network trained on the search's own games
  (self-play / expert iteration) is the next milestone.

### Experiment 12 — Expert iteration of a learned rack value (Milestone 13)

**Hypothesis.** Experiment 11's leaf is hand-made: the pot rate of the best
next shot. A value network trained on the search's own racks estimates the
value of the *whole rest of the rack*, so it should rank the positions a shot
leaves better. Iterating this (search ⇒ data ⇒ value ⇒ better search) should
keep improving the player.

**Method.**
- **Value network.** $V_\theta(s) \approx \mathbb E[G_t \mid s_t = s]$, with
  $G_t$ the balls still to be potted minus scratches, under the search player
  and its noise. It's a DeepSets encoder over object balls (mean- and
  max-pooled $\phi$). Per-ball features: position, offset and distance to the
  cue ball, the best ghost-ball ease over its open pockets, and the share of
  pockets open to it.
- **Expert iteration.** Generation 0 is Experiment 11's depth-2 search with
  the geometry proposer. For $k = 1..4$:
  1. fit $V_k$ by MSE on the Monte Carlo returns of the 8,000 racks that
     generation $k-1$ played (about 19k states);
  2. let the search with leaf $V_k$ play 8,000 new racks.

  The value is

$$
Q_k(s, a) = \frac1K \sum_{k'} \Big[ r(s, a + \varepsilon_{k'}) + \mathbb 1[\text{turn kept}]\, V_{k-1}(s'_{k'}) \Big].
$$

  Search is the improvement step and regression is the evaluation step, i.e.
  approximate policy iteration. This is the single-agent form of AlphaZero's
  loop: RunEnv has no opponent, so there is no adversarial self-play yet.
- **Settings.** 0.25° aim noise everywhere; 3 seeds.

**Setup.**
- `uv run python -m scripts.experiment12_selfplay` (about 40 min on 10 cores).
- Every $V_k$ player is evaluated on `run3` with Experiment 11's noise draws,
  paired per rack with generation 0.
- The headline is generation 4, fixed in advance.
- `results.json` caveat: in this run, `train_states` holds the number of
  states that generation *played*. A late label fix now stores the fitted
  count, and the two differ by under 2%.

**Results** (balls per rack; generation 0 scores **1.780**, 41.4% cleared):

| Generation | Seed 1 | Seed 2 | Seed 3 | Mean difference vs gen 0 | Value $R^2$ (held out) |
|---|---|---|---|---|---|
| 1 | +0.014 | +0.008 | −0.005 | +0.006 | 0.26–0.31 |
| 2 | +0.005 | −0.005 | +0.006 | +0.002 | 0.27–0.31 |
| 3 | −0.007 | +0.011 | −0.012 | −0.003 | 0.26–0.27 |
| **4** | −0.019 | −0.001 | −0.025 | **−0.015** | 0.25–0.30 |

- Every 95% interval contains 0; the widest is about ±0.05.
- Generation 4 scores 1.765 balls per rack, against 1.780 for generation 0.
- The self-play rack means stay between 1.78 and 1.85 across all generations.

**Diagnostic.** I took 821 states reached after a kept turn (where the leaf
is used), from 600 fresh racks played by generation 0, and measured how much
of the remaining return each leaf explains.

| Leaf | $R^2$ on $G_t$ |
|---|---|
| hand-made $\hat p$ (best next-shot pot rate), linear | 0.22 |
| $\hat p \times$ balls left, linear | 0.34 |
| learned $V$, seed 1, gen 1 / gen 4 | 0.33 / 0.32 |

44% of these states have a single ball left, where the rest of the rack *is*
the next shot.

**Interpretation.**
- **A null result.** The learned value matches the hand-made leaf and does not
  beat it, and iterating does not compound. Generations 1–4 are flat within
  ±0.025 balls per rack.
- **Why: the hand-made leaf is already as informative.**
  - On three-ball racks a kept turn leaves one or two balls, so "how makeable
    is the next shot" is nearly the whole future.
  - The learned value explains no more of the return than $\hat p$ scaled by
    the balls left (0.32 vs 0.34), and the two are correlated (0.66–0.70).
  - The return is mostly execution luck: about 70% of its variance is not
    predictable from the state by either leaf. More data or generations then
    refines the estimate of a quantity that's already known.
- **What it did buy is speed.** The learned leaf replaces the leaf
  simulations with one batched forward pass. On one thread a rack takes about
  0.1 s instead of 0.5 s. That was measured on 50–100 racks, so it is
  indicative only.
- **Where a learned value should matter:**
  - longer horizons: more balls, so the leave determines several future shots;
  - rules with an opponent: safeties and the value of handing over the table;
  - large branching factors, where per-leaf simulation is too costly.

  These are 8-ball/9-ball positions with rules, which need the rules engine in
  the Python bindings. That is the remaining step of the brief's final
  milestone (adversarial self-play).
