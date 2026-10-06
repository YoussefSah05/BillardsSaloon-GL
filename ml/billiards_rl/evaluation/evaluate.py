"""Evaluate a policy on a fixed suite (rl-system-design.md, Section 9).

    uv run python -m billiards_rl.evaluation.evaluate --policy geometry --suite medium
    uv run python -m billiards_rl.evaluation.evaluate --policy sim_search --suite hard --noise 0.25
    uv run python -m billiards_rl.evaluation.evaluate --checkpoint runs/<id>/checkpoints/best.zip --suite easy

Each layout is played once, the policy acting deterministically. With noise,
the executed shot is perturbed by a seeded generator, identical for every
policy (the same noise draw for the same layout index).
"""

from __future__ import annotations

import argparse
import json
import math
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from billiards_rl.envs.single_shot_env import ENV_VERSION, SingleShotEnv
from billiards_rl.evaluation.suites import load_suite, suite_positions


@dataclass
class Metrics:
    suite: str
    policy: str
    noise_phi_deg: float
    noise_speed: float
    episodes: int
    success_rate: float
    success_ci95: tuple[float, float]
    scratch_rate: float
    legal_contact_rate: float
    mean_distance_after_miss: float
    mean_reward: float
    seconds: float
    env_version: str = ENV_VERSION


def wilson_interval(successes: int, n: int, z: float = 1.96) -> tuple[float, float]:
    """95% Wilson score interval for a binomial proportion."""
    if n == 0:
        return (0.0, 0.0)
    p = successes / n
    denominator = 1.0 + z * z / n
    centre = (p + z * z / (2 * n)) / denominator
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denominator
    return (max(0.0, centre - half), min(1.0, centre + half))


def evaluate(policy, suite_name: str, noise: tuple[float, float] = (0.0, 0.0), *,
             action_frame: str = "absolute", obs_mode: str = "positions", noise_seed: int = 12345,
             limit: int | None = None) -> tuple[Metrics, list[dict]]:
    suite = load_suite(suite_name)
    layouts = suite_positions(suite)[:limit]
    env = SingleShotEnv(layouts=layouts, noise=noise, action_frame=action_frame, obs_mode=obs_mode)
    obs, _ = env.reset(seed=noise_seed)
    episodes, start = [], time.perf_counter()
    for index in range(len(layouts)):
        if index > 0:
            obs, _ = env.reset()
        action = policy.act(env, obs)
        _, reward, _, _, info = env.step(action)
        episodes.append({"index": index, "reward": reward, **{k: info[k] for k in
                         ("success", "potted", "pocket", "scratch", "legal_contact", "object_to_pocket_after", "phi", "speed")}})
    seconds = time.perf_counter() - start

    n = len(episodes)
    successes = sum(e["success"] for e in episodes)
    misses = [e["object_to_pocket_after"] for e in episodes if not e["potted"]]
    metrics = Metrics(
        suite=suite_name,
        policy=getattr(policy, "name", type(policy).__name__),
        noise_phi_deg=noise[0],
        noise_speed=noise[1],
        episodes=n,
        success_rate=successes / n,
        success_ci95=wilson_interval(successes, n),
        scratch_rate=sum(e["scratch"] for e in episodes) / n,
        legal_contact_rate=sum(e["legal_contact"] for e in episodes) / n,
        mean_distance_after_miss=float(np.mean(misses)) if misses else 0.0,
        mean_reward=float(np.mean([e["reward"] for e in episodes])),
        seconds=seconds,
    )
    return metrics, episodes


class ModelPolicy:
    """A trained Stable-Baselines3 model as a policy (deterministic actions)."""

    privileged = False

    def __init__(self, path: str):
        from stable_baselines3 import PPO, SAC, TD3
        for cls in (SAC, TD3, PPO):
            try:
                self.model = cls.load(path)
                self.name = f"{cls.__name__.lower()}:{Path(path).parent.parent.name}"
                return
            except Exception:   # noqa: BLE001 - try the next algorithm
                continue
        raise ValueError(f"{path} is not a SAC, TD3 or PPO checkpoint")

    def act(self, env, obs):
        action, _ = self.model.predict(obs, deterministic=True)
        return action


def main() -> None:
    from billiards_rl.baselines import make_baseline

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--policy", choices=["random", "geometry", "sim_search"])
    parser.add_argument("--checkpoint")
    parser.add_argument("--suite", required=True, choices=["easy", "medium", "hard"])
    parser.add_argument("--noise", type=float, default=0.0, help="aim error sd, degrees")
    parser.add_argument("--speed-noise", type=float, default=0.0, help="relative speed error sd")
    parser.add_argument("--action-frame", default="absolute", choices=["absolute", "ghost_relative"])
    parser.add_argument("--obs-mode", default="positions", choices=["positions", "positions+geometry"])
    parser.add_argument("--limit", type=int)
    parser.add_argument("--out", help="write metrics JSON here")
    args = parser.parse_args()

    if bool(args.policy) == bool(args.checkpoint):
        parser.error("give exactly one of --policy or --checkpoint")
    policy = make_baseline(args.policy) if args.policy else ModelPolicy(args.checkpoint)
    metrics, _ = evaluate(policy, args.suite, (args.noise, args.speed_noise), action_frame=args.action_frame,
                          obs_mode=args.obs_mode, limit=args.limit)
    print(json.dumps(asdict(metrics), indent=1))
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(json.dumps(asdict(metrics), indent=1) + "\n")


if __name__ == "__main__":
    main()
