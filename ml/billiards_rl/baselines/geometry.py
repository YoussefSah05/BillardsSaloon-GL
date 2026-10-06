"""The geometry solver: ghost-ball aim at the easiest pocket mouth.

Deterministic and model-free at decision time (no simulator calls): the aim
comes from the ghost-ball construction towards the pocket's mouth, the speed
from a friction model (docs/rl/environment.md). It knows nothing about throw
or jaws, which is what a learned policy can improve on.
"""

import numpy as np

import billiards_sim as bs

rl = bs.rl


class GeometryPolicy:
    name = "geometry"
    privileged = True   # reads the layout

    def act(self, env, obs):
        positions = env.positions
        shot = rl.geometry_shot(positions)
        if shot is None:
            # No clean line: a full hit on the object ball at medium pace.
            d = positions[1] - positions[0]
            return env.action_for(float(np.degrees(np.arctan2(d[1], d[0]))), 2.5)
        speed, phi = shot["strike"][0], shot["strike"][1]
        return env.action_for(phi, speed)
