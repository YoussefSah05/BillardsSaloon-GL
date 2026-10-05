# Billiards Saloon — Intelligence

Status: design, milestone M9. Last revised 2026-10-05. Companion to `GDD.md`.

## Goal

Make Billiards Saloon feel like a premium, intelligent game: opponents that play
like distinct people, a coach that teaches, difficulty that fits the player, and
an endless supply of good challenges. One family of self-play-trained models
powers all four. Product quality comes first; the training pipeline is also the
project's AI/ML showcase.

## Why not pure end-to-end RL

Pool is a hard fit for end-to-end deep RL:

- Actions are continuous (angle, speed, tip offset, elevation) and a pot often
  needs sub-degree accuracy.
- A shot's real value is the position it leaves, several shots later.
- Rewards are sparse (frames are won or lost) and the opponent moves between turns.

The strongest computer pool players (PickPocket and CueCard, winners of the
Computer Olympiad billiards events) combine three parts:

1. a geometric generator of candidate shots (ghost ball for each target and
   pocket, banks, kicks, safeties);
2. simulation of each candidate under execution noise to estimate success;
3. an evaluator of the resulting table position.

Self-play learning fits into this, AlphaZero-style: networks learn the
evaluator and a prior over shots; search stays at decision time for precision.
The event-based simulator (M3) is deterministic and fast, which suits
self-play.

## Architecture

```
C++ (shipping)                                Python (training, ml/)
───────────────────────────────────────       ─────────────────────────────────────
bs_sim  (event simulator, rules)  ◄── pybind11 ──  billiards_sim module
src/ai/                                          ml/env/       Gymnasium environment
  candidate generator                            ml/agents/    search + networks
  noisy-execution search                         ml/training/  self-play, expert iteration
  ONNX Runtime inference  ◄── assets/models/*.onnx ── ml/export/ ONNX export + parity checks
  coach, skill estimator, challenge generator    ml/eval/      Elo ladder, metrics
```

- The same C++ simulator and rules run in training and in the game; Python
  never reimplements physics.
- Models are small (CPU inference in milliseconds) and optional: without them
  the game falls back to the classical AI from M8.

## Environment

- **Observation:** the ball set, each ball as (x, y, potted, is-legal-target,
  is-own-group, is-opponent-group, is-cue, is-eight/nine); plus rules context
  (discipline, groups assigned, ball in hand and its zone, consecutive fouls,
  shot clock).
- **Action:** shot parameters (angle φ, speed V, tip offsets a and b, elevation θ).
  Agents usually act as *candidate index + continuous residual*, which keeps
  learning tractable.
- **Execution noise:** explicit and per player: Gaussian error on φ, V, a, b, θ,
  so skill is a parameter rather than a different model.
- **Transitions:** one call simulates a whole shot and returns the final table
  plus the referee's verdict.

## Networks

- **Encoder:** permutation-invariant over balls (DeepSets or a two-layer
  transformer), so ball order does not matter and any discipline fits.
- **Value head:** probability that the player to move wins the frame.
- **Policy head:** prior over candidate shots, plus Gaussian refinements of
  their parameters.
- **Personality conditioning:** an input vector (risk tolerance, safety
  preference, power style, break style). One model plays every Tour pro.

## Training

Expert iteration:

1. **Bootstrap:** games from the classical M8 AI give initial value targets
   and candidate preferences.
2. **Self-play:** search, guided by the current networks, chooses shots under
   each player's noise model.
3. **Learn:** the value head fits game outcomes (Monte Carlo returns, then
   TD(λ)); the policy head fits the shots search preferred.
4. **Gate:** a new model replaces the old one only after it wins on the Elo
   ladder.

Compute is this Mac: self-play across CPU cores with multiprocessing, network
training on MPS. The first task is a simulator throughput benchmark
(shots per second per core), which sets the size of every experiment.

## Player-facing features

| Feature | How the models are used |
|---------|-------------------------|
| **AI pros with personalities** | Skill comes from a calibrated noise model, style from the conditioning vector. Each Tour pro is a profile: name, noise, style, break habits. |
| **AI Coach** | After any shot: the shot the AI would have played and why (pot chance, position value); a heatmap of where the cue ball should have finished (value network evaluated over a grid of cue-ball spots); shot difficulty as a percentage under the player's own noise. Text comes from templates over these numbers. |
| **Adaptive difficulty** | A Bayesian estimate of the player's noise, from their executed shots versus the intended line, becomes a rating. Quick Match and drills choose opponents and layouts near it. |
| **Generated challenges** | Search over random and mutated layouts; keep those the solver clears at a target skill; rate them by solve probability. Feeds drills, trick shots and the daily challenge. |

## Shipping

- Export to ONNX; ONNX Runtime runs inference on the AI worker thread.
- Decision budget: 50 ms or less on the minimum-spec machine.
- Missing, corrupt or slow models fall back to the classical AI with no player-visible error.

## Evaluation

- Elo ladder by round robin: classical AI, value-network-only, full agent.
- Pot %, run-out %, break-and-run %, safety success, average decision time.
- Tier ordering: each difficulty tier beats the one below it.
- Human feel, judged by play-testing: no superhuman safeties from "easy" pros,
  plausible misses.
- Parity: Python bindings reproduce C++ trajectories exactly; ONNX outputs
  match PyTorch within tolerance.

## Reproducibility

- `ml/` is a uv project with pinned dependencies, one config per experiment,
  fixed seeds and TensorBoard logs.
- Every shipped model has a model card in `docs/ml/`: training data and budget,
  metrics, known weaknesses.
- Training checkpoints go to the Hugging Face Hub; only shipped ONNX files live
  in the repository.
- A technical report in `docs/ml/` covers the method, ablations (learned value
  versus hand-tuned evaluator) and results.

## References

- Smith, *PickPocket: A computer billiards shark*, Artificial Intelligence 171 (2007).
- Archibald, Altman, Shoham, *Analysis of a Winning Computational Billiards Player*, IJCAI 2009 (CueCard).
- Silver et al., *Mastering the game of Go without human knowledge*, Nature 2017 (AlphaZero-style expert iteration).
- Anthony, Tian, Barber, *Thinking Fast and Slow with Deep Learning and Tree Search*, NeurIPS 2017 (expert iteration).
- Kiefl, *Pooltool*, JOSS 2024 — simulator design and RL-ready interfaces.
- *CueTip: An Interactive and Explainable Physics-aware Pool Assistant*, arXiv 2501.18291 — language explanations for coaching.
