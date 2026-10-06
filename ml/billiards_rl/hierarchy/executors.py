"""Executors: the shot layer π_L(s, goal) → physical strike.

Both see only what the single-shot policies were trained on: the cue ball,
the chosen object ball and the target pocket. Other balls are present in the
simulation but invisible to the executor (the goal was chosen with clear lines).
"""

from __future__ import annotations

import numpy as np

import billiards_sim as bs
from billiards_rl.envs.single_shot_env import SingleShotEnv

rl = bs.rl


class GeometryExecutor:
    name = "geometry"

    def strike(self, positions: np.ndarray, ball: int, pocket: int) -> np.ndarray:
        two = positions[[0, ball]]
        shot = rl.geometry_shot(two, pocket=pocket, max_cut=90.0)
        if shot is None:
            d = two[1] - two[0]
            return np.array([2.5, float(np.degrees(np.arctan2(d[1], d[0])) % 360.0), 0, 0, 0])
        return np.asarray(shot["strike"], dtype=np.float64)


class PolicyExecutor:
    """A trained single-shot policy (ghost-relative, positions+target) as π_L."""

    def __init__(self, model, name: str = "policy"):
        self.model = model
        self.name = name
        # A two-ball env reused only for its observation and action mapping.
        self.env = SingleShotEnv(action_frame="ghost_relative", obs_mode="positions+target", reward="sparse_target")
        self.env.reset(seed=0)

    def strike(self, positions: np.ndarray, ball: int, pocket: int) -> np.ndarray:
        two = positions[[0, ball]]
        env = self.env
        env._positions = two.copy()
        env._layout = {"pocket": pocket, "difficulty": "none", "positions": two}
        env._aims = rl.ghost_aims(two)
        obs = env._observe()
        action, _ = self.model.predict(obs, deterministic=True)
        strike, _, _ = env.strike_for(action)
        return strike
