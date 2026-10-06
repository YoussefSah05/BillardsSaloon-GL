# Reward Design

Status: Milestone 3. Last revised 2026-10-06. Code:
`ml/billiards_rl/rewards/single_shot.py`.

## Principles

1. **Rewards are functions of the outcome.** A reward sees what the shot did
   (`ShotResult`: pocket, scratch, first contact, where the object ball
   stopped, the episode's target) and nothing else. Changing the reward never
   changes the physics or the environment.
2. **Start unshaped.** Shaping changes the optimal policy unless it is
   potential-based (Ng, Harada and Russell 1999), and in a needle-shaped
   landscape it easily rewards "nearly" in ways that pull the policy away from
   the narrow region that actually pots.
3. **Every new term earns its place experimentally.** A shaped reward is added
   as a new named function and kept only if it improves success on the fixed
   evaluation suites, under the same tuning budget, across seeds.
4. **Metrics never come from the reward.** Success, scratch rate and the
   other metrics are read from `info`, so experiments with different rewards
   remain comparable.

## Rewards

### `sparse` (default)

$$
r = \begin{cases}
-1 & \text{cue ball potted (scratch), whatever else happened}\\
+1 & \text{object ball potted in any pocket}\\
-0.1 & \text{otherwise.}
\end{cases}
$$

A scratch outranks a pot, because in every game a scratch is a foul and the
potted ball does not count for the shooter. The small miss penalty separates
"missed" from "potted" by 1.1 and "missed" from "scratched" by 0.9, so the
critic learns that scratches are worse than misses. With one shot per episode
($\gamma$ does not matter), the return is the reward.

### `sparse_target`

The same as `sparse`, but only the episode's target pocket counts; a pot
elsewhere scores like a miss. It is for goal-conditioned environments
(Milestone 8), where the pocket is part of the goal and HER relabels a pot in
pocket $j$ as success for goal $j$.

## Candidates for later (not implemented)

| Term | Form | Risk |
|---|---|---|
| Distance after a miss | $-\lambda\, d(\mathbf{p}_o, \text{mouth})$ | rewards rolling the ball near the jaws |
| Legal first contact | $+\epsilon$ if `first_contact == 1` | trivial to satisfy |
| Cue-ball position (Milestone 10) | $-\lambda\, d(\mathbf{p}_\text{cue}, \mathbf{p}_\text{target})$ | trades pots for position; $\lambda$ tuned per experiment |
