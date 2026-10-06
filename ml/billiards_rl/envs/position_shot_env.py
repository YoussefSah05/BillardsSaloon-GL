"""PositionShotEnv: pot the object ball and leave the cue ball on a target (Milestone 10).

The step from shot making to position play (rl-system-design.md, Section 14).

Action, in [-1, 1]^4:

    aim    as SingleShotEnv (``absolute`` or ``ghost_relative``)
    speed  as SingleShotEnv
    side   a = -0.525 * a2  (simulator a > 0 is left english; +a2 is right english)
    vert   b =  0.525 * a3  (follow above, draw below), fractions of R as in the game

``spin=False`` locks a and b at zero (the ablation), keeping the 4-D action.

Observation (8): cue ball, object ball, the target pocket's mouth and the cue
ball's target, in normalised table coordinates.

Targets are feasible by construction: the geometric aim at the target pocket
is played with a random speed (0.7-1.8 times the friction model's) and random
spin until it pots without a scratch; the cue ball's resting spot becomes the
target. Up to 12 tries per layout, then a new layout.

Reward:

    scratch                      -1
    no pot in the target pocket  -0.1
    pot                          1 + max(0, 1 - d / position_radius)

with d the cue ball's distance from its target (metres): a pot is always
better than a miss, and position adds up to +1. Position success: a pot with
the cue ball within ``success_distance`` of the target.
"""

from __future__ import annotations

from typing import Any

import numpy as np
from gymnasium import spaces

import billiards_sim as bs
from billiards_rl.envs.single_shot_env import SingleShotEnv

rl = bs.rl
MAX_TIP_OFFSET = 0.525   # the game's range: 0.75 x 0.7 of R


class PositionShotEnv(SingleShotEnv):
    def __init__(self, spin: bool = True, position_radius: float = 0.5, success_distance: float = 0.15,
                 cue_targets: list[np.ndarray] | None = None, **kwargs):
        kwargs.setdefault("reward", "sparse_target")
        kwargs["obs_mode"] = "positions+target"
        super().__init__(**kwargs)
        self.spin = bool(spin)
        self.position_radius = float(position_radius)
        self.success_distance = float(success_distance)
        self._fixed_targets = [np.asarray(t, dtype=np.float64) for t in cue_targets] if cue_targets is not None else None
        self.observation_space = spaces.Box(-1.0, 1.0, shape=(8,), dtype=np.float32)
        self.action_space = spaces.Box(-1.0, 1.0, shape=(4,), dtype=np.float32)
        self._cue_target = np.zeros(2)

    # ---- Layouts with feasible cue-ball targets ---------------------------------

    def reset(self, *, seed: int | None = None, options: dict[str, Any] | None = None):
        if options and "cue_target" in options:
            observation, info = super().reset(seed=seed, options=options)
            self._cue_target = np.asarray(options["cue_target"], dtype=np.float64)
            return self._position_obs(observation), self._position_info(info)
        if self._fixed is not None:
            index = self._fixed_index % len(self._fixed)
            observation, info = super().reset(seed=seed, options=options)
            self._cue_target = self._fixed_targets[index]
            return self._position_obs(observation), self._position_info(info)

        for _ in range(100):
            observation, info = super().reset(seed=seed, options=options)
            seed = None
            target = self.feasible_target(self._positions, self.target_pocket, self.np_random)
            if target is not None:
                self._cue_target = target
                return self._position_obs(observation), self._position_info(info)
        raise RuntimeError("No layout with a feasible cue-ball target found")

    @staticmethod
    def feasible_target(positions: np.ndarray, pocket: int, rng: np.random.Generator, tries: int = 12) -> np.ndarray | None:
        """A cue-ball resting spot reachable by potting into `pocket`, or None."""
        if pocket < 0:
            return None
        shot = rl.geometry_shot(positions, pocket=pocket)
        if shot is None:
            return None
        speed0, phi = shot["strike"][0], shot["strike"][1]
        strikes = np.zeros((tries, 5))
        strikes[:, 0] = np.clip(speed0 * rng.uniform(0.7, 1.8, tries), 0.5, 7.0)
        strikes[:, 1] = phi
        strikes[:, 3] = rng.uniform(-0.3, 0.3, tries) * MAX_TIP_OFFSET / 0.525
        strikes[:, 4] = rng.uniform(-0.5, 0.5, tries)
        out = rl.simulate_batch(np.broadcast_to(positions, (tries,) + positions.shape).copy(), strikes, 1)
        ok = np.flatnonzero((out["pocket_of"][:, 1] == pocket) & ~out["scratch"])
        if ok.size == 0:
            return None
        return np.asarray(out["final_positions"][ok[0], 0])

    # ---- Acting -------------------------------------------------------------

    def strike_for(self, action) -> tuple[np.ndarray, float, float]:
        a = np.clip(np.asarray(action, dtype=np.float64), -1.0, 1.0)
        strike, phi, speed = super().strike_for(a[:2])
        if self.spin:
            strike[3] = -MAX_TIP_OFFSET * a[2]
            strike[4] = MAX_TIP_OFFSET * a[3]
        return strike, phi, speed

    def action_for(self, phi_deg: float, speed: float, side: float = 0.0, vertical: float = 0.0) -> np.ndarray:
        base = super().action_for(phi_deg, speed)
        return np.array([base[0], base[1], -side / MAX_TIP_OFFSET, vertical / MAX_TIP_OFFSET], dtype=np.float32).clip(-1, 1)

    def _play(self, action):
        strike, phi, speed = self.strike_for(action)
        executed = strike.copy()
        if self.noise[0] > 0.0:
            executed[1] += self.np_random.normal(0.0, self.noise[0])
        if self.noise[1] > 0.0:
            executed[0] = float(np.clip(executed[0] * (1.0 + self.np_random.normal(0.0, self.noise[1])), self.v_min, self.v_max))
        out = rl.simulate(self._positions, executed)
        pocket = int(out["pocket_of"][1])
        scratch = bool(out["scratch"])
        cue_final = np.asarray(out["final_positions"][0])
        distance = float(np.linalg.norm(cue_final - self._cue_target))
        pot = (pocket == self.target_pocket) and not scratch
        info = {
            "potted": pocket >= 0, "pocket": pocket, "scratch": scratch,
            "first_contact": int(out["first_contact"]), "legal_contact": int(out["first_contact"]) == 1,
            "object_to_pocket_after": 0.0, "events": int(out["events"]), "complete": bool(out["complete"]),
            "difficulty": self._layout["difficulty"], "target_pocket": self.target_pocket,
            "phi": phi, "speed": speed, "side": float(strike[3]), "vertical": float(strike[4]),
            "success": (pocket >= 0) and not scratch, "target_success": pot,
            "cue_distance": distance, "position_success": pot and distance < self.success_distance,
        }
        return info

    def step(self, action):
        info = self._play(action)
        if info["scratch"]:
            reward = -1.0
        elif info["target_success"]:
            reward = 1.0 + max(0.0, 1.0 - info["cue_distance"] / self.position_radius)
        else:
            reward = -0.1
        return self._position_obs(self._observe()), reward, True, False, info

    # ---- Observation ---------------------------------------------------------

    def _position_obs(self, observation: np.ndarray) -> np.ndarray:
        t = self._cue_target
        target = np.array([2.0 * t[0] / self.width - 1.0, 2.0 * t[1] / self.length - 1.0])
        return np.clip(np.concatenate([observation, target]), -1.0, 1.0).astype(np.float32)

    def _position_info(self, info: dict) -> dict:
        return {**info, "cue_target": self._cue_target.copy()}

    @property
    def cue_target(self) -> np.ndarray:
        return self._cue_target.copy()
