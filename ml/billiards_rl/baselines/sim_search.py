"""Simulator search: an upper bound that uses the simulator as a perfect model.

For the clearest pockets and a few speeds, the aim is scanned around the
geometry solution with the real simulator (one batch). Among the
(pocket, speed) pairs, the one with the widest contiguous run of potting aims
is played, at the centre of that run. Wide windows are robust to execution
error, so this choice also behaves well under noise without sampling it.

It is not a competitor on equal terms with a learned policy, which cannot
simulate before acting; it shows what is achievable in this simulator.
"""

import numpy as np

import billiards_sim as bs

rl = bs.rl


class SimSearchPolicy:
    name = "sim_search"
    privileged = True   # reads the layout and simulates before acting

    def __init__(self, span_deg: float = 2.0, step_deg: float = 0.05,
                 speed_factors=(0.8, 1.0, 1.3), pockets: int = 3, threads: int = 0):
        self.offsets = np.arange(-span_deg, span_deg + 1e-9, step_deg)
        self.speed_factors = tuple(speed_factors)
        self.pockets = pockets
        self.threads = threads

    def act(self, env, obs):
        positions = env.positions
        aims = rl.ghost_aims(positions)
        ease = np.where(aims["blocked"], 0.0, aims["ease"])
        candidates = [int(k) for k in np.argsort(-ease)[: self.pockets] if ease[k] > 0.0]
        if not candidates:
            return GeometryFallback.act(env, positions)

        strikes, keys = [], []
        for k in candidates:
            shot = rl.geometry_shot(positions, pocket=k)
            if shot is None:
                continue
            base_speed, base_phi = shot["strike"][0], shot["strike"][1]
            for factor in self.speed_factors:
                speed = float(np.clip(base_speed * factor, env.v_min, env.v_max))
                for offset in self.offsets:
                    strikes.append((speed, (base_phi + offset) % 360.0, 0.0, 0.0, 0.0))
                    keys.append((k, speed))
        if not strikes:
            return GeometryFallback.act(env, positions)

        strikes = np.asarray(strikes)
        layouts = np.broadcast_to(positions, (len(strikes),) + positions.shape).copy()
        out = rl.simulate_batch(layouts, strikes, self.threads)
        success = (out["pocket_of"][:, 1] >= 0) & ~out["scratch"]

        # Widest contiguous run of successes per (pocket, speed) block.
        best = (-1, None)
        n = len(self.offsets)
        for block in range(len(strikes) // n):
            hits = success[block * n:(block + 1) * n]
            run_start, run_len, start = 0, 0, None
            for i, hit in enumerate(np.append(hits, False)):
                if hit and start is None:
                    start = i
                elif not hit and start is not None:
                    if i - start > run_len:
                        run_start, run_len = start, i - start
                    start = None
            if run_len > best[0]:
                centre = block * n + run_start + (run_len - 1) / 2.0
                best = (run_len, centre)
        if best[0] <= 0:
            # Nothing pots: fall back to the geometry aim.
            return GeometryFallback.act(env, positions)
        index_low = int(np.floor(best[1]))
        index_high = int(np.ceil(best[1]))
        phi = 0.5 * (strikes[index_low, 1] + strikes[index_high, 1])
        if abs(strikes[index_low, 1] - strikes[index_high, 1]) > 180.0:   # wrapped around 0/360
            phi = (phi + 180.0) % 360.0
        return env.action_for(phi, strikes[index_low, 0])


class GeometryFallback:
    @staticmethod
    def act(env, positions):
        shot = rl.geometry_shot(positions)
        if shot is None:
            d = positions[1] - positions[0]
            return env.action_for(float(np.degrees(np.arctan2(d[1], d[0]))), 2.5)
        return env.action_for(shot["strike"][1], shot["strike"][0])
