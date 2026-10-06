"""The training pipeline runs end to end and records what a result depends on."""

import json
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
