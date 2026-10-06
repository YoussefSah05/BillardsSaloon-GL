"""Summarise a group of runs: final success per suite and noise, mean ± sd over seeds.

    uv run python -m scripts.summarise_runs runs/experiment1

Groups runs by their name without the seed suffix (`-s<n>`), prints a
Markdown table of final success (mean ± sd over seeds) and each group's mean
learning curve on the during-training evaluation subset.
"""

from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np
import yaml


def main() -> None:
    root = Path(sys.argv[1])
    groups: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    curves: dict[str, list[list[tuple[int, float]]]] = defaultdict(list)
    times: dict[str, list[float]] = defaultdict(list)
    curve_suite: dict[str, str] = {}
    stages: dict[str, list] = defaultdict(list)
    for run in sorted(p for p in root.iterdir() if (p / "meta.json").exists()):
        group = re.sub(r"-s\d+$", "", run.name)
        meta = json.loads((run / "meta.json").read_text())
        if "final_success" not in meta:
            continue
        for key, value in meta["final_success"].items():
            groups[group][key].append(value)
        times[group].append(meta.get("training_seconds", float("nan")))
        if "curriculum_history" in meta:
            stages[group].append((meta["curriculum_final_stage"], meta["curriculum_history"]))
        config = yaml.safe_load((run / "config.yaml").read_text())
        curve_suite[group] = config["eval"]["during_training_suite"]
        curve = []
        for f in sorted((run / "eval").glob("step*.json")):
            m = json.loads(f.read_text())
            curve.append((int(f.name[4:12]), m["success_rate"]))
        curves[group].append(curve)

    keys = sorted({k for g in groups.values() for k in g}, key=lambda k: (k.split("@")[1], ["easy", "medium", "hard"].index(k.split("@")[0])))
    print("| Run group | seeds | " + " | ".join(keys) + " |")
    print("|---|---|" + "---|" * len(keys))
    for group, results in groups.items():
        seeds = len(next(iter(results.values())))
        cells = [f"{100 * np.mean(results[k]):.1f}% ± {100 * np.std(results[k], ddof=1 if seeds > 1 else 0):.1f}" for k in keys]
        print(f"| {group} | {seeds} | " + " | ".join(cells) + " |")
    print()
    for group, runs in curves.items():
        steps = [s for s, _ in runs[0]]
        means = [np.mean([r[i][1] for r in runs if len(r) > i]) for i in range(len(steps))]
        print(f"{group} learning curve ({curve_suite[group]}, during training): " +
              ", ".join(f"{s // 1000}k {100 * m:.0f}%" for s, m in zip(steps, means)))
        print(f"  training time per run: {np.mean(times[group]):.0f} s")
        for final, history in stages.get(group, []):
            print(f"  curriculum: final stage {final}; advances at episodes " +
                  ", ".join(f"{h[0]} -> stage {h[1]}" + (f" ({h[2]})" if len(h) > 2 else "") for h in history[1:]) if len(history) > 1
                  else f"  curriculum: stayed at stage {final}")


if __name__ == "__main__":
    main()
