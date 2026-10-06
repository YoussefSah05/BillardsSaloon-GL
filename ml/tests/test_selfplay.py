"""Expert iteration pieces (Milestone 13)."""

import numpy as np
import torch

from billiards_rl.hierarchy.executors import GeometryExecutor
from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.search.expectimax import ExpectimaxPlayer
from billiards_rl.selfplay.expert_iteration import play_racks
from billiards_rl.selfplay.value_net import LearnedValue, StateEncoder, ValueNet, fit_value


def _states():
    env = RunEnv(balls=3)
    env.reset(seed=4)
    return [env.positions, env.positions[[0, 2, 3]], env.positions[[0, 1]], env.positions[[0]]]


def test_encoder_masks_missing_balls():
    balls, mask, glob = StateEncoder().encode(_states())
    assert balls.shape == (4, 3, 7) and mask.sum(1).tolist() == [3, 2, 1, 0]
    assert np.all(balls[~mask] == 0) and np.allclose(glob[:, 2], [1, 2 / 3, 1 / 3, 0])


def test_value_is_permutation_invariant():
    torch.manual_seed(0)
    value = LearnedValue(ValueNet())
    s = _states()[0]
    v = value([s, s[[0, 3, 1, 2]]])
    assert v.shape == (2,) and abs(v[0] - v[1]) < 1e-5


def test_returns_to_go_and_learned_leaf():
    p = ExpectimaxPlayer(GeometryExecutor(), depth=2, samples=2, leaf_samples=1, sigma=0.25, threads=1)
    states, targets, totals = play_racks(p, 6, 0.25, seed=0)
    assert len(states) == len(targets) >= 6 and np.all(targets <= 3) and np.all(targets >= -1)
    assert np.isclose(sum(t for t, s in zip(targets, states) if len(s) == 4), totals.sum())   # first turn of every rack
    net = ValueNet()
    fit = fit_value(net, *StateEncoder().encode(states), targets, epochs=2)
    assert np.isfinite(fit["val_mse"])
    learned = ExpectimaxPlayer(GeometryExecutor(), depth=2, samples=2, sigma=0.25, threads=1, value_fn=LearnedValue(net))
    env = RunEnv(balls=3, noise=(0.25, 0.0))
    env.reset(seed=1)
    assert -1.0 <= learned.run_rack(env) <= 3.0
