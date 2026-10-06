"""DirectionGoalEnv: goals as the object ball's direction of travel (Experiment 6c).

Experiment 6b showed that "where the object ball comes to rest" is a poor
hindsight goal: reaching a spot needs the right direction *and* distance, so
the relabelled task is as needle-like as potting. The object ball's
**departure direction** is set by the contact alone: smooth in the aim,
independent of speed and cushions. Potting into pocket k is departing within
pocket k's window. Goals are 5-vectors:

    (cos theta, sin theta, potted, scratch, no_contact)

- **desired goal:** the direction from the object ball to the target pocket's
  mouth, with ``potted = 1``;
- **achieved goal:**
  - after a pot, the direction to the mouth of the pocket the ball entered
    (``potted = 1``), so a pocket goal means exactly "pot in that pocket";
  - after a contact without a pot, the departure direction (``potted = 0``);
  - after no contact, nothing (``no_contact = 1``, which meets no goal).

A goal is met when the flags match, there was contact and no scratch, and
the directions agree within `tolerance_deg`. A scratch scores −1, a met goal
+1, otherwise −0.1. With hindsight relabelling, every shot that touches the
object ball is a success for "send it that way".
"""

from __future__ import annotations

from typing import Any

import numpy as np
from gymnasium import spaces

from billiards_rl.envs.single_shot_env import SingleShotEnv

DIRECTION_GOAL_SIZE = 5


class DirectionGoalEnv(SingleShotEnv):
    def __init__(self, tolerance_deg: float = 1.0, **kwargs):
        kwargs.setdefault("reward", "sparse_target")
        super().__init__(**kwargs)
        self.tolerance_cos = float(np.cos(np.radians(tolerance_deg)))
        base = self.observation_space
        goal = spaces.Box(-1.0, 1.0, shape=(DIRECTION_GOAL_SIZE,), dtype=np.float32)
        self.observation_space = spaces.Dict({"observation": base, "achieved_goal": goal, "desired_goal": goal})
        self._achieved = self._goal(0.0, 0.0, 0.0, 0.0, 1.0)
        self._desired = self._achieved.copy()

    @staticmethod
    def _goal(cos: float, sin: float, potted: float, scratch: float, no_contact: float) -> np.ndarray:
        return np.array([cos, sin, potted, scratch, no_contact], dtype=np.float32)

    def _direction_to(self, point) -> tuple[float, float]:
        d = np.asarray(point, dtype=np.float64) - self._positions[1]
        d /= max(np.linalg.norm(d), 1e-12)
        return float(d[0]), float(d[1])

    def compute_reward(self, achieved_goal, desired_goal, info) -> np.ndarray:
        achieved = np.asarray(achieved_goal, dtype=np.float64)
        desired = np.asarray(desired_goal, dtype=np.float64)
        norm_a = np.linalg.norm(achieved[..., :2], axis=-1)
        norm_d = np.linalg.norm(desired[..., :2], axis=-1)
        cosine = np.sum(achieved[..., :2] * desired[..., :2], axis=-1) / np.maximum(norm_a * norm_d, 1e-12)
        scratch = achieved[..., 3] > 0.5
        met = ((np.abs(achieved[..., 2] - desired[..., 2]) < 0.5) & (achieved[..., 4] < 0.5) & ~scratch
               & (norm_a > 0.5) & (cosine > self.tolerance_cos))
        return np.where(scratch, -1.0, np.where(met, 1.0, -0.1))

    def reset(self, *, seed: int | None = None, options: dict[str, Any] | None = None):
        observation, info = super().reset(seed=seed, options=options)
        if options and "desired_goal" in options:
            self._desired = np.asarray(options["desired_goal"], dtype=np.float32)
        else:
            cos, sin = self._direction_to(self.mouths[max(self.target_pocket, 0)])
            self._desired = self._goal(cos, sin, 1.0, 0.0, 0.0)
        self._achieved = self._goal(0.0, 0.0, 0.0, 0.0, 1.0)
        return self._obs(observation), info

    def step(self, action):
        result, info = self._play(action)
        scratch = 1.0 if result.scratch else 0.0
        if result.first_contact != 1:
            self._achieved = self._goal(0.0, 0.0, 0.0, scratch, 1.0)
        elif result.potted:
            cos, sin = self._direction_to(self.mouths[result.object_pocket])
            self._achieved = self._goal(cos, sin, 1.0, scratch, 0.0)
        else:
            v = info["contact_velocity"]
            v = v / max(np.linalg.norm(v), 1e-12)
            self._achieved = self._goal(float(v[0]), float(v[1]), 0.0, scratch, 0.0)
        reward = float(self.compute_reward(self._achieved, self._desired, info))
        info["is_success"] = reward > 0.0
        return self._obs(self._observe()), reward, True, False, info

    def _obs(self, observation: np.ndarray) -> dict[str, np.ndarray]:
        return {"observation": observation, "achieved_goal": self._achieved.copy(), "desired_goal": self._desired.copy()}
