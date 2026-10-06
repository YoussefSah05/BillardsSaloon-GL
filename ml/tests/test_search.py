"""The expectimax player (Milestone 12)."""

import numpy as np

from billiards_rl.hierarchy.executors import GeometryExecutor
from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.search.expectimax import ExpectimaxPlayer


def test_candidates_cover_every_goal_with_variations():
    env = RunEnv(balls=3)
    env.reset(seed=0)
    player = ExpectimaxPlayer(GeometryExecutor(), depth=1, samples=2)
    shots = player.candidates(env)
    assert len(shots) == 9 * len(env.candidates)
    assert all(0.5 <= s[0] <= 7.0 and s[4] in (-0.3, 0.0, 0.3) for _, _, s in shots)


def test_search_plays_whole_racks_at_both_depths():
    env = RunEnv(balls=3, noise=(0.25, 0.0))
    for depth in (1, 2):
        player = ExpectimaxPlayer(GeometryExecutor(), depth=depth, samples=4, leaf_samples=2, seed=1)
        for seed in range(5):
            env.reset(seed=seed)
            total = player.run_rack(env)
            assert -1.0 <= total <= 3.0


def test_leaf_values_are_probabilities():
    env = RunEnv(balls=3)
    env.reset(seed=3)
    player = ExpectimaxPlayer(GeometryExecutor(), depth=2, samples=2, leaf_samples=4, sigma=0.25)
    values = player._leaf_values([env.positions, env.positions[[0, 1, 2]]])
    assert values.shape == (2,) and np.all((values >= 0) & (values <= 1))
