# Papers and Methods

Notes on the methods this project uses or plans to use. Each note covers the
problem the method solves, its assumptions, and what it implies for billiards
shots, where success lies in action windows about $10^{-3}$ of the action
range wide ([current-game-audit.md](current-game-audit.md)). Read before
borrowing; nothing here is reproduced blindly.

## Soft Actor-Critic (Haarnoja et al., ICML 2018)

**Problem.** Sample-efficient, stable off-policy learning for continuous
actions.

**Method.** It maximises reward plus policy entropy,
$J(\pi) = \mathbb E\left[\sum_t r_t + \alpha\,\mathcal H(\pi(\cdot\mid s_t))\right]$,
with a stochastic tanh-Gaussian actor, twin soft Q-critics, and a
temperature $\alpha$ tuned automatically towards a target entropy.

**Assumptions and implications.** The critic must model $Q(s, a)$ well
enough for the actor's gradient to point at the reward. A one-shot episode
makes $Q(s, a) = \mathbb E[r \mid s, a]$, a supervised regression, which is
the most favourable case for the critic. But the target here is a narrow
spike on a flat floor of $-0.1$. A smooth MLP critic tends to blur the spike,
and its gradient is near zero almost everywhere. Entropy keeps the actor
exploring, which helps it stumble on spikes but also pushes it off them,
since a precise action is a low-entropy one.

## TD3 (Fujimoto, van Hoof and Meger, ICML 2018)

**Problem.** Overestimation bias and instability in deterministic
actor-critic methods (DDPG).

**Method.**

- Twin critics, using the minimum of the two targets.
- The actor is updated less often than the critics.
- Target policy smoothing: noise is added to target actions.

**Implications.** Deterministic actions suit precision. Exploration comes
only from added action noise, which must be wide enough to find windows yet
not destroy precise ones. Target smoothing assumes $Q$ is smooth in $a$,
which is exactly what a needle violates; with one-step episodes the targets
are rewards, so it does not apply here.

## PPO (Schulman et al., 2017)

**Problem.** Stable on-policy policy-gradient updates.

**Method.** A clipped surrogate objective,
$\min\left(\rho_t A_t,\ \operatorname{clip}(\rho_t, 1 \pm \epsilon) A_t\right)$
with $\rho_t = \pi_\theta / \pi_{\theta_\text{old}}$.

**Implications.** It is on-policy, so it discards every sample after an
update; with a 0.2% success rate, gradients come from very few successes. It
is expected to be the least sample-efficient here and is used as a baseline,
as the brief says.

## Hindsight Experience Replay (Andrychowicz et al., NeurIPS 2017)

**Problem.** Sparse rewards in goal-reaching tasks.

**Method.** Goal-conditioned policies $\pi(a \mid s, g)$. Stored transitions
are relabelled with goals that were actually achieved, so failures become
successes for other goals.

**Assumptions.** It needs an `achieved_goal` that is meaningful for many
episodes, and a reward computable from (achieved, desired).

**Implications.** "The object ball dropped in pocket $j$" is an achieved goal
only when *some* pocket is reached. That is rare under random play (2–3%), so
naive HER relabels few transitions. Richer achieved goals help: a region of
the table where the object ball came to rest, or the direction it left in.
This is a design question for Milestone 8, not a given.

## Advantage-Weighted Regression (Peng et al., 2019) and elite-sample learning

**Problem.** Simple, stable policy improvement by supervised learning.

**Method.** Regress the policy onto past actions, weighted by
$\exp(A(s, a)/\beta)$; elite or cross-entropy variants keep only the top
fraction of samples.

**Implications.** It learns where successes are without needing a smooth
critic gradient. That is attractive for needles: a single success at
$\varphi^\star$ pulls the policy's mean straight there. The risk is mode
averaging when several windows exist, so the policy distribution should be
multimodal, or conditioned on the target pocket.

## SCOOT and billiards-specific work

The research brief names SCOOT ("State-Conditioned Shooting") and recent
computational billiards work (pix2pockets). Their exact formulations and
experimental assumptions (simulator, noise model, action parameterisation,
evaluation states) must be read from the papers before anything is borrowed;
this file will record them when Milestone 9 starts. Earlier work this project
already builds on:

- **Smith 2007**, PickPocket, and **Archibald et al. 2009**, CueCard, the
  Computer Olympiad winners: candidate generation, simulation under noise and
  evaluation of the resulting position. The game's classical AI follows this,
  and Milestone 12 generalises it with learned proposals.
- **Leckie and Greenspan 2005**, event-based billiards simulation, and
  **pooltool** (Kiefl 2024): the simulator's algorithm and our oracle for
  validation.

## AlphaZero (Silver et al., Science 2018) and MuZero (Schrittwieser et al., Nature 2020)

Policy and value networks guiding a search, trained by self-play. AlphaZero
assumes discrete actions and deterministic dynamics; MuZero adds a learned
model. In billiards the actions are continuous and the dynamics stochastic
(execution noise), so their direct use would need progressive widening or
sampled actions. Here they are the conceptual model for Milestones 12–13
(learned proposals plus search plus self-play), not an architecture to copy.
