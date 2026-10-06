"""Candidate shots plus sampled expectimax over execution noise (Milestone 12).

Each turn:

1. **Candidates.** For each goal (ball, pocket) of RunEnv, the shot policy
   π_L proposes a shot; the search adds variations of speed (×0.8, 1, 1.25)
   and follow/draw (b ∈ {−0.3, 0, 0.3}): 9 shots per goal, at most 72 per
   turn. The policy keeps the branching factor small; the search never
   explores the continuous action space blindly (research brief, §25).
2. **Chance nodes.** Each candidate is replayed `samples` times with the
   execution noise the player has (Gaussian aim error), in one batch.
3. **Value.** Depth 1: the mean of (balls potted − scratch). Depth 2: plus,
   for each replay that keeps the turn, the estimated success of the best
   next shot, itself simulated: the geometry shot for every next goal,
   replayed `leaf_samples` times with the same noise. This is expectimax
   with sampled chance nodes:

$$
Q(s, a) = \\frac1K \\sum_{k=1}^{K} \\Big[ r(s, a + \\varepsilon_k) + \\mathbb 1[\\text{turn kept}]\\, \\max_{g'} \\hat p(s'_k, g') \\Big],
$$

with $\\hat p(s', g')$ the simulated pot rate of goal $g'$ in state $s'$.
4. The candidate with the highest $Q$ is played.
"""

from __future__ import annotations

import numpy as np

import billiards_sim as bs
from billiards_rl.hierarchy.run_env import RunEnv

rl = bs.rl


class ExpectimaxPlayer:
    def __init__(self, executor, depth: int = 2, samples: int = 16, leaf_samples: int = 4, sigma: float = 0.25,
                 speed_factors=(0.8, 1.0, 1.25), verticals=(-0.3, 0.0, 0.3), seed: int = 0, threads: int = 0):
        self.executor = executor
        self.depth = depth
        self.samples = samples
        self.leaf_samples = leaf_samples
        self.sigma = sigma
        self.speed_factors = speed_factors
        self.verticals = verticals
        self.rng = np.random.default_rng(seed)
        self.threads = threads
        self.name = f"expectimax-d{depth}"

    # ---- Candidates ------------------------------------------------------------

    def candidates(self, env: RunEnv) -> list[tuple[int, int, np.ndarray]]:
        shots = []
        for goal in env.candidates:
            base = np.asarray(self.executor.strike(env.positions, goal.ball, goal.pocket), dtype=np.float64)
            for f in self.speed_factors:
                for b in self.verticals:
                    s = base.copy()
                    s[0] = float(np.clip(s[0] * f, 0.5, 7.0))
                    s[4] = b
                    shots.append((goal.ball, goal.pocket, s))
        return shots

    # ---- Evaluation -------------------------------------------------------------

    def _noisy(self, strikes: np.ndarray, k: int) -> np.ndarray:
        repeated = np.repeat(strikes, k, axis=0)
        repeated[:, 1] = (repeated[:, 1] + self.rng.normal(0.0, self.sigma, len(repeated))) % 360.0
        return repeated

    def _leaf_values(self, states: list[np.ndarray]) -> np.ndarray:
        """Best simulated pot rate of the next shot, for each state."""
        env = RunEnv(balls=3)
        layouts, strikes, owner, ball_of = [], [], [], []
        for i, positions in enumerate(states):
            for goal in env._candidates_for(positions):
                shot = rl.geometry_shot(positions[[0, goal.ball]], pocket=goal.pocket, max_cut=90.0)
                if shot is None:
                    continue
                layouts.append(positions)
                strikes.append(np.asarray(shot["strike"], dtype=np.float64))
                owner.append((i, len(layouts) - 1, goal.ball, goal.pocket))
        values = np.zeros(len(states))
        if not strikes:
            return values
        k = self.leaf_samples
        # Pad layouts to one shape (states differ in ball count): group by size.
        by_size: dict[int, list[int]] = {}
        for j, positions in enumerate(layouts):
            by_size.setdefault(len(positions), []).append(j)
        rates = np.zeros(len(layouts))
        for size, idx in by_size.items():
            batch_layouts = np.repeat(np.stack([layouts[j] for j in idx]), k, axis=0)
            batch_strikes = self._noisy(np.stack([strikes[j] for j in idx]), k)
            out = rl.simulate_batch(batch_layouts, batch_strikes, self.threads)
            for n, j in enumerate(idx):
                ball, pocket = owner[j][2], owner[j][3]
                rows = slice(n * k, (n + 1) * k)
                ok = (out["pocket_of"][rows, ball] == pocket) & ~out["scratch"][rows]
                rates[j] = ok.mean()
        for (i, j, _, _) in owner:
            values[i] = max(values[i], rates[j])
        return values

    def choose_strike(self, env: RunEnv) -> np.ndarray:
        shots = self.candidates(env)
        strikes = np.stack([s for _, _, s in shots])
        k = self.samples
        layouts = np.repeat(env.positions[None], len(strikes) * k, axis=0)
        out = rl.simulate_batch(layouts, self._noisy(strikes, k), self.threads)
        pocketed = out["pocket_of"] >= 0
        scratch = out["scratch"]
        immediate = pocketed[:, 1:].sum(1) - scratch.astype(float)

        values = immediate.astype(float)
        if self.depth >= 2:
            continuing = np.flatnonzero(~scratch & (pocketed[:, 1:].sum(1) > 0) & (pocketed[:, 1:].sum(1) < len(env.positions) - 1))
            states = []
            for row in continuing:
                remaining = [0] + [i for i in range(1, len(env.positions)) if not pocketed[row, i]]
                states.append(np.asarray(out["final_positions"][row])[remaining])
            if states:
                values[continuing] += self._leaf_values(states)
        q = values.reshape(len(strikes), k).mean(1)
        return strikes[int(np.argmax(q))]

    def run_rack(self, env: RunEnv) -> float:
        total = 0.0
        while True:
            reward, done, _ = env.play_strike(self.choose_strike(env))
            total += reward
            if done:
                return total
