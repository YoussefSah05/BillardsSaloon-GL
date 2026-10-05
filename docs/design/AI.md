# Billiards Saloon — Computer Opponents

Status: classical planner, milestone M8 (0.8.0). Last revised 2026-10-06.
Code: `src/ai/planner.*`, `src/ai/ai_profile.*`; profiles in
`assets/data/ai/players.json`. The learned extensions are in
[INTELLIGENCE.md](INTELLIGENCE.md).

This document explains how a computer player chooses a shot: the decision
problem, the geometry of potting, how human-like error is modelled, and why
the search is built the way it is.

## 1. The decision problem

On each turn a player picks a shot $a$, a vector of continuous parameters:
aim angle $\varphi$, cue speed $v$ and tip offsets $(a_x, a_y)$ (side spin and
follow/draw). Nobody, human or computer, executes exactly the shot they
intend. The executed shot is $a + \varepsilon$, with $\varepsilon$ random
error. The table after the shot is $s' = f(s, a + \varepsilon)$, where $f$ is
the simulator, deterministic and exact. A good player picks the shot that is
best **on average over their own error**:

$$
a^\star = \arg\max_{a \in \mathcal A(s)}\ \mathbb E_{\varepsilon}\!\left[\, U\big(s, f(s, a + \varepsilon)\big) \right],
$$

where $U$ scores the outcome for the player who shot. Three things make this
hard:

1. **The action space is continuous and needle-sharp.** A pot from a metre
   away can need the aim right to a fraction of a degree. Random search over
   $\mathcal A$ would almost never find a pot. Candidates must come from
   geometry (Section 2).
2. **$f$ is discontinuous.** A tiny change in aim decides whether a ball
   drops or rattles out, or which ball is hit first. Gradient methods do not
   apply. Outcomes must be sampled.
3. **The value of an outcome depends on the future.** Keeping the turn is only
   worth as much as the position left for the next shot. $U$ needs an estimate
   of that (Section 5).

This is the structure of the programs that won the computer pool tournaments
of the Computer Olympiad (PickPocket, CueCard): generate candidate shots
geometrically, simulate each under noise, and score the results with a
position evaluator (Smith 2007; Archibald et al. 2009).

## 2. Ghost-ball geometry

To send an object ball at $\mathbf b$ towards a pocket at $\mathbf p$, the
cue ball must strike it along the line from the pocket through the ball. At
contact, the cue ball's centre sits at the **ghost ball** position:

$$
\hat{\mathbf d} = \frac{\mathbf p - \mathbf b}{\lVert \mathbf p - \mathbf b\rVert},
\qquad
\mathbf g = \mathbf b - 2R\,\hat{\mathbf d}.
$$

The cue ball is aimed at $\mathbf g$. The **cut angle** $\theta$ is the angle
between the cue ball's path and the object ball's departure:

$$
\cos\theta = \frac{(\mathbf g - \mathbf c)\cdot\hat{\mathbf d}}{\lVert\mathbf g - \mathbf c\rVert}.
$$

The planner discards cuts above $78^\circ$, and lines blocked by another ball
(any ball whose centre is within $2R$ of either path segment). Ghost-ball
aiming ignores throw (friction between the balls deflecting the object ball
by up to a few degrees) and the cue ball's curved path when it has spin. The
simulator models both. The noisy evaluation in Section 4 measures how much
they matter for each candidate, so the geometric candidate only needs to be
close, not exact.

### Why thin cuts and long shots are hard

An aim error $\delta\varphi$ moves the cue ball's path sideways by
$e \approx d_c\,\delta\varphi$ by the time it reaches the object ball, with
$d_c$ the distance between them. The impact parameter is $2R\sin\theta$, so
$e = 2R\cos\theta\ \delta\theta$, and the object ball's direction, which turns
with the cue line and the cut, changes by

$$
\delta\psi \approx \delta\varphi\left(1 + \frac{d_c}{2R\cos\theta}\right).
$$

The pot succeeds when the object ball's direction is within the pocket's
angular window. That is about $\pm w/d_o$ for an effective half-width $w$
seen from distance $d_o$. With Gaussian aim error
$\delta\varphi \sim \mathcal N(0, \sigma^2)$, the pot probability is

$$
P_\text{pot} \approx 2\,\Phi\!\left(\frac{w/d_o}{\sigma\,\big(1 + d_c / (2R\cos\theta)\big)}\right) - 1.
$$

The error is amplified by the cue ball's distance $d_c$ and by $1/\cos\theta$
(thin cuts), and the window shrinks with the object ball's distance $d_o$.
That is the familiar experience of every player. The planner's quick
**ease** score builds the same dependencies into a cheap heuristic in
$[0, 1]$:

$$
\text{ease} = \cos^2\theta \cdot m \cdot \exp\!\left(-\frac{d_c + d_o}{2.2\,\mathrm{m}}\right),
$$

where $m$ penalises side pockets approached at an angle
($m = \lvert \hat d_y \rvert^{1.5}$ for side pockets, $1$ for corners). A
side pocket's mouth faces straight out from its cushion, so a ball arriving
at an angle sees a narrower opening. Ease is used only to rank and filter. The
real probability is measured by simulation.

## 3. Candidate shots

| Family | Generated as | Count |
|---|---|---|
| **Pots** | every legal ball × every pocket with ease $\ge 0.02$, in four strokes: stun, follow, draw, firm | $\le 4 \cdot 15 \cdot 6$ |
| **Safeties** | every legal ball reachable in a straight line, at fullness $\{0, \pm\tfrac12, \pm0.85\}$ and two soft speeds | $10$ per ball |
| **Kicks** | only when nothing else exists: 48 directions × two speeds, off the cushions | $96$ |

**Speed.** A pot's base speed grows with the travel distance and the cut
angle, and shifts with the player's style:

$$
v_{01} = \operatorname{clamp}\!\left(0.14 + 0.16\,(d_c + d_o) + 0.12\,\frac{\theta}{1.4} + 0.06\,\text{power style},\ 0.08,\ 0.85\right).
$$

The ball must reach the pocket with some pace. A cut sends only $\cos\theta$
of the speed into the object ball, so cuts need more.

**Fullness.** A safety's aim offset for fullness $\phi$ (the fraction of the
ball covered, signed for left or right) is
$\arcsin(2R\phi/d)$ off the line to the ball's centre.

## 4. Two-stage evaluation under noise

**Execution model.** Each profile has standard deviations for aim
($\sigma_\varphi$), relative speed ($\sigma_v$) and tip offset ($\sigma_s$):

$$
\varphi' = \varphi + \sigma_\varphi\, z_1, \qquad
v' = v\,(1 + \sigma_v z_2), \qquad
a_x' = a_x + \sigma_s z_3, \qquad
a_y' = a_y + \sigma_s z_4, \qquad z_i \sim \mathcal N(0, 1).
$$

Speed error is multiplicative: an error of 5% feels the same at any pace.

**Stage 1: the plan as intended.** Every candidate is simulated once without
error, and scored. This is cheap (about a hundred simulations) and removes
everything that fails even when struck perfectly.

**Stage 2: Monte Carlo.** The best ten candidates are each simulated $N$ times
with independent error samples ($N = 6$ to $12$ by profile), and the mean
score estimates the expected utility:

$$
\hat{\mathbb E}[U \mid a] = \frac{1}{N}\sum_{j=1}^{N} U\big(s, f(s, a + \varepsilon_j)\big).
$$

The standard error of that mean is $\sigma_U/\sqrt N$. With outcomes as far
apart as a pot and a foul ($\sigma_U$ of tens of points), $N = 10$ gives a
noisy estimate. That is deliberate. A small $N$ makes weaker profiles
occasionally prefer a risky shot that looked good in a few lucky samples,
which is a human trait. Stronger profiles get more samples, so they estimate
their chances better as well as executing better.

**Why two stages.** Simulation is the cost. Sampling everything would cost
$N\times$ the candidates. Stage 1 is a filter: a shot that fails when executed
perfectly will almost never succeed with error. The risk is the reverse
mistake, discarding a shot whose intended outcome is mediocre but which is
robust. The ten finalists are generous enough that this rarely matters.

**Selection bias.** Choosing the maximum of noisy estimates is biased: the
chosen candidate's estimate is, on average, too high (the "winner's curse",
or optimiser's curse; Smith and Winkler 2006). It does not change which shot is
played much, but it means the planner's reported score is optimistic. Planned
improvements are common random numbers (the same error samples
$\varepsilon_j$ for every candidate, which reduces the variance of the
*differences* between candidates) and an adaptive budget that spends samples
on the candidates that are still close to the best.

## 5. The utility of an outcome

Each simulated outcome is scored in points, from the shooter's view:

$$
U =
\begin{cases}
\pm 100 & \text{frame won or lost,}\\
-70 & \text{foul with two fouls already in a row (rotation games),}\\
-40 & \text{any other foul,}\\
2\,n_\text{pots} + 20 + 12\,V(s') & \text{turn kept,}\\
2\,n_\text{pots} - 18\,V(s'_\text{opp}) & \text{turn passed.}
\end{cases}
$$

$V \in [0, 1]$ is the **position value**: how good the table is for the player
about to shoot. When the turn passes, it is evaluated for the opponent and
subtracted. A safety earns its score by leaving the opponent nothing.

**Position value.** A good position has one easy shot and several options. For
the player about to shoot, with $e_\text{max}$ the best ease over all legal
balls and pockets and $n_\text{good}$ the number of lines with ease above
$0.25$:

$$
V = 0.75\,\min\!\left(\frac{e_\text{max}}{0.6}, 1\right) + 0.25\,\frac{\min(n_\text{good}, 3)}{3},
$$

with $V = 0.5$ for ball in hand. The first term rewards having a shot, and the
second rewards not depending on a single shot. This evaluator is hand-made and
looks one shot ahead. It is exactly what the learned value network in
[INTELLIGENCE.md](INTELLIGENCE.md) is meant to replace. That network estimates
the probability of winning the frame, $\hat P(\text{win} \mid s')$. Plugged
into the same slot, it maps onto the same scale as $100\,(2\hat P - 1)$, so
that "certain to win" scores like a won frame.

**Why these weights.** They encode a pool player's priorities: never foul
(a foul gives away ball in hand, worth almost a frame in rotation games), keep
the table, and leave yourself position. The three-foul penalty reflects that a
third consecutive foul in rotation games loses the frame. The constants were
tuned by watching games, and are checked by the tests in
`tests/test_ai.cpp` (for example, a champion must pot more than a club
player).

**Intent.** A pot gets $+1$ and a safety gets the profile's safety bias
(from $-8$ to $+6$ points). This expresses style: an attacker shoots unless the
safety is clearly better, and a tactician plays safe unless the pot is. A
"safety" that happens to pot a ball and keep the turn is reclassified as a pot,
so the bias does not apply to it.

## 6. Other decisions

**Break.** Aimed at the ball nearest the cue ball (the head ball), at the
profile's break speed, with a little draw to keep the cue ball in the middle
of the table.

**Cue ball placement** (ball in hand). Candidate spots lie behind each legal
ball on its line to each pocket, at three distances ($0.2$, $0.4$ and
$0.7\,\mathrm{m}$ behind contact) and three lateral offsets ($0$ and
$\pm 6\,\mathrm{cm}$, for natural angle). Spots are ranked by position value,
and the best four are then fully planned. The spot whose best shot scores
highest wins. Ranking first, planning second is the same two-stage idea as
Section 4.

**Options** (play or pass back after a push-out, play or re-rack after an
illegal break). The chooser plans the shot it would have, with three samples,
and plays when its score exceeds 5 points, a small margin above an even
position.

## 7. Profiles

Five fictional players span the tiers. Their execution noise is the main
difference in strength:

| Player | Tier | $\sigma_\varphi$ | $\sigma_v$ | $\sigma_s$ | Safety bias | Samples |
|---|---|---|---|---|---|---|
| Sam Whitlock | Club | $1.10^\circ$ | 0.14 | 0.18 | $-8$ | 6 |
| Dani Reyes | Regional | $0.60^\circ$ | 0.09 | 0.12 | $0$ | 8 |
| Iris Lindqvist | Pro | $0.32^\circ$ | 0.06 | 0.08 | $+6$ | 10 |
| Marta Okafor | Pro | $0.30^\circ$ | 0.06 | 0.08 | $-4$ | 10 |
| Viktor Hale | Champion | $0.16^\circ$ | 0.04 | 0.05 | $+2$ | 12 |

Each player also thinks for a fixed time, and the planner runs on a worker
thread so the frame never hitches. Two players of the same tier differ in
style, not in level: Iris and Marta have almost the same error but choose
different shots.

## 8. Limitations

- **One-shot lookahead.** The planner does not plan sequences ("pot this to
  get on that"). Position value only looks one shot ahead. This is the main
  weakness the learned value network targets.
- **No banks or combinations** as planned pots. They only happen by accident,
  or in kick safeties.
- **Fixed candidate strokes.** Four stroke types per pot line are coarse. Real
  players choose speed and spin for position on a continuum.
- **Safety evaluation** relies on $V$ for the opponent, which ignores that the
  opponent may also play safe.

## References

- Smith, M. *PickPocket: A Computer Billiards Shark.* Artificial Intelligence 171(16–17), 2007.
- Archibald, C., Altman, A., Shoham, Y. *Analysis of a Winning Computational Billiards Player.* IJCAI 2009.
- Archibald, C., Altman, A., Greenspan, M., Shoham, Y. *Computational Pool: A New Challenge for Game Theory Pragmatics.* AI Magazine 31(4), 2010.
- Smith, J. E., Winkler, R. L. *The Optimizer's Curse: Skepticism and Postdecision Surprise in Decision Analysis.* Management Science 52(3), 2006.
- Alciatore, D. G. *The Illustrated Principles of Pool and Billiards.* Sterling, 2004 (aiming, throw, cut-angle error).
