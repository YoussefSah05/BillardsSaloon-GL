"""Train a policy on SingleShotEnv from a config file.

    uv run python -m billiards_rl.training.train --config configs/sac_single_ball.yaml --seed 42
    uv run python -m billiards_rl.training.train --config configs/sac_single_ball.yaml --seed 1 \
        --set env.action_frame=ghost_relative training.total_timesteps=50000

Everything a result depends on is recorded in the run folder (run_logger.py).
"""

from __future__ import annotations

import argparse
import copy
import time
from pathlib import Path

import numpy as np
import torch
import yaml
from stable_baselines3 import PPO, SAC, TD3
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.logger import configure
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.noise import NormalActionNoise

from billiards_rl.curriculum import Curriculum
from billiards_rl.envs.single_shot_env import SingleShotEnv
from billiards_rl.evaluation.evaluate import evaluate
from billiards_rl.training.run_logger import RunLogger

ALGORITHMS = {"sac": SAC, "td3": TD3, "ppo": PPO}


def apply_overrides(config: dict, overrides: list[str]) -> dict:
    """`a.b=value` pairs, values parsed as YAML."""
    config = copy.deepcopy(config)
    for item in overrides:
        key, _, raw = item.partition("=")
        node = config
        parts = key.split(".")
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node[parts[-1]] = yaml.safe_load(raw)
    return config


def make_env(env_config: dict) -> SingleShotEnv:
    return SingleShotEnv(
        band=env_config.get("band", "any"),
        obs_mode=env_config.get("obs_mode", "positions"),
        action_frame=env_config.get("action_frame", "absolute"),
        reward=env_config.get("reward", "sparse"),
        noise=tuple(env_config.get("noise", (0.0, 0.0))),
    )


class TrainedPolicy:
    """The model being trained, as a deterministic policy for evaluate()."""

    def __init__(self, model, name: str):
        self.model = model
        self.name = name

    def act(self, env, obs):
        action, _ = self.model.predict(obs, deterministic=True)
        return action


class EvaluationCallback(BaseCallback):
    """Evaluates on part of a fixed suite every `every` steps; keeps the best."""

    def __init__(self, run: RunLogger, env_config: dict, suite: str, limit: int, every: int, name: str,
                 noise: tuple[float, float] = (0.0, 0.0), curriculum: Curriculum | None = None):
        super().__init__()
        self.curriculum = curriculum
        self.run, self.env_config, self.suite, self.limit, self.every, self.name = run, env_config, suite, limit, every, name
        self.noise = noise
        self.best = -1.0
        self.history: list[tuple[int, float]] = []

    def _on_step(self) -> bool:
        if self.num_timesteps % self.every != 0:
            return True
        metrics, _ = evaluate(TrainedPolicy(self.model, self.name), self.suite, self.noise, action_frame=self.env_config["action_frame"],
                              obs_mode=self.env_config["obs_mode"], limit=self.limit)
        self.history.append((self.num_timesteps, metrics.success_rate))
        if self.curriculum is not None:
            self.logger.record("curriculum/stage", self.curriculum.stage)
            self.logger.record("curriculum/success_rate", self.curriculum.success_rate)
        self.logger.record("eval/success_rate", metrics.success_rate)
        self.logger.record("eval/scratch_rate", metrics.scratch_rate)
        self.run.log_eval(f"step{self.num_timesteps:08d}-{self.suite}", metrics)
        if metrics.success_rate > self.best:
            self.best = metrics.success_rate
            self.model.save(self.run.dir / "checkpoints" / "best")
        return True


def train(config: dict, seed: int, runs_root: Path = Path("runs"), name: str | None = None) -> Path:
    algorithm = config["algorithm"]
    env_config = config["env"]
    training = config["training"]
    run = RunLogger(runs_root, config, seed, name)
    torch.set_num_threads(1)   # one core per run: several seeds run side by side

    curriculum = None
    base_env = make_env(env_config)
    curriculum_config = config.get("curriculum") or {}
    if curriculum_config.get("enabled", False):
        curriculum = Curriculum(base_env, curriculum_config["stages"], curriculum_config.get("threshold", 0.8),
                                curriculum_config.get("window", 2000))
        base_env = curriculum
    env = Monitor(base_env)
    env.reset(seed=seed)
    kwargs = dict(config.get(algorithm, {}))
    if algorithm == "td3" and "action_noise_sigma" in kwargs:
        sigma = kwargs.pop("action_noise_sigma")
        kwargs["action_noise"] = NormalActionNoise(np.zeros(2), sigma * np.ones(2))
    model = ALGORITHMS[algorithm]("MlpPolicy", env, seed=seed, device=training.get("device", "cpu"), verbose=0, **kwargs)
    model.set_logger(configure(str(run.dir), ["csv", "tensorboard"]))

    evaluation = config["eval"]
    # Checkpoints are selected under the noise the policy trains with: a
    # robust policy is chosen for robustness, a precise one for precision.
    selection_noise = tuple(evaluation.get("during_training_noise", env_config.get("noise", (0.0, 0.0))))
    callback = EvaluationCallback(run, env_config, evaluation["during_training_suite"],
                                  evaluation["during_training_limit"], training["eval_every"], f"{algorithm}:{run.dir.name}",
                                  selection_noise, curriculum)
    start = time.perf_counter()
    # One episode is one step: log every 1,000 episodes, not SB3's default 4.
    model.learn(total_timesteps=training["total_timesteps"], callback=callback, log_interval=1000, progress_bar=False)
    seconds = time.perf_counter() - start
    model.save(run.dir / "checkpoints" / "last")

    # Final evaluation of the best checkpoint on every suite and noise level.
    best = ALGORITHMS[algorithm].load(run.dir / "checkpoints" / "best", device="cpu")
    policy = TrainedPolicy(best, f"{algorithm}:{run.dir.name}")
    final = {}
    for suite in evaluation["final_suites"]:
        for sigma in evaluation["final_noise"]:
            metrics, _ = evaluate(policy, suite, (sigma, 0.0), action_frame=env_config["action_frame"],
                                  obs_mode=env_config["obs_mode"])
            run.log_eval(f"final-{suite}-noise{sigma:g}", metrics)
            final[f"{suite}@{sigma:g}"] = metrics.success_rate
    if curriculum is not None:
        run.meta["curriculum_history"] = curriculum.history   # (episode, stage) at each advance
        run.meta["curriculum_final_stage"] = curriculum.stage
    run.finish(training_seconds=round(seconds, 1), steps_per_second=round(training["total_timesteps"] / seconds, 1),
               best_during_training=callback.best, final_success=final)
    print(f"{run.dir}: {seconds:.0f}s, best on {evaluation['during_training_suite']} {callback.best:.3f}, final {final}", flush=True)
    return run.dir


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--set", nargs="*", default=[], help="overrides, e.g. env.action_frame=ghost_relative")
    parser.add_argument("--name")
    args = parser.parse_args()
    config = apply_overrides(yaml.safe_load(Path(args.config).read_text()), args.set)
    train(config, args.seed, name=args.name)


if __name__ == "__main__":
    main()
