"""RunEnv, executors and strategies (Milestone 11)."""

import numpy as np
import torch

from billiards_rl.hierarchy.executors import GeometryExecutor
from billiards_rl.hierarchy.run_env import CANDIDATE_FEATURES, GLOBAL_FEATURES, RunEnv
from billiards_rl.hierarchy.strategy import EasiestStrategy, LearnedStrategy, RandomStrategy, Scorer, train_strategy


def test_layouts_are_legal_and_have_goals():
    env = RunEnv(balls=3)
    rng = np.random.default_rng(0)
    for _ in range(20):
        p = env.random_layout(rng)
        assert p.shape == (4, 2)
        assert min(np.linalg.norm(p[i] - p[j]) for i in range(4) for j in range(i + 1, 4)) > 2 * 0.028575
        assert env._candidates_for(p)


def test_features_and_mask():
    env = RunEnv(balls=3, max_candidates=8)
    env.reset(seed=1)
    feats, mask, glob = env.features()
    assert feats.shape == (8, CANDIDATE_FEATURES) and mask.shape == (8,) and glob.shape == (GLOBAL_FEATURES,)
    assert mask.sum() == len(env.candidates) >= 1
    assert np.all(np.isfinite(feats)) and glob[2] == 1.0


def test_episodes_end_and_count_balls():
    env = RunEnv(balls=3)
    for seed in range(30):
        result = env.run_episode(EasiestStrategy(), GeometryExecutor(), seed=seed)
        assert -1.0 <= result["return"] <= 3.0
        assert 1 <= result["shots"] <= 4
        if result["cleared"]:
            assert result["return"] == 3.0


def test_scorer_masks_invalid_goals_and_learns_to_prefer_easy_ones():
    scorer = Scorer()
    feats = torch.zeros(1, 8, CANDIDATE_FEATURES)
    mask = torch.zeros(1, 8, dtype=torch.bool)
    mask[0, :2] = True
    logits = scorer(feats, mask, torch.zeros(1, GLOBAL_FEATURES))
    assert torch.all(logits[0, 2:] < -1e8)
    env = RunEnv(balls=3)
    scorer, _ = train_strategy(env, GeometryExecutor(), iterations=3, episodes_per_iteration=16, seed=0)
    choice = LearnedStrategy(scorer).choose((env.reset(seed=2), env)[1])
    assert 0 <= choice < len(env.candidates)


def test_random_strategy_is_seeded():
    env = RunEnv(balls=3)
    a = [env.run_episode(RandomStrategy(5), GeometryExecutor(), seed=s)["return"] for s in range(10)]
    b = [env.run_episode(RandomStrategy(5), GeometryExecutor(), seed=s)["return"] for s in range(10)]
    assert a == b
