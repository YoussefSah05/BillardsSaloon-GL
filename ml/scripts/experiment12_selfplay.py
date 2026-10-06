"""Experiment 12 (Milestone 13): expert iteration of a learned rack value.

    uv run python -m scripts.experiment12_selfplay

Geometry proposer, depth-2 search, 0.25 deg of aim noise everywhere.
Generation 0: the search with the simulated next-shot leaf (experiment 11)
plays 8000 random racks. Then for k = 1..4 and 3 seeds: fit V_k on the racks
of generation k-1 (on-policy Monte Carlo returns), let the search with leaf
V_k play 8000 new racks. Every V_k player is evaluated on the run3 suite
(1000 racks, the evaluation noise draws of experiments 10-11), paired per rack
with the generation-0 player; 95% bootstrap intervals. The headline is the
last generation, fixed in advance.
"""

from __future__ import annotations

import json
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
import torch

from billiards_rl.evaluation.suites import load_suite
from billiards_rl.hierarchy.executors import GeometryExecutor
from billiards_rl.hierarchy.run_env import RunEnv
from billiards_rl.search.expectimax import ExpectimaxPlayer
from billiards_rl.selfplay.expert_iteration import play_racks
from billiards_rl.selfplay.value_net import LearnedValue, StateEncoder, ValueNet, fit_value
from scripts.experiment10_hierarchy import bootstrap

OUT = Path("runs/experiment12")
NOISE = 0.25
RACKS = 8000
CHUNKS = 8
GENERATIONS = 4
SEEDS = (1, 2, 3)
WORKERS = 10


def player(value_state: dict | None, seed: int) -> ExpectimaxPlayer:
    torch.set_num_threads(1)
    value_fn = None
    if value_state is not None:
        net = ValueNet()
        net.load_state_dict(value_state)
        value_fn = LearnedValue(net)
    return ExpectimaxPlayer(GeometryExecutor(), depth=2, samples=16, leaf_samples=4, sigma=NOISE, seed=seed,
                            threads=1, value_fn=value_fn)


def generate_chunk(args: tuple[dict | None, int]) -> tuple[list[np.ndarray], np.ndarray, np.ndarray]:
    value_state, seed = args
    return play_racks(player(value_state, seed), RACKS // CHUNKS, NOISE, seed)


def evaluate_chunk(args: tuple[dict | None, int, int]) -> list[float]:
    value_state, start, end = args
    p = player(value_state, 0)
    layouts = [np.asarray(layout["balls"]) for layout in load_suite("run3")["layouts"]]
    env = RunEnv(balls=3, layouts=layouts[start:end], noise=(NOISE, 0.0))
    returns = []
    for i in range(start, end):
        env.rng = np.random.default_rng(4242 + i)   # the evaluation draws of experiments 10-11
        p.rng = np.random.default_rng(99 + i)        # the search's own samples, per rack
        env.reset()
        returns.append(p.run_rack(env))
    return returns


def generate(pool, value_state, seed) -> tuple[list[np.ndarray], np.ndarray, np.ndarray]:
    parts = list(pool.map(generate_chunk, [(value_state, seed * 1000 + c) for c in range(CHUNKS)]))
    states = [s for part in parts for s in part[0]]
    return states, np.concatenate([p[1] for p in parts]), np.concatenate([p[2] for p in parts])


def evaluate(pool, value_state) -> np.ndarray:
    bounds = np.linspace(0, 1000, 21).astype(int)
    parts = pool.map(evaluate_chunk, [(value_state, int(a), int(b)) for a, b in zip(bounds[:-1], bounds[1:])])
    return np.array([r for part in parts for r in part])


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    encoder = StateEncoder()
    rows = []
    with ProcessPoolExecutor(max_workers=WORKERS) as pool:
        base = evaluate(pool, None)
        print(f"generation 0 (simulated leaf): run3 {base.mean():.3f} balls, cleared {(base >= 3).mean():.3f}", flush=True)
        data0 = generate(pool, None, 0)
        print(f"generation 0 data: {len(data0[0])} states, rack mean {data0[2].mean():.3f}", flush=True)
        for seed in SEEDS:
            data = data0
            for gen in range(1, GENERATIONS + 1):
                torch.manual_seed(seed * 100 + gen)
                net = ValueNet()
                fit = fit_value(net, *encoder.encode(data[0]), data[1], seed=seed * 100 + gen)
                state = {k: v.clone() for k, v in net.state_dict().items()}
                torch.save(state, OUT / f"value-s{seed}-g{gen}.pt")
                test = evaluate(pool, state)
                d = test - base
                boot = bootstrap(d)
                data = generate(pool, state, seed * 100 + gen)
                row = {"seed": seed, "generation": gen, "train_states": int(fit["train_states"]),
                       "value_val_mse": fit["val_mse"], "target_variance": fit["val_target_variance"],
                       "value_r2": 1 - fit["val_mse"] / fit["val_target_variance"],
                       "run3_balls": float(test.mean()), "run3_cleared": float((test >= 3).mean()),
                       "difference_vs_gen0": float(d.mean()),
                       "ci95": [float(np.percentile(boot, 2.5)), float(np.percentile(boot, 97.5))],
                       "selfplay_rack_mean": float(data[2].mean())}
                rows.append(row)
                print(json.dumps(row), flush=True)
    (OUT / "results.json").write_text(json.dumps({"generation0": {"run3_balls": float(base.mean()),
                                                                   "run3_cleared": float((base >= 3).mean()),
                                                                   "selfplay_rack_mean": float(data0[2].mean())},
                                                   "generations": rows}, indent=1) + "\n")


if __name__ == "__main__":
    main()
