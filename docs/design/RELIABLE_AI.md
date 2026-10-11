# Architecture and reliability experiment

## What the game actually uses

The bundled AI already uses a **relational Transformer**, not a linear model
or a plain multilayer perceptron. Its three attention blocks learn interactions
between balls, rules and both players' execution profiles. See
[ADAPTIVE_AI.md](ADAPTIVE_AI.md) for the shipped architecture and training.

Shot proposals and collision simulation remain geometric. Exact physics and
the referee check shots before execution. The released evaluator contributes
25% of the outcome score, with terminal results and foul safeguards retained.
Thus the neural architecture is modern, while the overall decision system is
hybrid. An architecture name cannot guarantee shot accuracy, prediction
accuracy, or a higher match win rate.

## Implemented alternative

`ml/billiards_ml/reliable_model.py` implements a separate, trained candidate:

- Three independently initialized, whole-frame bootstrap members; 368,517
  trainable parameters in total.
- Each member has two geometry-aware Transformer blocks, width 64 and four
  attention heads. Continuous relative ball distances bias attention.
- RMSNorm and SwiGLU feed-forward layers, with context-conditioned attention
  pooling over the ball set and masked pooling over legal targets.
- The same 256 C++ features and opponent conditioning as the released model.
  Absent balls are zeroed before arithmetic and masked in attention; token
  permutations and empty tables are tested.
- Frame-winner, legal-pot and foul training heads. Runtime exports ensemble
  mean frame-win probability and member probability standard deviation.
- A bounded scalar temperature fitted on separate calibration frames, embedded
  in the ONNX graph. The fitted temperature for this run was 1.0.

This is a custom graph Transformer, informed by structural attention work such
as [Graphormer](https://arxiv.org/abs/2106.05234). Independent models follow the
[deep ensembles](https://arxiv.org/abs/1612.01474) approach; probability
calibration follows [temperature scaling](https://arxiv.org/abs/1706.04599).
Model disagreement is a diagnostic, **not** a certified error bound or a
guaranteed detector of unfamiliar positions. It is not used to adjust difficulty.

## Training and measured outcome

The candidate trained locally on the existing mixed-skill league, without
random-action players: 2,880 training frames and 126,164 positions. The frozen
validation frames were divided into 180 checkpoint-selection frames and 179
calibration frames, with 359 existing test frames kept separate. Each member
used its own initialization and frame-bootstrap seed (142, 314, 2718), AdamW,
cosine decay, gradient clipping, auxiliary shot losses and early stopping.
Targets were observed Monte Carlo frame winners. Test data never entered
training or temperature fitting.

Because the existing test set had already been reported for the previous
release, a second evaluation used **900 fresh classical-search matches** in
all three disciplines and all 25 ordered skill pairs, with execution-noise
jitter. Neither neural candidate selected the observed positions. All matches
completed; 898 yielded pre-shot observations, totalling 38,352 positions.
Two matches ended on the break. The fresh seed domain was 77 million, separate
from training and prior match evaluations.

| Metric | Current Transformer | New ensemble |
| --- | ---: | ---: |
| Existing test frame-winner accuracy | 66.09% | 64.85% |
| Fresh frame-winner accuracy | 63.24% | 62.18% |
| Fresh log loss (lower is better) | 0.64064 | 0.64325 |
| Fresh Brier score (lower is better) | 0.22479 | 0.22607 |
| Fresh ECE, 15 bins (lower is better) | 0.02525 | 0.01497 |
| Single-position CPU inference | 0.154 ms, prior benchmark | 0.315 ms |

Calibration improved in aggregate, but predictive accuracy and proper scoring
rules worsened. Per-game fresh accuracy was 66.26% / 57.57% / 61.43% for the
ensemble versus 65.51% / 59.49% / 63.55% for the current model (8/9/10-ball).
It improved 8-ball and regressed in the rotation games.

A bootstrap clustered by whole frame gave a mean per-frame accuracy change of
-0.68 percentage points, with a 95% interval of [-1.60, +0.20] points. This
uses equal frame weighting; the table above weights individual positions.
Neither establishes an accuracy gain. Frame-winner prediction is not pot
success and is not a match win rate.

**The candidate failed the release gate and was not installed.** The game
retains the more accurate Transformer and its opponent adaptation. Large
head-to-head strength and adaptation evaluations of the candidate were skipped
after the prediction gate failed. A 12-frame native arena run validated ONNX
loading and actual planning only; it is not strength evidence.

The trained candidate weights, checkpoints, detailed metrics and failed gate
remain in ignored `ml/runs/reliable/`. The report in
`docs/benchmarks/reliable-ai.json` preserves the measured results and artifact
hashes without bundling the rejected weights.

## Runtime and release safeguards

The native ONNX loader validates feature names, tensor types, supported feature
width, batch/output shapes, finite probability bounds and optional ensemble
disagreement. It probes the model during loading. The planner runs on its
worker thread; if learned evaluation throws during a decision, it retries the
whole decision with physics search, including placement and referee choices.

`reliable_validate.py` refuses installation unless fresh accuracy, log loss,
Brier score, calibration, per-game regression, artifact identity and latency
checks pass. A prediction failure prevents asset writes. Subsequent gates also
require paired match evaluations against both the current model and classical
search, using new seeds, plus tested adaptive response with matching estimator
and calibration hashes. These are engineering limits, not mathematical
performance guarantees.

Validation: Release build with warnings treated as errors, both native test
suites, 23 relevant Python tests, both-output PyTorch/ONNX parity (maximum
difference 1.49e-7), and the native ensemble arena smoke run. Runtime fallback
was compiled and reviewed; a live inference fault was not injected into the UI.

## Reproduce

After the league setup and data generation in ADAPTIVE_AI.md, run from `ml/`:

```bash
uv run python -m billiards_ml.reliable_train --epochs 12 --threads 4
uv run python -m billiards_ml.reliable_validate predictions --frames 900 --workers 6
uv run python -m billiards_ml.reliable_validate install
```

The recorded run is expected to refuse installation at the prediction gate.
For a future candidate that passes, evaluate the fixed model before installation:

```bash
uv run python -m billiards_ml.reliable_validate matches --racks 50 --workers 6
uv run python -m billiards_ml.reliable_validate adapt --racks 8 --workers 6
uv run python -m billiards_ml.reliable_validate install
```

Do not tune on release evaluation data. Later experiments need a new fresh
prediction seed and new match phases; the fixed domains here identify this run.
Use `--seed` for fresh prediction data and `--phase` for paired match tests.
