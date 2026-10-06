"""Generate the fixed position-play suites (Milestone 10; run once, commit the output).

    uv run python -m scripts.make_position_suites

`position` (1000 layouts, seed 1005) is the test suite; `position_validation`
(600, seed 1006) selects checkpoints. Bands cycle easy, medium, hard. Each
layout carries a feasible cue-ball target (PositionShotEnv.feasible_target):
where the cue ball stopped after potting the object ball into the target
pocket at a random speed and spin.
"""

from __future__ import annotations

import json
import platform
from datetime import datetime, timezone

import numpy as np

import billiards_sim as bs
from billiards_rl.envs.position_shot_env import PositionShotEnv
from billiards_rl.envs.single_shot_env import ENV_VERSION
from billiards_rl.evaluation.suites import SUITE_DIR, SUITE_FORMAT, layouts_hash
from scripts.make_eval_suites import git_commit

rl = bs.rl
SUITES = {"position": (1005, 1000), "position_validation": (1006, 600)}


def main() -> None:
    table = rl.table_info()
    for name, (seed, count) in SUITES.items():
        generator = rl.LayoutGenerator(seed)
        rng = np.random.default_rng(seed)
        bands = ["easy", "medium", "hard"]
        layouts = []
        while len(layouts) < count:
            layout = generator.single_ball(bands[len(layouts) % 3])
            p = np.round(layout["positions"], 6)
            target = PositionShotEnv.feasible_target(p, int(layout["pocket"]), rng)
            if target is None:
                continue
            layouts.append({
                "cue": [float(p[0, 0]), float(p[0, 1])],
                "object": [float(p[1, 0]), float(p[1, 1])],
                "difficulty": layout["difficulty"],
                "pocket": int(layout["pocket"]),
                "cut": round(float(layout["ghost"]["cut"]), 3),
                "cue_target": [round(float(target[0]), 6), round(float(target[1]), 6)],
            })
        suite = {
            "format": SUITE_FORMAT, "name": name, "task": "position", "env_version": ENV_VERSION,
            "generator_seed": seed, "count": count,
            "table": {"width": table["width"], "length": table["length"], "ball_radius": table["ball_radius"]},
            "created": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "git_commit": git_commit(), "platform": platform.platform(),
            "sha256": layouts_hash(layouts), "layouts": layouts,
        }
        path = SUITE_DIR / f"{name}.json"
        path.write_text(json.dumps(suite, indent=1) + "\n")
        print(f"{path}: {count} layouts, sha256 {suite['sha256'][:12]}")


if __name__ == "__main__":
    main()
