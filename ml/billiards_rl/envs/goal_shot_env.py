"""GoalShotEnv: SingleShotEnv with an explicit goal for the object ball (Milestone 8).

The goal is *where the object ball should end up*, as a 4-vector in
normalised table coordinates:

    (x', y', is_pocket, scratch)

- **desired goal:** the target pocket's mouth, with ``is_pocket = 1`` and
  ``scratch = 0``;
- **achieved goal:** the mouth of the pocket the object ball dropped into
  (``is_pocket = 1``), or the spot where it came to rest (``is_pocket = 0``),
  and ``scratch = 1`` if the cue ball was pocketed.

A shot achieves a goal when the flags match and the points are within
`success_radius` metres. A pocket goal is therefore met only by a pot in that
pocket, never by a ball resting near the jaws; a scratch fails every goal and
scores −1. The scratch is carried in the goal because Stable-Baselines3's HER
buffer calls `compute_reward` without the step's info.

Why not "which pocket" as the goal? Under random play only 2–3% of shots pot
anything, so hindsight relabelling would have almost nothing to relabel
(docs/rl/papers.md). With positions as goals, every shot that moves the object
ball becomes, in hindsight, a success for "send it there": a dense signal for
the coarse "hit the object ball in this direction" skill, which is exactly
what the absolute frame failed to learn (Experiments 1–4). The same goal space
serves cue-ball position play later.
"""

from __future__ import annotations

from typing import Any

import numpy as np
from gymnasium import spaces

from billiards_rl.envs.single_shot_env import SingleShotEnv

GOAL_SIZE = 4


class GoalShotEnv(SingleShotEnv):
    def __init__(self, success_radius: float = 0.1, **kwargs):
        kwargs.setdefault("reward", "sparse_target")
        super().__init__(**kwargs)
        self.success_radius = float(success_radius)
        base = self.observation_space
        goal = spaces.Box(-1.0, 1.0, shape=(GOAL_SIZE,), dtype=np.float32)
        self.observation_space = spaces.Dict({"observation": base, "achieved_goal": goal, "desired_goal": goal})
        self._achieved = np.zeros(GOAL_SIZE, dtype=np.float32)
        self._desired = np.zeros(GOAL_SIZE, dtype=np.float32)

    # ---- Goals -----------------------------------------------------------------

    def _point(self, p) -> np.ndarray:
        return np.array([2.0 * p[0] / self.width - 1.0, 2.0 * p[1] / self.length - 1.0])

    def _metres(self, g: np.ndarray) -> np.ndarray:
        """Goal points (..., 2) in normalised coordinates back to metres."""
        return np.stack([(g[..., 0] + 1.0) * 0.5 * self.width, (g[..., 1] + 1.0) * 0.5 * self.length], axis=-1)

    def _goal_for_pocket(self, pocket: int) -> np.ndarray:
        if pocket < 0:
            pocket = 0
        return np.concatenate([self._point(self.mouths[pocket]), [1.0, 0.0]]).astype(np.float32)

    def compute_reward(self, achieved_goal, desired_goal, info) -> np.ndarray:
        """Vectorised: −1 for a scratch, +1 when the goal is met, −0.1 otherwise."""
        achieved = np.asarray(achieved_goal, dtype=np.float64)
        desired = np.asarray(desired_goal, dtype=np.float64)
        distance = np.linalg.norm(self._metres(achieved[..., :2]) - self._metres(desired[..., :2]), axis=-1)
        scratch = achieved[..., 3] > 0.5
        met = (np.abs(achieved[..., 2] - desired[..., 2]) < 0.5) & (distance < self.success_radius) & ~scratch
        return np.where(scratch, -1.0, np.where(met, 1.0, -0.1))

    # ---- Gymnasium API ---------------------------------------------------------

    def reset(self, *, seed: int | None = None, options: dict[str, Any] | None = None):
        observation, info = super().reset(seed=seed, options=options)
        self._desired = self._goal_for_pocket(self.target_pocket)
        self._achieved = np.concatenate([self._point(self._positions[1]), [0.0, 0.0]]).astype(np.float32)
        return self._goal_obs(observation), info

    def step(self, action):
        result, info = self._play(action)
        if result.potted:
            point = self._point(self.mouths[result.object_pocket])
            flag = 1.0
        else:
            point = np.clip(self._point(result.object_final), -1.0, 1.0)
            flag = 0.0
        self._achieved = np.concatenate([point, [flag, 1.0 if result.scratch else 0.0]]).astype(np.float32)
        reward = float(self.compute_reward(self._achieved, self._desired, info))
        info["is_success"] = reward > 0.0
        return self._goal_obs(self._observe()), reward, True, False, info

    def _goal_obs(self, observation: np.ndarray) -> dict[str, np.ndarray]:
        return {"observation": observation, "achieved_goal": self._achieved.copy(), "desired_goal": self._desired.copy()}
