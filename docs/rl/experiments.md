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
