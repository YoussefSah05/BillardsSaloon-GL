"""A success-gated curriculum over SingleShotEnv's layout limits.

Stages are data (the config's `curriculum.stages`): each sets the limits of
the shots generated (cut angle, cue and object ball distances), and the last
stage may be `null` for the full training distribution. The curriculum moves
to the next stage when:

- the success rate over the last `window` episodes of the current stage
  exceeds `threshold`; or
- (optional) it has plateaued: no improvement of at least
  `plateau_improvement` in its best windowed success for `plateau_episodes`
  episodes, so it advances rather than freezing below the gate
  (Experiment 4).

It never moves back. With `review_fraction` > 0, that share of episodes draws
a layout from a random earlier stage, to keep earlier skills from being
forgotten. Review episodes do not count towards advancing. Switching the
curriculum off is the ablation.
"""

from __future__ import annotations

from collections import deque

import gymnasium as gym
import numpy as np


class Curriculum(gym.Wrapper):
    def __init__(self, env, stages: list[dict | None], threshold: float = 0.8, window: int = 2000,
                 plateau_episodes: int | None = None, plateau_improvement: float = 0.02,
                 review_fraction: float = 0.0, seed: int = 0):
        super().__init__(env)
        if not stages:
            raise ValueError("A curriculum needs at least one stage")
        if not 0.0 <= review_fraction < 1.0:
            raise ValueError("review_fraction must be in [0, 1)")
        self.stages = stages
        self.threshold = threshold
        self.window = window
        self.plateau_episodes = plateau_episodes
        self.plateau_improvement = plateau_improvement
        self.review_fraction = review_fraction
        self._rng = np.random.default_rng(seed)
        self.stage = 0
        self.episodes = 0
        self._recent: deque[bool] = deque(maxlen=window)
        self._best = -1.0
        self._since_best = 0
        self._reviewing = False
        # (episode, stage, reason) at each change; reason "start", "threshold" or "plateau".
        self.history: list[tuple[int, int, str]] = [(0, 0, "start")]
        self.env.unwrapped.limits = self.stages[0]

    @property
    def success_rate(self) -> float:
        return sum(self._recent) / len(self._recent) if self._recent else 0.0

    def reset(self, **kwargs):
        # Choose this episode's stage: usually the current one, sometimes an
        # earlier one for review.
        self._reviewing = (self.stage > 0) and (self._rng.random() < self.review_fraction)
        stage = int(self._rng.integers(0, self.stage)) if self._reviewing else self.stage
        self.env.unwrapped.limits = self.stages[stage]
        return self.env.reset(**kwargs)

    def _advance(self, reason: str) -> None:
        self.stage += 1
        self._recent.clear()
        self._best = -1.0
        self._since_best = 0
        self.history.append((self.episodes, self.stage, reason))

    def step(self, action):
        obs, reward, terminated, truncated, info = self.env.step(action)
        self.episodes += 1
        if not self._reviewing:
            self._recent.append(bool(info.get("success", False)))
            if len(self._recent) == self.window:
                rate = self.success_rate
                if rate >= self._best + self.plateau_improvement:
                    self._best = rate
                    self._since_best = 0
                else:
                    self._since_best += 1
                if self.stage + 1 < len(self.stages):
                    if rate > self.threshold:
                        self._advance("threshold")
                    elif self.plateau_episodes is not None and self._since_best >= self.plateau_episodes:
                        self._advance("plateau")
        info = {**info, "curriculum_stage": self.stage, "curriculum_review": self._reviewing}
        return obs, reward, terminated, truncated, info
