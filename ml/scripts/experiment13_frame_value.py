"""Experiment 13 (Milestone 13): self-play of a learned frame value under WPA rules.

    uv run python -m scripts.experiment13_frame_value

9-ball, the Pro profile `iris_lindqvist` (0.32 deg aim error) in both seats.
Generation 0: the classical planner with its hand-made scoring plays itself
(20000 frames). For k = 1..3: fit V_k on all self-play positions so far
(both players' points of view, labelled with the frame winner), evaluate the
planner with V_k against the hand-made planner, then let V_k play itself for
the next generation's data. Head-to-head: 2000 racks, each played twice with
the seats (and the break) swapped; win rate with a Wilson 95% interval and a
95% bootstrap interval over racks (the pair is the unit). Seat swapping makes
a hand-made vs hand-made match exactly 50% by construction, so the protocol is
checked with a negative control instead: a constant value (0.5, so every
outcome scores the same) against the hand-made planner, on 500 racks.
"""

from __future__ import annotations

import json
import os
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

from billiards_rl.evaluation.evaluate import wilson_interval
from billiards_rl.selfplay.frame_value import NumpyValue, fit_frame_value
from billiards_rl.selfplay.frames import play_frame

OUT = Path("runs/experiment13")
PROFILE = "iris_lindqvist"
FRAMES = 20000
RACKS = 2000
GENERATIONS = 3
WORKERS = max(1, (os.cpu_count() or 2) - 1)
CHUNK = 50


def _constant(features) -> float:
    return 0.5


def _value(state):
    if state is None:
        return None
    if isinstance(state, str) and state == "constant":
        return _constant
    return NumpyValue.from_state(state)


def selfplay_chunk(args) -> tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    state, start, count, generation = args
    v = _value(state)
    xs, zs, frames, unfinished = [], [], [], 0
    for i in range(start, start + count):
        rack = 10_000_000 * (generation + 1) + i
        winner, records = play_frame(rack, i % 2, PROFILE, (v, v), record=True)
        if winner < 0:
            unfinished += 1
            continue
        for features, player in records:
            xs.append(features)
            zs.append(1.0 if player == winner else 0.0)
            frames.append(rack)
    return np.asarray(xs, dtype=np.float32), np.asarray(zs, dtype=np.float32), np.asarray(frames), unfinished


def match_chunk(args) -> list[tuple[int, int]]:
    """Per rack: (wins of A as player 0 breaking, wins of A as player 1, B breaking)."""
    state_a, state_b, start, count = args
    a, b = _value(state_a), _value(state_b)
    out = []
    for i in range(start, start + count):
        rack = 900_000_000 + i                 # the evaluation racks, never used in training
        w0, _ = play_frame(rack, 0, PROFILE, (a, b))   # A is player 0 and breaks
        w1, _ = play_frame(rack, 0, PROFILE, (b, a))   # B is player 0 and breaks
        out.append((int(w0 == 0), int(w1 == 1)))
    return out


def chunks(total: int):
    return [(s, min(CHUNK, total - s)) for s in range(0, total, CHUNK)]


def match(pool, state_a, state_b, racks: int = RACKS) -> dict:
    parts = pool.map(match_chunk, [(state_a, state_b, s, c) for s, c in chunks(racks)])
    per_rack = np.array([r for part in parts for r in part])   # (RACKS, 2)
    wins = int(per_rack.sum())
    frames = 2 * len(per_rack)
    rng = np.random.default_rng(0)
    rack_scores = per_rack.mean(1)
    boot = [rng.choice(rack_scores, len(rack_scores)).mean() for _ in range(2000)]
    return {"win_rate": wins / frames, "wilson95": list(wilson_interval(wins, frames)),
            "bootstrap95": [float(np.percentile(boot, 2.5)), float(np.percentile(boot, 97.5))],
            "win_rate_breaking": float(per_rack[:, 0].mean()), "win_rate_not_breaking": float(per_rack[:, 1].mean()),
            "frames": frames}


def selfplay(pool, state, generation) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    parts = list(pool.map(selfplay_chunk, [(state, s, c, generation) for s, c in chunks(FRAMES)]))
    unfinished = sum(p[3] for p in parts)
    x = np.concatenate([p[0] for p in parts])
    print(f"generation {generation} self-play: {FRAMES - unfinished} frames, {len(x)} records, {unfinished} unfinished", flush=True)
    return x, np.concatenate([p[1] for p in parts]), np.concatenate([p[2] for p in parts])


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    results = {"profile": PROFILE, "frames_per_generation": FRAMES, "racks": RACKS, "generations": []}
    with ProcessPoolExecutor(max_workers=WORKERS) as pool:
        results["negative_control"] = match(pool, "constant", None, racks=500)
        print("negative control (constant value vs hand-made):", json.dumps(results["negative_control"]), flush=True)
        data = [selfplay(pool, None, 0)]
        for gen in range(1, GENERATIONS + 1):
            x = np.concatenate([d[0] for d in data])
            z = np.concatenate([d[1] for d in data])
            f = np.concatenate([d[2] for d in data])
            net, fit = fit_frame_value(x, z, f, seed=gen)
            state = NumpyValue(net).state()
            np.savez(OUT / f"value-g{gen}.npz", *state["weights"], *state["biases"])
            row = {"generation": gen, "fit": fit, "vs_handmade": match(pool, state, None)}
            print(json.dumps(row), flush=True)
            results["generations"].append(row)
            (OUT / "results.json").write_text(json.dumps(results, indent=1) + "\n")
            if gen < GENERATIONS:
                data.append(selfplay(pool, state, gen))


if __name__ == "__main__":
    main()
