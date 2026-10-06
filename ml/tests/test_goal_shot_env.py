"""GoalShotEnv: goal semantics, rewards and compatibility with HER."""

import json
from pathlib import Path

import numpy as np
import pytest
import yaml
from gymnasium.utils.env_checker import check_env

import billiards_sim as bs
from billiards_rl.envs import GoalShotEnv
from billiards_rl.training.train import apply_overrides, train

rl = bs.rl


@pytest.mark.parametrize("frame", ["absolute", "ghost_relative"])
def test_passes_gymnasium_checks(frame):
    check_env(GoalShotEnv(action_frame=frame), skip_render_check=True)


def test_desired_goal_is_the_target_pockets_mouth():
    env = GoalShotEnv(band="medium")
    obs, _ = env.reset(seed=0)
    mouth = env.mouths[env.target_pocket]
    np.testing.assert_allclose(env._metres(obs["desired_goal"][:2]), mouth, atol=1e-6)
    assert obs["desired_goal"][2] == 1.0 and obs["desired_goal"][3] == 0.0
    assert obs["achieved_goal"][2] == 0.0          # before the shot: the ball at rest


def test_compute_reward_follows_the_goal_rules():
    env = GoalShotEnv()
    env.reset(seed=1)
    pocket = env._goal_for_pocket(2)
    other = env._goal_for_pocket(4)
    spot = np.array([0.1, -0.2, 0.0, 0.0])
    near_spot = spot + np.array([0.01, 0.0, 0.0, 0.0])          # well within 10 cm
    near_mouth_resting = pocket.copy(); near_mouth_resting[2] = 0.0
    scratched = pocket.copy(); scratched[3] = 1.0
    achieved = np.stack([pocket, other, near_spot, near_mouth_resting, scratched])
    desired = np.stack([pocket, pocket, spot, pocket, pocket])
    np.testing.assert_allclose(env.compute_reward(achieved, desired, None), [1.0, -0.1, 1.0, -0.1, -1.0])


def test_rewards_match_outcomes_and_success_flags():
    env = GoalShotEnv(band="easy")
    env.reset(seed=2)
    for _ in range(100):
        shot = rl.geometry_shot(env.positions, pocket=env.target_pocket)
        if shot is None:
            env.reset()
            continue
        _, reward, _, _, info = env.step(env.action_for(shot["strike"][1], shot["strike"][0]))
        if info["scratch"]:
            assert reward == -1.0
        elif info["target_success"]:
            assert reward == 1.0 and info["is_success"]
        else:
            assert reward == pytest.approx(-0.1)
        env.reset()


def test_a_short_her_run_trains_and_evaluates(tmp_path):
    config_path = Path(__file__).resolve().parents[1] / "configs" / "sac_her.yaml"
    config = apply_overrides(yaml.safe_load(config_path.read_text()), [
        "training.total_timesteps=500", "training.eval_every=250", "sac.learning_starts=100",
        "eval.during_training_limit=20", "eval.final_suites=[easy]", "eval.final_noise=[0.0]",
    ])
    run = train(config, seed=5, runs_root=tmp_path, name="her-run")
    final = json.loads((run / "eval" / "final-easy-noise0.json").read_text())
    assert final["episodes"] == 1000
    assert 0.0 <= final["target_success_rate"] <= final["success_rate"] <= 1.0
