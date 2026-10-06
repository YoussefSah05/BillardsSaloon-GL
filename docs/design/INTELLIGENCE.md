# Billiards Saloon — Intelligence

Status: design, milestone M9. Last revised 2026-10-06. Companion to `GDD.md`;
the classical planner it builds on is described in [AI.md](AI.md).

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
- **Action:** shot parameters (aim angle $\varphi$, speed $v$, tip offsets $a$
  and $b$, elevation $\theta$).
  Agents usually act as *candidate index + continuous residual*, which keeps
  learning tractable.
- **Execution noise:** explicit and per player: Gaussian error on $\varphi$,
  $v$, $a$, $b$ and $\theta$,
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
   TD($\lambda$)); the policy head fits the shots search preferred.
4. **Gate:** a new model replaces the old one only after it wins on the Elo
   ladder.

Compute is this Mac: self-play across CPU cores with multiprocessing, network
training on MPS. The first task is a simulator throughput benchmark
(shots per second per core), which sets the size of every experiment.

## Mathematical foundations

This section gives the models behind the design: what the networks estimate,
how they are trained and evaluated, and how player skill is inferred.

### The value function

Pool is a two-player, zero-sum, turn-based game with stochastic transitions:
the state $s$ (ball positions plus the rules context) changes under a chosen
shot $a$ and random execution error $\varepsilon$, through the deterministic
simulator, $s' = f(s, a + \varepsilon)$. The **value** of a state, for the
player about to shoot when both players follow policies $\pi$, is the
probability that they win the frame:

$$
V^\pi(s) = \Pr\big(\text{player to move wins} \mid s, \pi\big).
$$

Who moves next depends on the outcome (a pot keeps the turn, a miss passes
it), so the value obeys a Bellman equation with a sign flip on the turn:

$$
V^\pi(s) = \mathbb E_{a \sim \pi(s),\ \varepsilon}\Big[\, \mathbb 1[\text{turn kept}]\ V^\pi(s') + \mathbb 1[\text{turn passed}]\ \big(1 - V^\pi(s')\big) \Big],
$$

with $V = 1$ or $0$ at the end of a frame. This is what the classical
planner's position value ([AI.md](AI.md)) approximates by hand, one shot
ahead. The network learns it from games.

### Learning the value: proper scoring

The network outputs a logit $z_\theta(s)$, and
$\hat p = \sigma(z_\theta(s)) = 1/(1 + e^{-z})$. Training minimises the binary
cross-entropy against the frame's outcome $y \in \{0, 1\}$ for the player who
moved at $s$:

$$
\mathcal L(\theta) = -\frac{1}{N}\sum_{i=1}^{N} \Big[ y_i \log \hat p_i + (1 - y_i)\log(1 - \hat p_i) \Big].
$$

Cross-entropy is a **strictly proper scoring rule**: its expectation is
minimised exactly when $\hat p$ equals the true conditional probability
$\Pr(y = 1 \mid s)$. So the network is pushed towards calibrated
probabilities, not merely good rankings. That is what the planner needs, since
it adds values across outcomes. Each outcome label is noisy (the same
position can be won or lost), but its expectation is the value, so the noise
averages out over many positions.

**Metrics.** Three numbers are reported on a validation set split **by frame**
(positions from one frame are strongly correlated, so a random split would
leak):

- log loss, the objective above;
- the Brier score $\frac1N\sum_i (\hat p_i - y_i)^2$, also proper, and easier
  to read;
- accuracy of $\hat p > \tfrac12$.

Two baselines bound what counts as learning. The **base rate** predicts the
training mean $\bar y$ everywhere. Its log loss is the entropy
$H(\bar y) = -\bar y\log\bar y - (1-\bar y)\log(1-\bar y)$, about $0.692$ for
$\bar y = 0.528$. **Logistic regression** on the same features shows how much
a linear model can extract. A network is only useful if it beats both.

### Monte Carlo and temporal-difference targets

Labelling every position of a frame with the frame's final result is the
**Monte Carlo** target $G_t = y$. It is unbiased but high-variance: the label
of an early position depends on every later shot, including luck.
**Temporal-difference** targets replace the remainder of the game by the
current estimate (Sutton 1988). The one-step target, written for the player
who moves at $s_t$, is

$$
G_t^{(1)} =
\begin{cases}
\hat V(s_{t+1}) & \text{turn kept,} \\
1 - \hat V(s_{t+1}) & \text{turn passed.}
\end{cases}
$$

The **TD($\lambda$)** target mixes all horizons geometrically,
$G_t^\lambda = (1-\lambda)\sum_{n \ge 1}\lambda^{n-1} G_t^{(n)}$. It runs from
one-step TD at $\lambda = 0$ (low variance, biased by the current estimate) to
Monte Carlo at $\lambda = 1$. The first experiments use Monte Carlo. TD($\lambda$)
is the planned next step when outcome noise limits learning.

### A set encoder over the balls

A table is a **set** of balls: the order in which they are listed carries no
meaning. A function of a set must be permutation-invariant:
$F(\{x_{\pi(1)}, \dots, x_{\pi(n)}\}) = F(\{x_1, \dots, x_n\})$ for every
permutation $\pi$. Two architectures guarantee this:

- **DeepSets** (Zaheer et al. 2017): $F(X) = \rho\big(\sum_i \phi(x_i)\big)$.
- **Transformer encoders without positional encoding**: self-attention is
  permutation-*equivariant*,
  $\operatorname{Attn}(Q, K, V) = \operatorname{softmax}\!\big(QK^\top/\sqrt{d}\big)V$
  with $Q, K, V$ projections of every ball, so permuting the input permutes
  the output. A pooling step that ignores order (here a masked mean over the
  balls on the table) makes the result invariant.

Attention lets each ball's representation depend on the others (which balls
block which lines, which are clustered), which a sum of independent $\phi(x_i)$
cannot do before pooling. Pocketed balls are masked out of attention and
pooling, so one network handles any number of balls and any discipline.
Global context (discipline, groups, ball in hand, fouls) is concatenated after
pooling.

### Expert iteration

Search with a model is stronger than the model alone, so search can be the
teacher (Anthony et al. 2017; Silver et al. 2017). One round of expert
iteration:

1. **Expert:** the planner searches with the current value $V_\theta$ (and,
   later, a shot prior $\pi_\theta$) to choose shots in self-play.
2. **Apprentice:** $V_\theta$ is trained on the outcomes of those games, and
   $\pi_\theta$ to imitate the shots the search chose,
   $\mathcal L_\pi = -\sum_a \pi_\text{search}(a \mid s) \log \pi_\theta(a \mid s)$.
3. **Repeat:** a better apprentice makes a better expert.

### Measuring strength: Elo

Agents are compared by round-robin matches. Under the Elo model, the expected
score of $A$ against $B$ with ratings $R_A$ and $R_B$ is

$$
E_A = \frac{1}{1 + 10^{(R_B - R_A)/400}},
$$

so a 100-point gap means about 64%. Ratings are fitted to all results by
maximum likelihood (the Bradley–Terry model). Every win rate is reported with
a confidence interval. From $n$ frames with win rate $\hat w$, the Wilson
95% interval is

$$
\frac{\hat w + \frac{z^2}{2n} \pm z\sqrt{\frac{\hat w(1-\hat w)}{n} + \frac{z^2}{4n^2}}}{1 + \frac{z^2}{n}}, \qquad z = 1.96.
$$

For example, 55% over 400 frames is $[50\%, 60\%]$: barely distinguishable
from even. Hence the gating rule: a new model replaces the old one only when
the lower bound of its win rate is above 50%.

### Estimating a player's skill

The execution model gives every shot an error $\delta\varphi$ in aim, the
angle between the shot played and the line the player meant. For a human, the
intended line is not observed directly. It is taken as the line of the best
shot the planner finds near what was played (the nearest candidate in aim and
speed). Assume errors are Gaussian, $\delta\varphi_i \sim \mathcal N(0, \sigma^2)$, and
put a conjugate inverse-gamma prior on the variance,
$\sigma^2 \sim \text{Inv-Gamma}(\alpha_0, \beta_0)$. After $n$ shots the
posterior is again inverse-gamma:

$$
\sigma^2 \mid \delta\varphi_{1:n} \sim \text{Inv-Gamma}\!\left(\alpha_0 + \frac n2,\ \beta_0 + \frac12\sum_{i=1}^{n}\delta\varphi_i^2\right).
$$

The prior is centred on a club player, so a few shots barely move it, and the
estimate firms up as evidence accumulates. Its posterior mean,
$\beta_n/(\alpha_n - 1)$, sets the player's rating and picks opponents whose
noise is close. The same update applies to speed and spin error. Ambiguous
shots (deliberate safeties, kicks) are left out, because the intended line is
uncertain.

### Rating a generated challenge

A layout's difficulty at skill $\sigma$ is its solve probability under that
skill, estimated by Monte Carlo: $\hat q = k/n$ from $k$ successes in $n$ noisy
attempts by the solver. Its standard error is $\sqrt{\hat q(1-\hat q)/n}$. A
layout is kept when $\hat q$ lands in the target band for its tier (for
example 30–60% for a "hard" drill at club skill), and stars are assigned from
$\hat q$ at the player's estimated skill.

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
- Sutton, *Learning to Predict by the Methods of Temporal Differences*, Machine Learning 3 (1988).
- Zaheer et al., *Deep Sets*, NeurIPS 2017.
- Vaswani et al., *Attention Is All You Need*, NeurIPS 2017.
- Gneiting, Raftery, *Strictly Proper Scoring Rules, Prediction, and Estimation*, JASA 102 (2007).
- Elo, *The Rating of Chessplayers, Past and Present*, 1978; Bradley, Terry, *Rank Analysis of Incomplete Block Designs*, Biometrika 39 (1952).
- Wilson, *Probable Inference, the Law of Succession, and Statistical Inference*, JASA 22 (1927).
- Kiefl, *Pooltool*, JOSS 2024 — simulator design and RL-ready interfaces.
- *CueTip: An Interactive and Explainable Physics-aware Pool Assistant*, arXiv 2501.18291 — language explanations for coaching.
