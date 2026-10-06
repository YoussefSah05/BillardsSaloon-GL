"""DirectionGoalEnv: direction goals, exact pocket goals, compatibility with HER."""

import numpy as np
import pytest
from gymnasium.utils.env_checker import check_env

import billiards_sim as bs
from billiards_rl.envs import DirectionGoalEnv

rl = bs.rl


@pytest.mark.parametrize("frame", ["absolute", "ghost_relative"])
def test_passes_gymnasium_checks(frame):
    check_env(DirectionGoalEnv(action_frame=frame), skip_render_check=True)


def test_rewards_follow_the_goal_rules():
    env = DirectionGoalEnv(tolerance_deg=1.0)
    env.reset(seed=0)
    d = np.array([1.0, 0.0, 0.0, 0.0, 0.0])
    near = np.array([np.cos(np.radians(0.5)), np.sin(np.radians(0.5)), 0, 0, 0])
    far = np.array([np.cos(np.radians(3.0)), np.sin(np.radians(3.0)), 0, 0, 0])
    no_contact = np.array([0.0, 0.0, 0.0, 0.0, 1.0])
    scratched = near.copy(); scratched[3] = 1.0
    potted_same_dir = near.copy(); potted_same_dir[2] = 1.0
    achieved = np.stack([near, far, no_contact, scratched, potted_same_dir])
    desired = np.stack([d] * 5)
    np.testing.assert_allclose(env.compute_reward(achieved, desired, None), [1.0, -0.1, -0.1, -1.0, -0.1])


def test_a_pot_in_the_target_pocket_meets_the_pocket_goal():
    env = DirectionGoalEnv(band="easy")
    env.reset(seed=1)
    met = 0
    for _ in range(100):
        shot = rl.geometry_shot(env.positions, pocket=env.target_pocket)
        if shot is None:
            env.reset()
            continue
        _, reward, _, _, info = env.step(env.action_for(shot["strike"][1], shot["strike"][0]))
        assert (reward == 1.0) == bool(info["target_success"])
        met += reward == 1.0
        env.reset()
    assert met > 60


def test_a_contact_achieves_its_departure_direction():
    env = DirectionGoalEnv()
    env.reset(seed=2, options={"positions": np.array([[0.5, 0.8], [0.6, 1.4]])})
    _, _, _, _, info = env.step(env.action_for(np.degrees(np.arctan2(0.6, 0.1)) + 1.0, 2.0))
    assert info["first_contact"] == 1
    v = info["contact_velocity"] / np.linalg.norm(info["contact_velocity"])
    if not info["potted"]:
        np.testing.assert_allclose(env._achieved[:2], v, atol=1e-6)
    assert env._achieved[4] == 0.0
