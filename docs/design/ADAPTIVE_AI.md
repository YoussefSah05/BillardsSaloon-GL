# Trained, opponent-conditioned AI

The playable opponents use a hybrid of geometric shot search and a trained
relational value network. The network evaluates simulated outcomes; search
supplies the precision needed to pot a ball. It is not a random-action policy.
`assets/models/adaptive_ai.json` is the model card with the actual training,
held-out prediction, inference and match-evaluation results. The ONNX weights
are bundled at `assets/models/adaptive_value.onnx`.

This is already a Transformer architecture. A separately trained gated graph
Transformer ensemble was evaluated on fresh matches and rejected because its
prediction accuracy was lower; the current release remains selected. See
[RELIABLE_AI.md](RELIABLE_AI.md) for the experiment and runtime safeguards.

## Model

`ml/billiards_ml/adaptive_model.py`: 132,951 parameters, trained in PyTorch.

- **256 shared C++ features:** 16 ball slots × 14 features, 16 rules features,
  and 8 execution/personality features for each player.
- **Ball features:** the original presence, position, identity and legal/group
  flags, plus open-pot ease, open-pocket fraction, cue distance, nearest-ball
  distance, cushion clearance and nearest-pocket distance.
- **Player features:** aim, power and spin error, safety preference, power
  style, break power, search sample count and a reserved constant channel.
- **Encoder:** 64-dimensional ball and rules/player tokens, three pre-norm
  residual attention blocks, four heads, 128-unit GELU feed-forward blocks.
  A small network turns relative x/y distances and separation into an
  attention bias. The context token has no spatial bias.
- **Readout:** context-token representation, masked means over present balls
  and legal targets, and global context feed a 128-unit head. A direct skill
  projection also feeds the value logit.
- **Heads:** frame-win probability, next-shot legal-pot probability and foul
  probability. The latter two supply auxiliary training losses; the exported
  runtime graph only exposes frame-win probability.

Ball identity travels with the token, rather than its storage position. The
encoder is invariant to ball-storage permutations. Pocketed tokens are masked
in attention and pooling. Empty tables remain finite.

## Training and evaluation

Generation zero plays purposeful classical search across all 25 ordered pairs
of Club, Regional, two Pro styles and Champion, balanced across all three
disciplines. Small execution-noise perturbations cover intermediate ability.
Both perspectives are recorded before shots/placements and labelled with the
frame winner. Next-pot/foul labels are masked to the player who actually acts.
Unfinished frames are counted and excluded from training.

Subsequent generations mix classical and learned search independently in each
seat. This retains a population of opponents rather than converging solely to
one mirror opponent. Training uses AdamW, gradient clipping, weight decay,
cosine learning-rate decay, and validation checkpoint selection with early
stopping. After warm-up, the value target mixes 70% Monte Carlo outcomes and
30% TD(lambda=0.7) targets from a frozen prediction snapshot. Targets stay in
the same player's perspective across turn changes.

Train/validation/test are split 80/10/10 **by frame**. Membership is saved and
frozen across generations, so a warm-started model cannot have already trained
on the next generation's test partition. The best existing model is retained
when a later generation does not improve validation loss.

Match tests compare the same Pro execution profile with classical or learned
scoring against Club, Pro and Champion opponents in all three games. Each rack
is played with both seats/breaks; baseline and learned runs share seeds.
Tuning racks and final-evaluation racks have different seed domains. Results
include raw paired outcomes, unfinished counts and a paired bootstrap interval.
A constant 50% frame-value predictor is also reported on held-out positions.

The installer checks predictive improvement, actual opponent conditioning,
ONNX numerical parity, CPU latency, match sample count and bounded regression
against classical scoring. These are explicit engineering release gates, not
a claim that any statistically uncertain win-rate increase is proven strength.
Learned scoring is blended with the classical score, keeping exact terminal
results and classical foul safeguards. There is no online neural training in
the rendering loop.

## Adaptive play

Choose **ADAPTIVE OPPONENT** in Quick Match, or launch with
`--screen game --opponent adaptive`. At first it assumes an intermediate player.
Recognisable direct-pot attempts update a smoothed, difficulty-adjusted skill
estimate. A Bayesian filter compares each result with five skill hypotheses.
Its monotone pot/foul likelihoods are fitted on 4,561 attempted pots from 300
additional classical matches, with separate intercepts for each discipline.
Fouls provide negative evidence. Breaks and legal safeties do not count as
missed pots. Gentle forgetting allows later changes in ability, and smoothing
bounds a single observation's effect. Simulated skill response is validated;
the accuracy of a human's inferred rating has not been established.

Estimated skill changes aim/power/spin precision and the number of search
samples for the Adaptive opponent. It preserves that opponent's playing
personality. Named opponents retain their configured execution difficulty.
All learned opponents receive the estimated opposing skill as model input.
Planning and execution use the same frozen profile for a whole shot; later
human results only affect subsequent decisions.

Evidence persists across frames in a race, resets for a new match/rematch,
and is never collected again from replay playback. It stays in memory;
human play is not uploaded or written to training files.

## Reproduce locally

From the project root:

```bash
cd ml
uv sync
cd ..
cmake -B build-py -DCMAKE_BUILD_TYPE=Release -DBS_BUILD_PYTHON=ON \
  -DBS_WITH_ONNXRUNTIME=OFF -DBS_BUILD_TESTS=OFF \
  -DPython_EXECUTABLE="$PWD/ml/.venv/bin/python"
cmake --build build-py -j6 --target billiards_sim
cd ml
uv run python -m billiards_ml.adaptive_train generate --frames 2400 --workers 6 --seed 42
uv run python -m billiards_ml.adaptive_train train --epochs 25 --threads 4 --seed 42
uv run python -m billiards_ml.adaptive_train generate --frames 1200 --workers 4 \
  --generation 1 --model runs/adaptive/value.onnx --weight .25 --seed 42
uv run python -m billiards_ml.adaptive_train train --generation 1 --epochs 18 \
  --threads 4 --lr .0001 --seed 42
uv run python -m billiards_ml.adaptive_train evaluate --racks 30 --weight .25 --phase 0
uv run python -m billiards_ml.adaptive_train evaluate --racks 30 --weight 0 --phase 0
# Select the blend on tuning racks; use that same weight for final evaluation.
uv run python -m billiards_ml.adaptive_train evaluate --racks 50 --weight 0 --phase 1
uv run python -m billiards_ml.adaptive_train evaluate --racks 50 --weight .25 --phase 1
uv run python -m billiards_ml.adaptive_train calibrate --frames 300 --workers 6
uv run python -m billiards_ml.adaptive_train adapt --racks 8 --workers 6 --weight .25
uv run python -m billiards_ml.adaptive_train install --weight .25
```

The installer refuses mismatched model hashes or failed release checks. Local
data, checkpoints and run logs remain under ignored `ml/runs/adaptive/`; only
the exported model and model card are bundled. Build with
`-DBS_WITH_ONNXRUNTIME=ON` (the new default). An existing build cache with the
option off needs it explicitly enabled. Without runtime/model support the
game uses classical search; Adaptive execution still works.

## Bundled release results

- 3,600 league matches across two generations; 157,678 recorded positions.
  The selected generation-one checkpoint uses 2,880 training frames; 359
  frames each are reserved for validation and test. Two generation-one matches
  ended on the break and therefore produced no pre-shot records.
- Held-out win-probability log loss **0.63075**, compared with **0.63338** for
  a regularised linear model on the same features and **0.69315** for a
  constant predictor. This small predictive gain is not a proven strength gain.
- Single-position ONNX CPU inference **0.154 ms** in the recorded benchmark;
  maximum PyTorch/ONNX probability difference **1.49e-7**.
- The same held-out boards receive, on average, an **18.4 percentage point**
  higher win estimate against weak rather than strong execution, with all
  other tested inputs held fixed.
- Final paired match comparison: **900 learned and 900 classical frames**,
  zero unfinished. Overall win-rate difference **-0.89 percentage points**;
  paired, stratified bootstrap 95% interval **[-4.56, +2.89] points**. Strength
  is currently comparable to classical search. The release uses **25% learned
  scoring**, retaining classical tactical safeguards.
- Adaptation against purposeful opponents across eight frames per matchup
  separated Club and Champion estimates in every discipline. This validates
  response direction on bots; it does not calibrate real human ratings.
- A native game smoke run loaded the bundled model, exercised the Adaptive
  opponent's turn and rendered a selectable Adaptive opponent in Match Setup.
  After startup, the observed game frames were about 8.6–14.6 ms, settling
  near 9.5 ms in that run. This is a local smoke measurement, not a universal
  frame-rate guarantee.

## References

- [Expert Iteration](https://arxiv.org/abs/1705.08439): search supplies training
  experience while the learned evaluator generalises between positions.
- [BRExIt: Opponent Modelling in Expert Iteration](https://www.ijcai.org/proceedings/2023/0422.pdf):
  motivation for conditioning search/learning on the opposing policy.
  This implementation conditions on execution/personality profiles; it does
  not implement the paper's complete algorithm.
