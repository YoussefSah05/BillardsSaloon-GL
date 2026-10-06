"""Experiment 0: the baselines on the fixed suites, with and without aiming noise.

    uv run python -m scripts.experiment0_baselines

Writes runs/experiment0/results.json and prints a Markdown table for
docs/rl/experiments.md.
"""

from __future__ import annotations

import json
from dataclasses import asdict
from pathlib import Path

from billiards_rl.baselines import make_baseline
from billiards_rl.evaluation.evaluate import evaluate

SUITES = ("easy", "medium", "hard")
NOISE = (0.0, 0.1, 0.25, 0.5)
OUT = Path("runs/experiment0")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    results = []
    for policy_name in ("random", "geometry", "sim_search"):
        for suite in SUITES:
            for sigma in (NOISE if policy_name != "random" else (0.0,)):
                metrics, _ = evaluate(make_baseline(policy_name), suite, (sigma, 0.0))
                results.append(asdict(metrics))
                print(f"{policy_name:10s} {suite:6s} sigma={sigma:4.2f}  success {metrics.success_rate:6.1%} "
                      f"[{metrics.success_ci95[0]:.3f}, {metrics.success_ci95[1]:.3f}]  scratch {metrics.scratch_rate:5.1%}  "
                      f"legal {metrics.legal_contact_rate:5.1%}  {metrics.seconds:5.1f}s", flush=True)
    (OUT / "results.json").write_text(json.dumps(results, indent=1) + "\n")

    print("\n| Policy | Aim noise σ | easy | medium | hard |\n|---|---|---|---|---|")
    for policy_name in ("random", "geometry", "sim_search"):
        for sigma in (NOISE if policy_name != "random" else (0.0,)):
            cells = []
            for suite in SUITES:
                m = next(r for r in results if r["policy"] == policy_name and r["suite"] == suite and r["noise_phi_deg"] == sigma)
                cells.append(f"{m['success_rate']:.1%} ± {(m['success_ci95'][1] - m['success_ci95'][0]) / 2:.1%}")
            print(f"| {policy_name} | {sigma:g}° | " + " | ".join(cells) + " |")


if __name__ == "__main__":
    main()
