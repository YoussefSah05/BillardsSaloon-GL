"""Train an elite-regression or AWR agent on SingleShotEnv (Milestone 9).

    uv run python -m billiards_rl.training.train_elite --config configs/elite_single_ball.yaml --seed 1

Same run folder, validation-suite checkpoint selection and final evaluation
as the Stable-Baselines3 trainer (train.py), so results compare directly.
"""

from __future__ import annotations

import argparse
import csv
import time
from pathlib import Path

import numpy as np
import torch
import yaml

from billiards_rl.agents.elite import EliteAgent, EliteConfig
from billiards_rl.evaluation.evaluate import evaluate
from billiards_rl.training.run_logger import RunLogger
from billiards_rl.training.train import apply_overrides, make_env


class AgentPolicy:
    def __init__(self, agent: EliteAgent, name: str):
        self.agent, self.name = agent, name

    def act(self, env, obs):
        return self.agent.act(obs, deterministic=True)


def train_elite(config: dict, seed: int, runs_root: Path = Path("runs"), name: str | None = None) -> Path:
    env_config, training, evaluation = config["env"], config["training"], config["eval"]
    run = RunLogger(runs_root, config, seed, name)
    torch.set_num_threads(1)

    env = make_env(env_config)
    obs, _ = env.reset(seed=seed)
    agent_config = EliteConfig(**{**config.get("elite", {}), "method": config["algorithm"]})
    if "hidden" in config.get("elite", {}):
        agent_config.hidden = tuple(config["elite"]["hidden"])
    agent = EliteAgent(env.observation_space.shape[0], env.action_space.shape[0], agent_config, seed)
    policy = AgentPolicy(agent, f"{config['algorithm']}:{run.dir.name}")

    selection_noise = tuple(evaluation.get("during_training_noise", env_config.get("noise", (0.0, 0.0))))
    best, steps, next_eval = -1.0, 0, training["eval_every"]
    start = time.perf_counter()
    with open(run.dir / "progress.csv", "w", newline="") as progress_file:
        progress = csv.writer(progress_file)
        progress.writerow(["steps", "round_success", "elites", "mean_std", "policy_loss", "eval_success"])
        while steps < training["total_timesteps"]:
            successes = 0
            for _ in range(agent_config.batch_episodes):
                action = agent.act(obs)
                _, reward, _, _, info = env.step(action)
                agent.store(obs, action, reward, bool(reward > 0.0))
                successes += int(reward > 0.0)
                obs, _ = env.reset()
                steps += 1
            stats = agent.update()
            eval_success = ""
            if steps >= next_eval:
                next_eval += training["eval_every"]
                metrics, _ = evaluate(policy, evaluation["during_training_suite"], selection_noise,
                                      action_frame=env_config["action_frame"], obs_mode=env_config["obs_mode"],
                                      limit=evaluation["during_training_limit"])
                run.log_eval(f"step{steps:08d}-{evaluation['during_training_suite']}", metrics)
                eval_success = f"{metrics.success_rate:.4f}"
                if metrics.success_rate > best:
                    best = metrics.success_rate
                    agent.save(run.dir / "checkpoints" / "best.pt")
            progress.writerow([steps, successes / agent_config.batch_episodes, stats.get("elites", 0),
                               f"{stats.get('mean_std', float('nan')):.4f}", f"{stats.get('policy_loss', float('nan')):.4f}", eval_success])
            progress_file.flush()
    seconds = time.perf_counter() - start
    agent.save(run.dir / "checkpoints" / "last.pt")

    final_agent = EliteAgent.load(run.dir / "checkpoints" / "best.pt")
    final_policy = AgentPolicy(final_agent, policy.name)
    final = {}
    for suite in evaluation["final_suites"]:
        for sigma in evaluation["final_noise"]:
            metrics, _ = evaluate(final_policy, suite, (sigma, 0.0), action_frame=env_config["action_frame"],
                                  obs_mode=env_config["obs_mode"])
            run.log_eval(f"final-{suite}-noise{sigma:g}", metrics)
            final[f"{suite}@{sigma:g}"] = metrics.success_rate
    run.finish(training_seconds=round(seconds, 1), steps_per_second=round(steps / seconds, 1),
               best_during_training=best, final_success=final)
    print(f"{run.dir}: {seconds:.0f}s, best on {evaluation['during_training_suite']} {best:.3f}, final {final}", flush=True)
    return run.dir


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--set", nargs="*", default=[])
    parser.add_argument("--name")
    args = parser.parse_args()
    config = apply_overrides(yaml.safe_load(Path(args.config).read_text()), args.set)
    if config["algorithm"] not in ("elite", "awr"):
        parser.error("algorithm must be elite or awr")
    train_elite(config, args.seed, name=args.name)


if __name__ == "__main__":
    main()
