"""PositionShotEnv: feasible targets, spin mapping, rewards, suites and baselines."""

import numpy as np
import pytest
from gymnasium.utils.env_checker import check_env

import billiards_sim as bs
from billiards_rl.baselines import GeometryPolicy, PositionSearchPolicy
from billiards_rl.envs.position_shot_env import MAX_TIP_OFFSET, PositionShotEnv
from billiards_rl.evaluation.evaluate import evaluate
from billiards_rl.evaluation.suites import load_suite, suite_cue_targets

rl = bs.rl


@pytest.mark.parametrize("spin", [True, False])
def test_passes_gymnasium_checks(spin):
    check_env(PositionShotEnv(action_frame="ghost_relative", spin=spin), skip_render_check=True)


def test_targets_are_reachable_by_construction():
    env = PositionShotEnv(action_frame="ghost_relative")
    env.reset(seed=0)
    rng = np.random.default_rng(0)
    for _ in range(20):
        target = PositionShotEnv.feasible_target(env.positions, env.target_pocket, rng)
        if target is not None:
            assert 0.0 < target[0] < env.width and 0.0 < target[1] < env.length
        env.reset()


def test_spin_maps_to_tip_offsets_and_can_be_locked():
    env = PositionShotEnv(action_frame="ghost_relative")
    env.reset(seed=1)
    strike, _, _ = env.strike_for([0.0, 0.0, 1.0, -1.0])
    assert strike[3] == pytest.approx(-MAX_TIP_OFFSET)     # right english is simulator a < 0
    assert strike[4] == pytest.approx(-MAX_TIP_OFFSET)     # draw
    locked = PositionShotEnv(action_frame="ghost_relative", spin=False)
    locked.reset(seed=1)
    strike, _, _ = locked.strike_for([0.0, 0.0, 1.0, -1.0])
    assert strike[3] == 0.0 and strike[4] == 0.0
    action = env.action_for(env.reference_phi(), 2.0, side=0.2, vertical=-0.3)
    strike, phi, speed = env.strike_for(action)
    assert strike[3] == pytest.approx(0.2, abs=1e-5) and strike[4] == pytest.approx(-0.3, abs=1e-5)


def test_reward_rewards_position_on_top_of_the_pot():
    env = PositionShotEnv(action_frame="ghost_relative", band="easy")
    env.reset(seed=2)
    seen_pot = False
    for _ in range(50):
        shot = rl.geometry_shot(env.positions, pocket=env.target_pocket)
        _, reward, _, _, info = env.step(env.action_for(shot["strike"][1], shot["strike"][0]))
        if info["scratch"]:
            assert reward == -1.0
        elif info["target_success"]:
            seen_pot = True
            expected = 1.0 + max(0.0, 1.0 - info["cue_distance"] / 0.5)
            assert reward == pytest.approx(expected)
            assert info["position_success"] == (info["cue_distance"] < 0.15)
        else:
            assert reward == pytest.approx(-0.1)
        env.reset()
    assert seen_pot


def test_position_suites_and_baselines():
    suite = load_suite("position")
    assert suite["count"] == 1000 and len(suite_cue_targets(suite)) == 1000
    config = {"task": "position", "action_frame": "ghost_relative"}
    geometry, _ = evaluate(GeometryPolicy(), "position", env_config=config, limit=60)
    search, _ = evaluate(PositionSearchPolicy(), "position", env_config=config, limit=60)
    assert search.position_success_rate > geometry.position_success_rate
    assert search.target_success_rate >= 0.9
