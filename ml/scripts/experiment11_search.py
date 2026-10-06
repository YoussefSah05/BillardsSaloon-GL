"""Experiment 11 (Milestone 12): candidate shots plus sampled expectimax.

    uv run python -m scripts.experiment11_search

On the run3 suite (1000 racks), with exact execution and with 0.25 deg of aim
noise; the search assumes the player's true noise (0: one deterministic
sample). Proposers: the robust SAC shot policy (experiment 8) or geometry.
Paired per rack with easiest-first + the same proposer as executor
(identical noise draws), 95% bootstrap intervals of the mean difference.
"""

from __future__ import annotations

import json
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

from billiards_rl.evaluation.suites import load_suite
from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.hierarchy.strategy import EasiestStrategy
from billiards_rl.search.expectimax import ExpectimaxPlayer
from scripts.experiment10_hierarchy import bootstrap, executor, play_suite

OUT = Path("runs/experiment11")


def search_suite(args: tuple[str, int, float]) -> tuple[tuple[str, int, float], list[float]]:
    proposer_name, depth, noise = args
    ex = executor(proposer_name)
    samples = 16 if noise > 0 else 1
    player = ExpectimaxPlayer(ex, depth=depth, samples=samples, leaf_samples=4 if noise > 0 else 1, sigma=noise, seed=99, threads=1)
    layouts = [np.asarray(layout["balls"]) for layout in load_suite("run3")["layouts"]]
    env = RunEnv(balls=3, layouts=layouts, noise=(noise, 0.0))
    returns = []
    for i in range(len(layouts)):
        env.rng = np.random.default_rng(4242 + i)   # the same noise draws as play_suite
        env.reset()
        returns.append(player.run_rack(env))
    return args, returns


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    jobs = [(p, d, n) for p in ("sac", "geometry") for d in (1, 2) for n in (0.0, 0.25)]
    with ProcessPoolExecutor(max_workers=8) as pool:
        searched = dict(pool.map(search_suite, jobs))
    rows = []
    for proposer in ("sac", "geometry"):
        for noise in (0.0, 0.25):
            base = play_suite("run3", EasiestStrategy(), executor(proposer), noise)
            for depth in (1, 2):
                r = np.array(searched[(proposer, depth, noise)])
                d = r - base
                boot = bootstrap(d)
                row = {"proposer": proposer, "depth": depth, "noise": noise, "search_balls": float(r.mean()),
                       "search_cleared": float((r >= 3).mean()), "easiest_balls": float(base.mean()),
                       "easiest_cleared": float((base >= 3).mean()), "difference": float(d.mean()),
                       "ci95": [float(np.percentile(boot, 2.5)), float(np.percentile(boot, 97.5))]}
                rows.append(row)
                print(json.dumps(row), flush=True)
    (OUT / "results.json").write_text(json.dumps(rows, indent=1) + "\n")


if __name__ == "__main__":
    main()
