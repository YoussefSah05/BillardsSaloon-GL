"""Experiment 10 (Milestone 11): a learned strategic policy over a fixed shot policy.

    uv run python -m scripts.experiment10_hierarchy             # suites (once), training, evaluation
    uv run python -m scripts.experiment10_hierarchy --suites    # only (re)generate the suites

Suites: `run3` (1000 racks, seed 1007) and `run3_validation` (600, seed 1008):
a cue ball and 3 object balls, legal, with at least one clear goal. For each
executor (geometry; the robust SAC shot policy of experiment 8), π_H is
trained for 5 seeds (REINFORCE, 300 iterations x 64 racks), selected on the
validation suite, and evaluated on the test suite with and without 0.25° of
aim noise, against random and easiest-first strategies on the same racks.
"""

from __future__ import annotations

import argparse
import json
import platform
from concurrent.futures import ProcessPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import torch

from billiards_rl.evaluation.suites import SUITE_DIR, SUITE_FORMAT, layouts_hash, load_suite
from billiards_rl.hierarchy.executors import GeometryExecutor, PolicyExecutor
from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.hierarchy.strategy import EasiestStrategy, LearnedStrategy, RandomStrategy, Scorer, train_strategy
from scripts.make_eval_suites import git_commit

OUT = Path("runs/experiment10")
SAC_EXECUTOR = "runs/experiment8/sac-robust-s1/checkpoints/best"
SUITES = {"run3": (1007, 1000), "run3_validation": (1008, 600)}


def make_suites() -> None:
    for name, (seed, count) in SUITES.items():
        env = RunEnv(balls=3)
        rng = np.random.default_rng(seed)
        layouts = [{"balls": env.random_layout(rng).tolist()} for _ in range(count)]
        suite = {"format": SUITE_FORMAT, "name": name, "task": "run", "balls": 3, "generator_seed": seed, "count": count,
                 "created": datetime.now(timezone.utc).isoformat(timespec="seconds"), "git_commit": git_commit(),
                 "platform": platform.platform(), "sha256": layouts_hash(layouts), "layouts": layouts}
        (SUITE_DIR / f"{name}.json").write_text(json.dumps(suite, indent=1) + "\n")
        print(f"{name}: {count} racks, sha256 {suite['sha256'][:12]}")


def executor(name: str):
    if name == "geometry":
        return GeometryExecutor()
    from billiards_rl.training.models import load_policy_model
    return PolicyExecutor(load_policy_model("sac", SAC_EXECUTOR), "sac-robust")


def play_suite(suite_name: str, strategy, executor_obj, noise: float, noise_seed: int = 4242) -> np.ndarray:
    """Balls potted (minus scratches) per rack; identical noise draws for every strategy."""
    layouts = [np.asarray(layout["balls"]) for layout in load_suite(suite_name)["layouts"]]
    env = RunEnv(balls=3, layouts=layouts, noise=(noise, 0.0))
    returns = []
    for i in range(len(layouts)):
        env.rng = np.random.default_rng(noise_seed + i)
        env.reset()
        total = 0.0
        while True:
            reward, done, _ = env.play(env.candidates[strategy.choose(env)], executor_obj)
            total += reward
            if done:
                break
        returns.append(total)
    return np.array(returns)


def train_one(args: tuple[str, int]) -> dict:
    """300 iterations of 64 racks, in 12 blocks; keeps the best on validation."""
    executor_name, seed = args
    torch.set_num_threads(1)
    ex = executor(executor_name)
    env = RunEnv(balls=3)
    scorer, optimizer = None, None
    best, best_state, history = -1e9, None, []
    for block in range(12):
        scorer, optimizer = train_strategy(env, ex, iterations=25, seed=seed * 100 + block, scorer=scorer, optimizer=optimizer)
        score = play_suite("run3_validation", LearnedStrategy(scorer), ex, 0.0).mean()
        history.append(float(score))
        if score > best:
            best, best_state = score, {k: v.clone() for k, v in scorer.state_dict().items()}
    path = OUT / f"scorer-{executor_name}-s{seed}.pt"
    torch.save(best_state, path)
    return {"executor": executor_name, "seed": seed, "validation_best": float(best), "validation_history": history, "path": str(path)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--suites", action="store_true")
    args = parser.parse_args()
    if args.suites or not (SUITE_DIR / "run3.json").exists():
        make_suites()
        if args.suites:
            return
    OUT.mkdir(parents=True, exist_ok=True)
    jobs = [(e, s) for e in ("geometry", "sac") for s in range(1, 6)]
    with ProcessPoolExecutor(max_workers=6) as pool:
        trained = list(pool.map(train_one, jobs))
    for t in trained:
        print(f"trained {t['executor']} s{t['seed']}: validation best {t['validation_best']:.3f} history {[round(h, 2) for h in t['validation_history']]}", flush=True)

    results = {"trained": trained, "evaluation": []}
    for executor_name in ("geometry", "sac"):
        ex = executor(executor_name)
        for noise in (0.0, 0.25):
            base = {"random": play_suite("run3", RandomStrategy(7), ex, noise), "easiest": play_suite("run3", EasiestStrategy(), ex, noise)}
            learned = []
            for t in (t for t in trained if t["executor"] == executor_name):
                scorer = Scorer()
                scorer.load_state_dict(torch.load(t["path"]))
                learned.append(play_suite("run3", LearnedStrategy(scorer), ex, noise))
            learned = np.array(learned)
            diff = learned - base["easiest"][None, :]
            row = {
                "executor": executor_name, "noise": noise,
                "random_balls": float(base["random"].mean()), "easiest_balls": float(base["easiest"].mean()),
                "easiest_cleared": float((base["easiest"] >= 3).mean()),
                "learned_balls_mean": float(learned.mean()), "learned_balls_sd_seeds": float(learned.mean(1).std(ddof=1)),
                "learned_cleared_mean": float((learned >= 3).mean()),
                "diff_vs_easiest_per_seed": diff.mean(1).tolist(),
                # paired per-rack differences: seeds whose 95% bootstrap CI of the mean difference excludes 0
                "seeds_better": int(sum(np.percentile(bootstrap(d), 2.5) > 0 for d in diff)),
                "seeds_worse": int(sum(np.percentile(bootstrap(d), 97.5) < 0 for d in diff)),
            }
            results["evaluation"].append(row)
            print(json.dumps(row), flush=True)
    (OUT / "results.json").write_text(json.dumps(results, indent=1) + "\n")


def bootstrap(d: np.ndarray, n: int = 2000, seed: int = 0) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return np.array([rng.choice(d, len(d)).mean() for _ in range(n)])


if __name__ == "__main__":
    main()
