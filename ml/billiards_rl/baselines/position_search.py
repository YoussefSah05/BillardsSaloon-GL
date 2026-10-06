"""Position search: an upper bound for position play (Milestone 10).

Around the geometric aim at the target pocket, the simulator plays a grid of
aims (±1° in 0.05° steps), speeds (0.6-2.2 times the friction model's), follow
or draw (b in [-0.5, 0.5]) and side spin (a in {-0.3, 0, 0.3}); among the
shots that pot in the target pocket without a scratch, the one that leaves the
cue ball nearest its target is played. Privileged: it reads the layout and the
target and simulates about 3,700 shots before acting.
"""

import numpy as np

import billiards_sim as bs

rl = bs.rl


class PositionSearchPolicy:
    name = "position_search"
    privileged = True

    def __init__(self, threads: int = 0):
        self.offsets = np.arange(-1.0, 1.0 + 1e-9, 0.05)
        self.speed_factors = (0.6, 0.8, 1.0, 1.3, 1.7, 2.2)
        self.verticals = (-0.5, -0.25, 0.0, 0.25, 0.5)
        self.sides = (-0.3, 0.0, 0.3)
        self.threads = threads

    def act(self, env, obs):
        positions, target, pocket = env.positions, env.cue_target, env.target_pocket
        shot = rl.geometry_shot(positions, pocket=pocket) if pocket >= 0 else None
        if shot is None:
            d = positions[1] - positions[0]
            return env.action_for(float(np.degrees(np.arctan2(d[1], d[0]))), 2.5)
        speed0, phi0 = shot["strike"][0], shot["strike"][1]
        grid = np.array([(np.clip(speed0 * f, 0.5, 7.0), (phi0 + o) % 360.0, 0.0, a, b)
                         for f in self.speed_factors for b in self.verticals for a in self.sides for o in self.offsets])
        out = rl.simulate_batch(np.broadcast_to(positions, (len(grid),) + positions.shape).copy(), grid, self.threads)
        ok = (out["pocket_of"][:, 1] == pocket) & ~out["scratch"]
        if not ok.any():
            return env.action_for(phi0, speed0)
        distance = np.linalg.norm(out["final_positions"][:, 0] - target, axis=1)
        best = np.flatnonzero(ok)[np.argmin(distance[ok])]
        speed, phi, _, side, vertical = grid[best]
        return env.action_for(phi, speed, side=side, vertical=vertical)
