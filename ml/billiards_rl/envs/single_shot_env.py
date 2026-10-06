"""SingleShotEnv: one cue ball, one object ball, one shot per episode.

Specification: docs/rl/rl-system-design.md, Section 5. Reference and
measurements: docs/rl/environment.md.

The episode is a contextual bandit with a continuous action: reset() draws a
layout, step() plays one shot to rest with the C++ simulator and terminates.
"""

from __future__ import annotations

from typing import Any

import gymnasium as gym
import numpy as np
from gymnasium import spaces

import billiards_sim as bs
from billiards_rl.rewards.single_shot import ShotResult, reward_function

rl = bs.rl

# Bumped whenever the environment's dynamics, observation or action mapping
# change, so results and evaluation suites record what they were made with.
ENV_VERSION = "single_shot-v1"

OBS_MODES = ("positions", "positions+geometry")
ACTION_FRAMES = ("absolute", "ghost_relative")


class SingleShotEnv(gym.Env):
    """Pot the object ball with one shot.

    Observation (``obs_mode``):
        "positions"           [x_c, y_c, x_o, y_o] scaled to [-1, 1]
        "positions+geometry"  plus, per pocket: cos(cut), cue-to-ghost and
                              object-to-mouth distances / table length, blocked

    Action (``action_frame``), in [-1, 1]^2:
        "absolute"        phi = 180 deg * a0
        "ghost_relative"  phi = phi_ghost(target pocket) + max_delta_deg * a0
        and in both, speed = v_min + (a1 + 1) / 2 * (v_max - v_min).

    ``band`` selects generated layouts by difficulty (easy, medium, hard, any).
    ``layouts`` (a list of (2, 2) arrays) replaces generation with a fixed
    cycle of states, for evaluation. ``reset(options={"positions": ...})``
    plays one given layout.

    ``limits`` (max_cut, max_cue_distance, max_object_distance) generates
    shots within those limits instead of a band; a curriculum changes it.

    ``noise`` = (sigma_phi_deg, sigma_speed_relative) perturbs every executed
    shot (design, Section 10); the action and observation are unaffected.
    """

    metadata = {"render_modes": []}

    def __init__(
        self,
        band: str = "any",
        obs_mode: str = "positions",
        action_frame: str = "absolute",
        reward: str = "sparse",
        max_delta_deg: float = 4.0,
        v_min: float = 0.5,
        v_max: float = 7.0,
        noise: tuple[float, float] = (0.0, 0.0),
        layouts: list[np.ndarray] | None = None,
        limits: dict | None = None,
    ):
        super().__init__()
        if obs_mode not in OBS_MODES:
            raise ValueError(f"obs_mode must be one of {OBS_MODES}")
        if action_frame not in ACTION_FRAMES:
            raise ValueError(f"action_frame must be one of {ACTION_FRAMES}")
        self.band = band
        self.obs_mode = obs_mode
        self.action_frame = action_frame
        self.reward_name = reward
        self._reward = reward_function(reward)
        self.max_delta_deg = float(max_delta_deg)
        self.v_min = float(v_min)
        self.v_max = float(v_max)
        self.noise = (float(noise[0]), float(noise[1]))
        self._fixed = [np.asarray(p, dtype=np.float64) for p in layouts] if layouts is not None else None
        self._fixed_index = 0
        # Curriculum: generate shots within these limits instead of a band
        # (keys max_cut, max_cue_distance, max_object_distance); None = band.
        self.limits = dict(limits) if limits else None

        table = rl.table_info()
        self.width = float(table["width"])
        self.length = float(table["length"])
        self.mouths = np.asarray(table["mouths"])

        size = 4 if obs_mode == "positions" else 4 + 6 * 4
        self.observation_space = spaces.Box(-1.0, 1.0, shape=(size,), dtype=np.float32)
        self.action_space = spaces.Box(-1.0, 1.0, shape=(2,), dtype=np.float32)

        self._generator: Any = None
        self._analyser = rl.LayoutGenerator(0)
        self._positions = np.zeros((2, 2))
        self._layout: dict[str, Any] = {}
        self._aims: dict[str, np.ndarray] = {}

    # ---- Gymnasium API ------------------------------------------------------

    def reset(self, *, seed: int | None = None, options: dict[str, Any] | None = None):
        super().reset(seed=seed)
        if (self._generator is None) or (seed is not None):
            # The C++ generator is seeded from Gymnasium's np_random, so one
            # reset seed fixes the whole sequence of layouts.
            self._generator = rl.LayoutGenerator(int(self.np_random.integers(0, 2**63 - 1)))

        if options and "positions" in options:
            positions = np.asarray(options["positions"], dtype=np.float64)
            self._layout = self._analyser.analyse(positions)
        elif self._fixed is not None:
            positions = self._fixed[self._fixed_index % len(self._fixed)]
            self._fixed_index += 1
            self._layout = self._analyser.analyse(positions)
        elif self.limits is not None:
            self._layout = self._generator.single_ball_within(**self.limits)
        else:
            self._layout = self._generator.single_ball(self.band)
        self._positions = np.asarray(self._layout["positions"], dtype=np.float64)
        self._aims = rl.ghost_aims(self._positions)
        return self._observe(), self._info_reset()

    def step(self, action):
        result, info = self._play(action)
        reward = float(self._reward(result))
        return self._observe(), reward, True, False, info

    def _play(self, action) -> tuple[ShotResult, dict[str, Any]]:
        """Plays the action (with any execution noise) and describes the outcome."""
        strike, phi, speed = self.strike_for(action)
        executed = strike.copy()
        if self.noise[0] > 0.0:
            executed[1] += self.np_random.normal(0.0, self.noise[0])
        if self.noise[1] > 0.0:
            executed[0] = float(np.clip(executed[0] * (1.0 + self.np_random.normal(0.0, self.noise[1])), self.v_min, self.v_max))

        out = rl.simulate(self._positions, executed)
        result = ShotResult(
            object_pocket=int(out["pocket_of"][1]),
            scratch=bool(out["scratch"]),
            first_contact=int(out["first_contact"]),
            object_final=np.asarray(out["final_positions"][1]),
            target_pocket=self.target_pocket,
        )
        info = {
            "potted": result.potted,
            "pocket": result.object_pocket,
            "scratch": result.scratch,
            "first_contact": result.first_contact,
            "legal_contact": result.first_contact == 1,
            "object_to_pocket_after": 0.0 if result.potted else float(np.min(np.linalg.norm(self.mouths - result.object_final, axis=1))),
            "events": int(out["events"]),
            "complete": bool(out["complete"]),
            "difficulty": self._layout["difficulty"],
            "target_pocket": self.target_pocket,
            "phi": phi,
            "speed": speed,
            "success": result.potted and not result.scratch,
            "target_success": result.potted and not result.scratch and result.object_pocket == self.target_pocket,
        }
        return result, info

    # ---- Mapping -------------------------------------------------------------

    @property
    def target_pocket(self) -> int:
        """The pocket the ghost-relative frame aims around: the layout's
        easiest clean pocket, else the clear pocket of greatest ease, else -1."""
        pocket = int(self._layout.get("pocket", -1))
        if pocket >= 0:
            return pocket
        ease = np.where(self._aims["blocked"], 0.0, self._aims["ease"])
        return int(np.argmax(ease)) if np.max(ease) > 0.0 else -1

    def reference_phi(self) -> float:
        """The aim the ghost-relative frame corrects: the ghost-ball line to
        the target pocket's mouth, or a full hit on the object ball if no
        pocket is makeable."""
        pocket = self.target_pocket
        if pocket >= 0:
            return float(self._aims["phi"][pocket])
        d = self._positions[1] - self._positions[0]
        return float(np.degrees(np.arctan2(d[1], d[0])) % 360.0)

    def strike_for(self, action) -> tuple[np.ndarray, float, float]:
        """The physical strike (speed, phi, theta, a, b) for a normalised action."""
        a = np.clip(np.asarray(action, dtype=np.float64), -1.0, 1.0)
        if self.action_frame == "absolute":
            phi = 180.0 * a[0]
        else:
            phi = self.reference_phi() + self.max_delta_deg * a[0]
        phi = float(phi % 360.0)
        speed = float(self.v_min + 0.5 * (a[1] + 1.0) * (self.v_max - self.v_min))
        return np.array([speed, phi, 0.0, 0.0, 0.0]), phi, speed

    def action_for(self, phi_deg: float, speed: float) -> np.ndarray:
        """The normalised action that plays a given aim and speed (for
        baselines and tests); in the ghost-relative frame the aim must lie
        within max_delta_deg of the reference."""
        v = 2.0 * (speed - self.v_min) / (self.v_max - self.v_min) - 1.0
        if self.action_frame == "absolute":
            a0 = ((phi_deg + 180.0) % 360.0 - 180.0) / 180.0
        else:
            delta = (phi_deg - self.reference_phi() + 180.0) % 360.0 - 180.0
            a0 = delta / self.max_delta_deg
        return np.clip(np.array([a0, v], dtype=np.float32), -1.0, 1.0)

    @property
    def positions(self) -> np.ndarray:
        return self._positions.copy()

    # ---- Observation ---------------------------------------------------------

    def _observe(self) -> np.ndarray:
        p = self._positions
        scaled = np.array([
            2.0 * p[0, 0] / self.width - 1.0, 2.0 * p[0, 1] / self.length - 1.0,
            2.0 * p[1, 0] / self.width - 1.0, 2.0 * p[1, 1] / self.length - 1.0,
        ])
        if self.obs_mode == "positions":
            return scaled.astype(np.float32)
        geometry = np.column_stack([
            np.cos(np.radians(self._aims["cut"])),
            np.clip(self._aims["cue_distance"] / self.length, 0.0, 1.0),
            np.clip(self._aims["object_distance"] / self.length, 0.0, 1.0),
            np.where(self._aims["blocked"], 1.0, -1.0),
        ]).reshape(-1)
        return np.clip(np.concatenate([scaled, geometry]), -1.0, 1.0).astype(np.float32)

    def _info_reset(self) -> dict[str, Any]:
        return {"difficulty": self._layout["difficulty"], "target_pocket": self.target_pocket}
