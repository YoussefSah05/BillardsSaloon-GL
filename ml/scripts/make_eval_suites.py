"""Generate the fixed single-ball evaluation suites (run once; commit the output).

    uv run python -m scripts.make_eval_suites                 # all suites
    uv run python -m scripts.make_eval_suites validation      # only the named ones

Seeds 1001-1004 are reserved for evaluation. The validation suite (200 layouts
of each band) is for choosing checkpoints during training, so the test suites
are never used for selection; training draws its layouts from
generators seeded through Gymnasium, so the chance of a training layout
coinciding with a suite layout is negligible (both are continuous draws).
"""

from __future__ import annotations

import json
import platform
import sys
import subprocess
from datetime import datetime, timezone

import billiards_sim as bs
from billiards_rl.envs.single_shot_env import ENV_VERSION
from billiards_rl.evaluation.suites import SUITE_DIR, SUITE_FORMAT, layouts_hash

rl = bs.rl
SUITES = {"easy": 1001, "medium": 1002, "hard": 1003, "validation": 1004}
COUNT = 1000
VALIDATION_PER_BAND = 200


def git_commit() -> str:
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def main() -> None:
    SUITE_DIR.mkdir(parents=True, exist_ok=True)
    table = rl.table_info()
    only = set(sys.argv[1:])
    for name, seed in SUITES.items():
        if only and name not in only:
            continue
        generator = rl.LayoutGenerator(seed)
        bands = ["easy", "medium", "hard"] * VALIDATION_PER_BAND if name == "validation" else [name] * COUNT
        layouts = []
        for band in bands:
            layout = generator.single_ball(band)
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
            "count": len(layouts),
            "table": {"width": table["width"], "length": table["length"], "ball_radius": table["ball_radius"]},
            "created": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "git_commit": git_commit(),
            "platform": platform.platform(),
            "sha256": layouts_hash(layouts),
            "layouts": layouts,
        }
        path = SUITE_DIR / f"{name}.json"
        path.write_text(json.dumps(suite, indent=1) + "\n")
        print(f"{path}: {len(layouts)} layouts, sha256 {suite['sha256'][:12]}")


if __name__ == "__main__":
    main()
