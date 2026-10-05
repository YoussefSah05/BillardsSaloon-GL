"""Self-play data for the value network.

The classical AI plays itself (a random pair of the five players for each
frame, so the data covers strong and weak play). Before every shot we record
the table from the shooter's point of view; when the frame ends, each record is
labelled 1 if that player won it. Frames are split by worker and seed, so the
same frame never lands in both training and validation.

    uv run python -m billiards_ml.selfplay --frames 6000 --out data/selfplay.npz
"""

from __future__ import annotations

import argparse
import multiprocessing as mp
import os
import time

import numpy as np

DISCIPLINES = ("EIGHT_BALL", "NINE_BALL", "TEN_BALL")


def play_frames(task: tuple[int, int, int]) -> dict[str, np.ndarray]:
    worker, frames, seed_base = task
    import billiards_sim as bs  # imported in the worker process

    profiles = bs.profiles()
    rng = np.random.default_rng(seed_base + worker)
    features, labels, disciplines, frame_ids, players = [], [], [], [], []

    for i in range(frames):
        discipline_index = (worker + i) % len(DISCIPLINES)
        env = bs.Environment(getattr(bs.Discipline, DISCIPLINES[discipline_index]), 1)
        seed = int(seed_base * 1_000_003 + worker * 100_000 + i)
        env.reset(seed, int(rng.integers(0, 2)))
        seats = [profiles[int(rng.integers(0, len(profiles)))] for _ in range(2)]
        frame_id = worker * 1_000_000 + i

        start = len(labels)
        moves = 0
        while env.decision() != bs.Decision.FRAME_OVER and moves < 250:
            player = env.to_act()
            if env.decision() in (bs.Decision.SHOT, bs.Decision.PLACE):
                features.append(env.features(player))
                players.append(player)
                labels.append(-1.0)
                disciplines.append(discipline_index)
                frame_ids.append(frame_id)
            env.play_classical(seats[player], seed + moves)
            moves += 1

        winner = env.winner()
        if winner < 0:  # unfinished (very rare): drop its records
            del features[start:], players[start:], labels[start:], disciplines[start:], frame_ids[start:]
            continue
        for k in range(start, len(labels)):
            labels[k] = 1.0 if players[k] == winner else 0.0

    return {
        "x": np.asarray(features, dtype=np.float32).reshape(-1, bs.FEATURE_COUNT),
        "y": np.asarray(labels, dtype=np.float32),
        "discipline": np.asarray(disciplines, dtype=np.int8),
        "frame": np.asarray(frame_ids, dtype=np.int64),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--frames", type=int, default=6000)
    parser.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--out", default="data/selfplay.npz")
    args = parser.parse_args()

    chunk = 25
    tasks = []
    remaining = args.frames
    worker = 0
    while remaining > 0:
        tasks.append((worker, min(chunk, remaining), args.seed))
        remaining -= chunk
        worker += 1

    start = time.time()
    parts = []
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for n, part in enumerate(pool.imap_unordered(play_frames, tasks), 1):
            parts.append(part)
            if n % 10 == 0 or n == len(tasks):
                positions = sum(len(p["y"]) for p in parts)
                print(f"{n * chunk:6d} frames  {positions:8d} positions  {time.time() - start:6.0f} s", flush=True)

    data = {key: np.concatenate([p[key] for p in parts]) for key in parts[0]}
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    np.savez_compressed(args.out, **data)
    print(f"wrote {args.out}: {len(data['y'])} positions from {len(np.unique(data['frame']))} frames, "
          f"win rate of the player to act {data['y'].mean():.3f}")


if __name__ == "__main__":
    main()
