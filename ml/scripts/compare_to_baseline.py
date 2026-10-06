"""Paired comparison of every run's best checkpoint with a baseline.

    uv run python -m scripts.compare_to_baseline runs/experiment2 [--baseline geometry] [--noise 0]

For each run and test suite, both policies play the same layouts with the
same noise draws. The exact McNemar test (a two-sided binomial test on the
discordant layouts) asks whether they differ. Prints, per run group and suite,
the mean success difference and how many seeds are significantly better or
worse at p < 0.05.
"""

from __future__ import annotations

import argparse
import re
from collections import defaultdict
from math import comb
from pathlib import Path

import numpy as np
import yaml

from billiards_rl.baselines import make_baseline
from billiards_rl.evaluation.evaluate import evaluate
from billiards_rl.training.models import load_policy_model

SUITES = ("easy", "medium", "hard")


def mcnemar_exact(b: int, c: int) -> float:
    """Two-sided exact McNemar p-value from the discordant counts b and c."""
    n = b + c
    if n == 0:
        return 1.0
    k = min(b, c)
    tail = sum(comb(n, i) for i in range(k + 1)) / 2**n
    return min(1.0, 2 * tail)


class Checkpoint:
    def __init__(self, model, name):
        self.model, self.name = model, name

    def act(self, env, obs):
        return self.model.predict(obs, deterministic=True)[0]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--baseline", default="geometry")
    parser.add_argument("--noise", type=float, default=0.0)
    args = parser.parse_args()

    baseline_cache: dict[tuple[str, str, str], np.ndarray] = {}
    rows: dict[tuple[str, str], list[tuple[float, float, int, int]]] = defaultdict(list)
    for run in sorted(p for p in args.root.iterdir() if (p / "checkpoints" / "best.zip").exists()):
        config = yaml.safe_load((run / "config.yaml").read_text())
        frame, obs = config["env"]["action_frame"], config["env"]["obs_mode"]
        model = load_policy_model(config["algorithm"], run / "checkpoints" / "best")
        group = re.sub(r"-s\d+$", "", run.name)
        for suite in SUITES:
            key = (suite, frame, obs)
            if key not in baseline_cache:
                _, episodes = evaluate(make_baseline(args.baseline), suite, (args.noise, 0.0), action_frame="absolute", obs_mode=obs)
                baseline_cache[key] = np.array([e["success"] for e in episodes])
            base = baseline_cache[key]
            _, episodes = evaluate(Checkpoint(model, run.name), suite, (args.noise, 0.0), action_frame=frame, obs_mode=obs,
                                   goal_conditioned=config["env"].get("goal_conditioned", False),
                                   require_contact=config["env"].get("require_contact", False))
            mine = np.array([e["success"] for e in episodes])
            b, c = int(np.sum(mine & ~base)), int(np.sum(~mine & base))
            rows[(group, suite)].append((mine.mean(), base.mean(), b, c))

    print(f"Paired with {args.baseline} (aim noise {args.noise:g}°); exact McNemar, p < 0.05\n")
    print("| Run group | Suite | Success (mean over seeds) | Baseline | Difference | Seeds better | Seeds worse |")
    print("|---|---|---|---|---|---|---|")
    for (group, suite), seeds in rows.items():
        mine = np.mean([s[0] for s in seeds])
        base = seeds[0][1]
        better = sum(1 for s in seeds if s[2] > s[3] and mcnemar_exact(s[2], s[3]) < 0.05)
        worse = sum(1 for s in seeds if s[3] > s[2] and mcnemar_exact(s[2], s[3]) < 0.05)
        print(f"| {group} | {suite} | {100 * mine:.1f}% | {100 * base:.1f}% | {100 * (mine - base):+.1f} | {better}/{len(seeds)} | {worse}/{len(seeds)} |")


if __name__ == "__main__":
    main()
