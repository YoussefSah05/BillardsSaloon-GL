"""Generate the fixed single-ball evaluation suites (run once; commit the output).

    uv run python -m scripts.make_eval_suites

Seeds 1001-1003 are reserved for evaluation; training draws its layouts from
generators seeded through Gymnasium, so the chance of a training layout
coinciding with a suite layout is negligible (both are continuous draws).
"""

from __future__ import annotations

import json
import platform
import subprocess
from datetime import datetime, timezone

import billiards_sim as bs
from billiards_rl.envs.single_shot_env import ENV_VERSION
from billiards_rl.evaluation.suites import SUITE_DIR, SUITE_FORMAT, layouts_hash

rl = bs.rl
SUITES = {"easy": 1001, "medium": 1002, "hard": 1003}
COUNT = 1000


def git_commit() -> str:
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def main() -> None:
    SUITE_DIR.mkdir(parents=True, exist_ok=True)
    table = rl.table_info()
    for name, seed in SUITES.items():
        generator = rl.LayoutGenerator(seed)
        layouts = []
        for _ in range(COUNT):
            layout = generator.single_ball(name)
            p = layout["positions"]
            layouts.append({
                "cue": [round(float(p[0, 0]), 6), round(float(p[0, 1]), 6)],
                "object": [round(float(p[1, 0]), 6), round(float(p[1, 1]), 6)],
                "difficulty": layout["difficulty"],
                "pocket": int(layout["pocket"]),
                "cut": round(float(layout["ghost"]["cut"]), 3),
            })
        suite = {
            "format": SUITE_FORMAT,
            "name": name,
            "env_version": ENV_VERSION,
            "generator_seed": seed,
            "count": COUNT,
            "table": {"width": table["width"], "length": table["length"], "ball_radius": table["ball_radius"]},
            "created": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "git_commit": git_commit(),
            "platform": platform.platform(),
            "sha256": layouts_hash(layouts),
            "layouts": layouts,
        }
        path = SUITE_DIR / f"{name}.json"
        path.write_text(json.dumps(suite, indent=1) + "\n")
        print(f"{path}: {COUNT} layouts, sha256 {suite['sha256'][:12]}")


if __name__ == "__main__":
    main()
