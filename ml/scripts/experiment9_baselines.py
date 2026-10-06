"""Experiment 9 baselines: geometry and position search on the position suite.

    uv run python -m scripts.experiment9_baselines
"""

import json
from dataclasses import asdict
from pathlib import Path

from billiards_rl.baselines import GeometryPolicy, PositionSearchPolicy
from billiards_rl.evaluation.evaluate import evaluate

CONFIG = {"task": "position", "action_frame": "ghost_relative"}


def main() -> None:
    out = Path("runs/experiment9_baselines")
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for policy in (GeometryPolicy(), PositionSearchPolicy()):
        for sigma in (0.0, 0.25):
            m, _ = evaluate(policy, "position", (sigma, 0.0), env_config=CONFIG)
            results.append(asdict(m))
            print(f"{m.policy:16s} sigma={sigma:4.2f}  pot {m.target_success_rate:6.1%}  position {m.position_success_rate:6.1%}  "
                  f"median cue distance {m.median_cue_distance:.3f} m  scratch {m.scratch_rate:5.1%}  mean reward {m.mean_reward:.3f}", flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=1) + "\n")


if __name__ == "__main__":
    main()
