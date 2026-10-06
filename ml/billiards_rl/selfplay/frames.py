"""Full frames under WPA rules between classical planners (optionally with a
learned value), for self-play data and head-to-head evaluation."""

from __future__ import annotations

import numpy as np

import billiards_sim as bs

MAX_MOVES = 250


def play_frame(rack_seed: int, breaker: int, profile: str, values: tuple, record: bool = False,
               discipline: str = "NINE_BALL") -> tuple[int, list[tuple[np.ndarray, int]]]:
    """Plays one frame; values[p] is player p's value callable (or None: the
    hand-made scoring). Returns (winner or -1 if unfinished, records), records
    being (features, player) for both players' points of view before every
    shot or cue-ball placement."""
    env = bs.Environment(getattr(bs.Discipline, discipline), 1)
    env.reset(rack_seed, breaker)
    records = []
    moves = 0
    while env.decision() != bs.Decision.FRAME_OVER and moves < MAX_MOVES:
        if record and env.decision() in (bs.Decision.SHOT, bs.Decision.PLACE):
            for p in (0, 1):
                records.append((env.features(p), p))
        player = env.to_act()
        env.play_classical(profile, (rack_seed * 7919 + moves) % 2**32, True, values[player])
        moves += 1
    return env.winner(), records
