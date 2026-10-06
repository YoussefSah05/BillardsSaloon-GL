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
