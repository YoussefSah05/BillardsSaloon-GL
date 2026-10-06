"""SingleShotEnv: the critical environment tests of rl-system-design.md, Section 12."""

import numpy as np
import pytest
from gymnasium.utils.env_checker import check_env

import billiards_sim as bs
from billiards_rl.envs import SingleShotEnv
from billiards_rl.rewards.single_shot import ShotResult, sparse, sparse_target

rl = bs.rl


@pytest.mark.parametrize("obs_mode", ["positions", "positions+geometry", "positions+target"])
@pytest.mark.parametrize("frame", ["absolute", "ghost_relative"])
def test_passes_gymnasium_checks(obs_mode, frame):
    check_env(SingleShotEnv(obs_mode=obs_mode, action_frame=frame), skip_render_check=True)


@pytest.mark.parametrize("obs_mode", ["positions", "positions+geometry", "positions+target"])
def test_observations_have_a_stable_shape_and_stay_in_bounds(obs_mode):
    env = SingleShotEnv(obs_mode=obs_mode)
    obs, _ = env.reset(seed=0)
    for _ in range(200):
        assert obs.shape == env.observation_space.shape
        assert obs.dtype == np.float32
        assert np.all(np.isfinite(obs))
        assert env.observation_space.contains(obs)
        obs, *_ = env.step(env.action_space.sample())
        obs, _ = env.reset()


def test_same_seed_gives_the_same_layouts():
    a, b = SingleShotEnv(band="medium"), SingleShotEnv(band="medium")
    a.reset(seed=123)
    b.reset(seed=123)
    for _ in range(20):
        np.testing.assert_array_equal(a.positions, b.positions)
        a.reset()
        b.reset()
    c = SingleShotEnv(band="medium")
    c.reset(seed=124)
    assert not np.array_equal(a.positions, c.positions)


def test_actions_map_into_physical_bounds():
    env = SingleShotEnv()
    env.reset(seed=1)
    for a0 in (-1.0, -0.3, 0.0, 1.0, 5.0):
        for a1 in (-1.0, 0.0, 1.0, -9.0):
            strike, phi, speed = env.strike_for([a0, a1])
            assert 0.0 <= phi < 360.0
            assert env.v_min <= speed <= env.v_max
            assert strike[2:].tolist() == [0.0, 0.0, 0.0]


def test_action_for_inverts_strike_for():
    for frame in ("absolute", "ghost_relative"):
        env = SingleShotEnv(action_frame=frame)
        env.reset(seed=2)
        target_phi = env.reference_phi() + 1.5
        action = env.action_for(target_phi, 3.25)
        _, phi, speed = env.strike_for(action)
        assert phi == pytest.approx(target_phi % 360.0, abs=1e-3)
        assert speed == pytest.approx(3.25, abs=1e-4)


def test_every_episode_terminates_after_one_shot():
    env = SingleShotEnv()
    env.reset(seed=3)
    for _ in range(100):
        _, _, terminated, truncated, info = env.step(env.action_space.sample())
        assert terminated and not truncated
        assert info["complete"]
        env.reset()


def test_reward_matches_the_outcome():
    env = SingleShotEnv()
    env.reset(seed=4)
    for _ in range(300):
        _, reward, _, _, info = env.step(env.action_space.sample())
        if info["scratch"]:
            assert reward == -1.0
        elif info["potted"]:
            assert reward == 1.0
        else:
            assert reward == pytest.approx(-0.1)
        env.reset()


def test_the_geometry_shot_through_either_frame_pots_easy_layouts():
    for frame in ("absolute", "ghost_relative"):
        env = SingleShotEnv(band="easy", action_frame=frame)
        env.reset(seed=5)
        successes = 0
        for _ in range(100):
            shot = rl.geometry_shot(env.positions)
            speed, phi = shot["strike"][0], shot["strike"][1]
            _, _, _, _, info = env.step(env.action_for(phi, speed))
            successes += int(info["success"])
            env.reset()
        assert successes >= 70, frame


def test_fixed_layouts_cycle_in_order():
    gen = rl.LayoutGenerator(9)
    layouts = [gen.single_ball("hard")["positions"] for _ in range(3)]
    env = SingleShotEnv(layouts=layouts)
    for i in range(6):
        env.reset(seed=0 if i == 0 else None)
        np.testing.assert_array_equal(env.positions, layouts[i % 3])


def test_given_positions_override_generation():
    positions = np.array([[0.4, 0.8], [0.7, 1.6]])
    env = SingleShotEnv()
    env.reset(seed=0, options={"positions": positions})
    np.testing.assert_array_equal(env.positions, positions)


def test_execution_noise_changes_shots_but_not_actions():
    clean = SingleShotEnv(band="easy")
    noisy = SingleShotEnv(band="easy", noise=(0.5, 0.05))
    clean.reset(seed=6)
    noisy.reset(seed=6)
    differs = 0
    for _ in range(50):
        shot = rl.geometry_shot(clean.positions)
        action = clean.action_for(shot["strike"][1], shot["strike"][0])
        _, _, _, _, a = clean.step(action)
        _, _, _, _, b = noisy.step(action)
        assert a["phi"] == b["phi"]          # the intended shot is the same
        differs += int(a["success"] != b["success"])
        clean.reset()
        noisy.reset()
    assert differs > 0


def test_rewards_follow_their_definitions():
    pot = ShotResult(object_pocket=2, scratch=False, first_contact=1, object_final=np.zeros(2), target_pocket=4)
    scratch = ShotResult(object_pocket=2, scratch=True, first_contact=1, object_final=np.zeros(2), target_pocket=2)
    miss = ShotResult(object_pocket=-1, scratch=False, first_contact=1, object_final=np.zeros(2), target_pocket=2)
    assert sparse(pot) == 1.0
    assert sparse(scratch) == -1.0
    assert sparse(miss) == pytest.approx(-0.1)
    assert sparse_target(pot) == pytest.approx(-0.1)   # wrong pocket
    assert sparse_target(scratch) == -1.0


def test_bad_configuration_is_rejected():
    with pytest.raises(ValueError):
        SingleShotEnv(obs_mode="pixels")
    with pytest.raises(ValueError):
        SingleShotEnv(action_frame="polar")
    with pytest.raises(ValueError):
        SingleShotEnv(reward="dense")
