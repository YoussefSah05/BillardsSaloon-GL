"""The training pipeline runs end to end and records what a result depends on."""

import json

import numpy as np
import pytest
from pathlib import Path

import yaml

from billiards_rl.training.train import apply_overrides, train

CONFIG = Path(__file__).resolve().parents[1] / "configs" / "sac_single_ball.yaml"


def test_overrides_parse_yaml_values():
    config = apply_overrides({"env": {"noise": [0, 0]}}, ["env.noise=[0.25, 0.0]", "training.total_timesteps=10", "env.action_frame=ghost_relative"])
    assert config["env"]["noise"] == [0.25, 0.0]
    assert config["training"]["total_timesteps"] == 10
    assert config["env"]["action_frame"] == "ghost_relative"


def test_a_short_sac_run_writes_a_complete_run_folder(tmp_path):
    config = apply_overrides(yaml.safe_load(CONFIG.read_text()), [
        "training.total_timesteps=600", "training.eval_every=300", "sac.learning_starts=100",
        "eval.during_training_limit=20", "eval.final_suites=[easy]", "eval.final_noise=[0.0]",
    ])
    run = train(config, seed=3, runs_root=tmp_path, name="test-run")
    assert (run / "config.yaml").exists()
    assert (run / "progress.csv").exists()
    assert (run / "checkpoints" / "best.zip").exists()
    assert (run / "checkpoints" / "last.zip").exists()
    meta = json.loads((run / "meta.json").read_text())
    for key in ("seed", "env_version", "git_commit", "platform", "torch", "stable_baselines3", "final_success"):
        assert key in meta
    final = json.loads((run / "eval" / "final-easy-noise0.json").read_text())
    assert final["episodes"] == 1000
    assert 0.0 <= final["success_rate"] <= 1.0


def test_curriculum_advances_on_success_and_records_it():
    import numpy as np
    import billiards_sim as bs
    from billiards_rl.curriculum import Curriculum
    from billiards_rl.envs import SingleShotEnv

    stages = [{"max_cut": 5, "max_cue_distance": 0.4, "max_object_distance": 0.4}, None]
    env = Curriculum(SingleShotEnv(), stages, threshold=0.5, window=20)
    env.reset(seed=0)
    assert env.unwrapped.limits == stages[0]
    # The geometry shot succeeds on these easy layouts, so the stage advances.
    for _ in range(60):
        shot = bs.rl.geometry_shot(env.unwrapped.positions)
        _, _, _, _, info = env.step(env.unwrapped.action_for(shot["strike"][1], shot["strike"][0]))
        env.reset()
        if env.stage == 1:
            break
    assert env.stage == 1
    assert env.unwrapped.limits is None
    assert env.history[-1][1] == 1
    assert env.history[-1][2] == "threshold"
    assert info["curriculum_stage"] in (0, 1)
    # Stage-1 layouts respect the limits.
    env2 = SingleShotEnv(limits=stages[0])
    env2.reset(seed=1)
    for _ in range(50):
        aims = bs.rl.ghost_aims(env2.positions)
        assert np.any((aims["cut"] <= 5.0 + 1e-6) & (aims["cue_distance"] <= 0.4 + 1e-6))
        env2.reset()


def test_a_short_curriculum_run_records_its_history(tmp_path):
    config = apply_overrides(yaml.safe_load((CONFIG.parent / "sac_curriculum.yaml").read_text()), [
        "training.total_timesteps=400", "training.eval_every=200", "sac.learning_starts=100",
        "eval.during_training_limit=20", "eval.final_suites=[easy]", "eval.final_noise=[0.0]",
    ])
    run = train(config, seed=4, runs_root=tmp_path, name="curriculum-run")
    meta = json.loads((run / "meta.json").read_text())
    assert meta["curriculum_history"][0] == [0, 0, "start"]
    assert "curriculum_final_stage" in meta


def test_curriculum_advances_on_a_plateau_and_reviews_earlier_stages():
    from billiards_rl.curriculum import Curriculum
    from billiards_rl.envs import SingleShotEnv

    stages = [{"max_cut": 5, "max_cue_distance": 0.4, "max_object_distance": 0.4},
              {"max_cut": 25, "max_cue_distance": 0.5, "max_object_distance": 0.5}, None]
    # A policy that always misses: success never improves, so the plateau rule fires.
    env = Curriculum(SingleShotEnv(), stages, threshold=0.9, window=10, plateau_episodes=15, review_fraction=0.5, seed=1)
    env.reset(seed=0)
    reviews = 0
    for _ in range(120):
        _, _, _, _, info = env.step([0.0, -1.0])    # a soft shot along +x: (almost) never pots
        reviews += int(info["curriculum_review"])
        env.reset()
    assert env.stage == 2
    assert [h[2] for h in env.history[1:]] == ["plateau", "plateau"]
    assert reviews > 10                             # earlier stages are revisited


@pytest.mark.parametrize("method", ["elite", "awr"])
def test_a_short_elite_run_learns_and_records(tmp_path, method):
    from billiards_rl.training.models import load_policy_model
    from billiards_rl.training.train_elite import train_elite

    config = apply_overrides(yaml.safe_load((CONFIG.parent / "elite_single_ball.yaml").read_text()), [
        f"algorithm={method}", "training.total_timesteps=3000", "training.eval_every=1000",
        "eval.during_training_limit=50", "eval.final_suites=[easy]", "eval.final_noise=[0.0]",
    ])
    run = train_elite(config, seed=2, runs_root=tmp_path, name=f"{method}-run")
    assert (run / "checkpoints" / "best.pt").exists()
    meta = json.loads((run / "meta.json").read_text())
    assert meta["final_success"]["easy@0"] > 0.3        # ghost-relative: learns quickly
    agent = load_policy_model(method, run / "checkpoints" / "best")
    action, _ = agent.predict(np.zeros(6, dtype=np.float32))
    assert action.shape == (2,) and np.all(np.abs(action) <= 1.0)
