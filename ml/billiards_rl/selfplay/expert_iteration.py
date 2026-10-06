"""Expert iteration on RunEnv (Milestone 13).

Generation $k$: the search player with leaf value $V_{k-1}$ (generation 0:
the simulated next-shot pot rate of experiment 11) plays racks under the
execution noise; every turn's state is stored with its return-to-go
$G_t = \\sum_{t' \\ge t} r_{t'}$. $V_k$ is fitted to these returns, and becomes
the leaf of generation $k+1$:

$$
Q_k(s, a) = \\frac1K \\sum_{k'} \\Big[ r(s, a + \\varepsilon_{k'}) + \\mathbb 1[\\text{turn kept}]\\, V_{k-1}(s'_{k'}) \\Big].
$$

The search is the policy-improvement operator and Monte Carlo regression is
policy evaluation: approximate policy iteration, with search in place of a
policy network (the single-agent form of AlphaZero's loop; there is no
opponent in RunEnv).
"""

from __future__ import annotations

import numpy as np

from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.search.expectimax import ExpectimaxPlayer


def play_racks(player: ExpectimaxPlayer, racks: int, noise: float, seed: int) -> tuple[list[np.ndarray], np.ndarray, np.ndarray]:
    """(states, returns-to-go, rack returns) from `racks` random 3-ball racks."""
    env = RunEnv(balls=3, noise=(noise, 0.0))
    rng = np.random.default_rng(seed)
    states, targets, totals = [], [], []
    for _ in range(racks):
        env.reset(int(rng.integers(0, 2**31)))
        visited, rewards = [], []
        while True:
            visited.append(env.positions.copy())
            reward, done, _ = env.play_strike(player.choose_strike(env))
            rewards.append(reward)
            if done:
                break
        to_go = np.cumsum(rewards[::-1])[::-1]
        states.extend(visited)
        targets.extend(to_go.tolist())
        totals.append(float(sum(rewards)))
    return states, np.asarray(targets, dtype=np.float32), np.asarray(totals)
