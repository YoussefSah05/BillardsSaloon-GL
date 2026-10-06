"""A success-gated curriculum over SingleShotEnv's layout limits.

Stages are data (the config's `curriculum.stages`): each sets the limits of
the shots generated (cut angle, cue and object ball distances), and the last
stage may be `null` for the full training distribution. The curriculum moves
to the next stage when the success rate over the last `window` episodes of the
current stage exceeds `threshold`. It never moves back. Switching it off is
the ablation.
"""

from __future__ import annotations

from collections import deque

import gymnasium as gym


class Curriculum(gym.Wrapper):
    def __init__(self, env, stages: list[dict | None], threshold: float = 0.8, window: int = 2000):
        super().__init__(env)
        if not stages:
            raise ValueError("A curriculum needs at least one stage")
        self.stages = stages
        self.threshold = threshold
        self.window = window
        self.stage = 0
        self.episodes = 0
        self._recent: deque[bool] = deque(maxlen=window)
        self.history: list[tuple[int, int]] = [(0, 0)]   # (episode, stage) at each change
        self._apply()

    def _apply(self) -> None:
        self.env.unwrapped.limits = self.stages[self.stage]

    @property
    def success_rate(self) -> float:
        return sum(self._recent) / len(self._recent) if self._recent else 0.0

    def step(self, action):
        obs, reward, terminated, truncated, info = self.env.step(action)
        self.episodes += 1
        self._recent.append(bool(info.get("success", False)))
        if (len(self._recent) == self.window and self.success_rate > self.threshold
                and self.stage + 1 < len(self.stages)):
            self.stage += 1
            self._recent.clear()
            self.history.append((self.episodes, self.stage))
            self._apply()
        info = {**info, "curriculum_stage": self.stage}
        return obs, reward, terminated, truncated, info
