"""Rewards for one shot, computed from its outcome only.

Every reward here is a function of the shot's outcome (and the episode's
target), never of the simulator's internals, so environments and rewards can
be combined freely and a reward change never changes the physics. Rationale
and the rules for adding shaped variants: docs/rl/reward-design.md.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

import numpy as np


@dataclass(frozen=True)
class ShotResult:
    """What a single-ball shot did, from billiards_sim.rl.simulate."""

    object_pocket: int          # pocket the object ball dropped into, -1 if none
    scratch: bool               # the cue ball was pocketed
    first_contact: int          # first ball the cue ball touched, -1 if none
    object_final: np.ndarray    # (2,) where the object ball came to rest (if not potted)
    target_pocket: int          # the episode's intended pocket, -1 if any pocket counts

    @property
    def potted(self) -> bool:
        return self.object_pocket >= 0


def sparse(result: ShotResult) -> float:
    """+1 for a pot without a scratch, -1 for a scratch, -0.1 otherwise.

    Any pocket counts. This is the brief's reward for the single-shot task:
    it is unshaped, so what is learned is not an artefact of shaping.
    """
    if result.scratch:
        return -1.0
    if result.potted:
        return 1.0
    return -0.1


def sparse_target(result: ShotResult) -> float:
    """As `sparse`, but only the episode's target pocket counts as a pot.

    Potting into another pocket scores like a miss. Used with goal-conditioned
    environments, where the pocket is part of the goal.
    """
    if result.scratch:
        return -1.0
    if result.potted and (result.target_pocket < 0 or result.object_pocket == result.target_pocket):
        return 1.0
    return -0.1


REWARDS: dict[str, Callable[[ShotResult], float]] = {
    "sparse": sparse,
    "sparse_target": sparse_target,
}


def reward_function(name: str) -> Callable[[ShotResult], float]:
    try:
        return REWARDS[name]
    except KeyError as error:
        raise ValueError(f"Unknown reward '{name}'; choose from {sorted(REWARDS)}") from error
