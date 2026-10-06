"""Scripted policies every learned policy is compared with.

A policy here is any object with ``act(env, obs) -> action``: it may read the
environment's layout (``env.positions``), which learned policies never do, so
baselines are privileged by design and are labelled as such in results.
"""

from billiards_rl.baselines.geometry import GeometryPolicy
from billiards_rl.baselines.position_search import PositionSearchPolicy
from billiards_rl.baselines.random_policy import RandomPolicy
from billiards_rl.baselines.sim_search import SimSearchPolicy

__all__ = ["GeometryPolicy", "PositionSearchPolicy", "RandomPolicy", "SimSearchPolicy", "make_baseline"]


def make_baseline(name: str, seed: int = 0):
    if name == "random":
        return RandomPolicy(seed)
    if name == "geometry":
        return GeometryPolicy()
    if name == "sim_search":
        return SimSearchPolicy()
    if name == "position_search":
        return PositionSearchPolicy()
    raise ValueError(f"Unknown baseline '{name}' (random, geometry, sim_search, position_search)")
