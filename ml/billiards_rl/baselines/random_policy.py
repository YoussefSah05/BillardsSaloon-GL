"""The floor: uniform actions over the action box."""

import numpy as np


class RandomPolicy:
    name = "random"
    privileged = False

    def __init__(self, seed: int = 0):
        self._rng = np.random.default_rng(seed)

    def act(self, env, obs):
        low, high = env.action_space.low, env.action_space.high
        return self._rng.uniform(low, high).astype(np.float32)
