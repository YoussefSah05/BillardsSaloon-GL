"""Fresh-match evaluation and local release gate for the reliable ensemble.

python -m billiards_ml.reliable_validate predictions --frames 900
python -m billiards_ml.reliable_validate matches --racks 50
python -m billiards_ml.reliable_validate adapt --racks 8
python -m billiards_ml.reliable_validate install

No weights, blend or calibration may be tuned on these release matches.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import multiprocessing as mp
from pathlib import Path
import shutil
import time

import numpy as np

from . import adaptive_train as league
from .reliable_train import OUTPUT, measure

PREVIOUS = Path("../assets/models/adaptive_value.onnx")
WEIGHT = .25


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def predictions(args):
    model, previous = OUTPUT / "value.onnx", PREVIOUS
    # Deliberately distinct from league seeds (42), existing match tests
    # (800M/810M), and adaptation (950M). Classical play supplies observations
    # so neither evaluated neural model chooses which positions it is tested on.
    tasks = [(start, min(25, args.frames - start), 0, None, 0., args.seed)
             for start in range(0, args.frames, 25)]
    parts, start = [], time.perf_counter()
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for part in pool.imap_unordered(league.generate_chunk, tasks):
            parts.append(part)
            print(f"fresh holdout: {sum(p['completed'] for p in parts)}/{args.frames} matches, "
                  f"{time.perf_counter()-start:.1f}s", flush=True)
    if sum(part["completed"] for part in parts) != args.frames:
        raise RuntimeError("Fresh holdout had incomplete matches")
    data = {k:np.concatenate([p[k] for p in parts]) for k in ("x", "y", "frame", "strata")}
    order = np.argsort(data["frame"], kind="stable")
    data = {k:v[order] for k,v in data.items()}
    np.savez_compressed(OUTPUT / "fresh-holdout.npz", **data)
    new_runtime, old_runtime = league.OnnxValue(model), league.OnnxValue(previous)
    p, old_p, std = [], [], []
    for batch in np.array_split(data["x"], max(1, len(data["x"])//1024)):
        outputs = new_runtime.session.run(None, {"features":batch})
        p.append(outputs[0]); std.append(outputs[1])
        old_p.append(old_runtime.session.run(["win_probability"], {"features":batch})[0])
    p, old_p, std = np.concatenate(p), np.concatenate(old_p), np.concatenate(std)
    groups = {}
    frame_delta = []
    for frame in np.unique(data["frame"]):
        selected = data["frame"] == frame
        frame_delta.append(float((((p[selected] > .5) == data["y"][selected]).astype(float) -
                                  ((old_p[selected] > .5) == data["y"][selected]).astype(float)).mean()))
    rng = np.random.default_rng(911)
    ci = np.quantile(rng.choice(frame_delta, (5000, len(frame_delta)), replace=True).mean(1), [.025, .975])
    for d, name in enumerate(league.DISCIPLINES):
        selected = data["strata"][:, 0] == d
        groups[name] = dict(new=measure(p[selected], data["y"][selected]),
                            previous=measure(old_p[selected], data["y"][selected]))
    report = dict(model_sha256=digest(model), previous_model_sha256=digest(previous), frames=args.frames,
                  frames_with_positions=len(np.unique(data["frame"])), positions=len(p), seed=args.seed,
                  new=measure(p, data["y"]), previous=measure(old_p, data["y"]), by_discipline=groups,
                  frame_mean_accuracy_change=float(np.mean(frame_delta)), accuracy_frame_bootstrap_95=ci.tolist(),
                  disagreement_mean=float(std.mean()), dataset_sha256=digest(OUTPUT / "fresh-holdout.npz"),
                  interpretation="Frame-winner prediction accuracy, not pot success or match win rate.")
    (OUTPUT / "predictions.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


def matches(args):
    league.ROOT = OUTPUT
    # Compare against both physics search and the current bundled neural model
    # on the same previously unused seeds, with the blend fixed before testing.
    for model, weight, name in ((PREVIOUS, WEIGHT, "previous"),
                                 (OUTPUT / "value.onnx", WEIGHT, "new"),
                                 (OUTPUT / "value.onnx", 0., "classical")):
        evaluation = argparse.Namespace(model=str(model), weight=weight, phase=args.phase,
                                         racks=args.racks, workers=args.workers)
        league.evaluate(evaluation)
        shutil.copy2(OUTPUT / f"evaluation-phase{args.phase}-w{weight:g}.json", OUTPUT / f"matches-{name}.json")


def adapt(args):
    league.ROOT = OUTPUT
    league.adapt(argparse.Namespace(model=str(OUTPUT / "value.onnx"), racks=args.racks,
                                   workers=args.workers, weight=WEIGHT))


def install(args):
    training = json.loads((OUTPUT / "training.json").read_text())
    fresh = json.loads((OUTPUT / "predictions.json").read_text())
    # Fail prediction checks before expensive match tests or any asset writes.
    checks = dict(fresh_accuracy=fresh["new"]["accuracy"] >= fresh["previous"]["accuracy"],
                  fresh_log_loss=fresh["new"]["log_loss"] < fresh["previous"]["log_loss"],
                  fresh_brier=fresh["new"]["brier"] < fresh["previous"]["brier"],
                  calibration=fresh["new"]["ece_15"] <= fresh["previous"]["ece_15"],
                  held_out_size=fresh["frames"] >= 900,
                  per_game_loss=max(v["new"]["log_loss"] - v["previous"]["log_loss"]
                                    for v in fresh["by_discipline"].values()) <= .01,
                  cpu_budget=training["inference_ms"] < 1., onnx_parity=training["onnx_max_difference"] < 1e-5,
                  opponent_conditioning=training["weaker_opponent_value_delta"] > .02,
                  artifact_identity=digest(OUTPUT / "value.onnx") == training["model_sha256"] == fresh["model_sha256"],
                  previous_identity=digest(PREVIOUS) == fresh["previous_model_sha256"] == training["previous_model_sha256"])
    gate = dict(checks=checks, installed=False,
                interpretation="Engineering regression limits do not guarantee accuracy or prove a strength gain.")
    (OUTPUT / "gate.json").write_text(json.dumps(gate, indent=2) + "\n")
    if not all(checks.values()):
        raise RuntimeError(f"Prediction release checks failed; shipped model preserved: {checks}")
    results = {name:json.loads((OUTPUT / f"matches-{name}.json").read_text()) for name in ("new", "previous", "classical")}
    change = {name:league.comparison(results["new"], results[name]) for name in ("previous", "classical")}
    adaptation = json.loads((OUTPUT / "adaptation.json").read_text())
    calibration_path = Path("../assets/data/ai/skill_calibration.json")
    checks.update(match_size=results["new"]["racks_per_matchup"] >= 50,
                  match_identity=results["new"]["model_sha256"] == training["model_sha256"] == adaptation["model_sha256"],
                  match_previous_identity=results["previous"]["model_sha256"] == fresh["previous_model_sha256"],
                  adaptive_identity=adaptation["weight"] == WEIGHT and adaptation["calibration_sha256"] == digest(calibration_path)
                      and adaptation["native_estimator_sha256"] == digest("../src/ai/opponent_model.cpp"),
                  adaptive_completion=not any(v["unfinished"] for v in adaptation["matchups"].values()),
                  adaptive_response=all(adaptation["matchups"][f"{game}/viktor_hale"]["curve"][-1]["skill"] >
                                        adaptation["matchups"][f"{game}/sam_whitlock"]["curve"][-1]["skill"]+.15
                                        for game in league.DISCIPLINES),
                  match_point_estimates=min(c["paired_win_rate_change"] for c in change.values()) >= -.01,
                  match_intervals=min(c["paired_bootstrap_95"][0] for c in change.values()) > -.06,
                  match_per_game=min(min(c["by_discipline"].values()) for c in change.values()) >= -.04)
    gate["comparisons"] = change
    (OUTPUT / "gate.json").write_text(json.dumps(gate, indent=2) + "\n")
    if not all(checks.values()):
        raise RuntimeError(f"Match release checks failed; shipped model preserved: {checks}")
    destination = Path("../assets/models")
    # Archive the prior release locally so an evaluated upgrade is reversible.
    shutil.copy2(PREVIOUS, OUTPUT / "previous-release.onnx")
    shutil.copy2(destination / "adaptive_ai.json", OUTPUT / "previous-release.json")
    shutil.copy2(OUTPUT / "value.onnx", PREVIOUS)
    gate["installed"] = True
    card = dict(name=training["name"], version=2, training=training, release=gate, learned_weight=WEIGHT,
                predictions=fresh, evaluation=results, adaptation=adaptation,
                skill_calibration=json.loads(calibration_path.read_text()))
    (destination / "adaptive_ai.json").write_text(json.dumps(card, indent=2) + "\n")
    (OUTPUT / "gate.json").write_text(json.dumps(gate, indent=2) + "\n")
    print(json.dumps(gate, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("predictions", "matches", "adapt", "install"))
    parser.add_argument("--frames", type=int, default=900)
    parser.add_argument("--racks", type=int, default=50)
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--seed", type=int, default=77, help="fresh prediction seed domain; change for later experiments")
    parser.add_argument("--phase", type=int, default=3, help="fresh paired-match phase; change for later experiments")
    args = parser.parse_args()
    if min(args.frames, args.racks, args.workers) < 1:
        parser.error("counts must be positive")
    if args.seed <= 0 or args.seed == 42 or args.frames >= 1_000_000 or args.phase < 2:
        parser.error("use a positive fresh seed other than training seed 42, fewer than one million frames, and phase >= 2")
    globals()[args.command](args)


if __name__ == "__main__":
    main()
