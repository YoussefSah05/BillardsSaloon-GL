"""RunEnv: clear a small table, one goal at a time (Milestone 11).

A cue ball and `balls` object balls. The player keeps shooting while it pots:
no game rules, any order. The episode ends on a miss, a scratch or a cleared
table. Reward: +1 per object ball potted, −1 for a scratch.

The strategic decision of each turn is a **goal**: an (object ball, pocket)
pair whose ghost-ball line is clear and whose cut is below 75°, the
`max_candidates` easiest by ease. An **executor** turns the goal into a
physical shot (the learned shot policy π_L, or geometry). This separates the
two layers of rl-system-design.md, Section 14:

    π_H(s) → goal (ball, pocket)      this module's strategies
    π_L(s, goal) → shot               executors.py

The candidate features per goal (normalised): cos(cut), cue-to-ghost and
object-to-mouth distances / L, ease, the object ball's position, the mouth's
position; global features: the cue ball's position and the share of balls left.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

import billiards_sim as bs

rl = bs.rl
CANDIDATE_FEATURES = 8
GLOBAL_FEATURES = 3


@dataclass
class Candidate:
    ball: int          # index into the current positions (1..n)
    pocket: int
    phi: float
    cut: float
    cue_distance: float
    object_distance: float
    ease: float


class RunEnv:
    def __init__(self, balls: int = 3, max_candidates: int = 8, noise: tuple[float, float] = (0.0, 0.0),
                 layouts: list[np.ndarray] | None = None, max_cut: float = 75.0):
        table = rl.table_info()
        self.width, self.length = float(table["width"]), float(table["length"])
        self.mouths = np.asarray(table["mouths"])
        self.balls = balls
        self.max_candidates = max_candidates
        self.noise = noise
        self.max_cut = max_cut
        self._fixed = [np.asarray(p, dtype=np.float64) for p in layouts] if layouts is not None else None
        self._index = 0
        self._checker = rl.LayoutGenerator(0)
        self.rng = np.random.default_rng(0)
        self.positions = np.zeros((1 + balls, 2))
        self.candidates: list[Candidate] = []

    # ---- Layouts --------------------------------------------------------------

    def random_layout(self, rng: np.random.Generator) -> np.ndarray:
        """A legal layout with at least one candidate goal."""
        r = rl.table_info()["ball_radius"]
        for _ in range(1000):
            placed: list[np.ndarray] = []
            while len(placed) < 1 + self.balls:
                p = np.array([rng.uniform(r, self.width - r), rng.uniform(r, self.length - r)])
                others = np.array(placed) if placed else np.zeros((0, 2))
                if self._checker.legal_spot(float(p[0]), float(p[1]), others):
                    placed.append(p)
            positions = np.round(np.array(placed), 6)
            if self._candidates_for(positions):
                return positions
        raise RuntimeError("no layout with a candidate goal")

    def reset(self, seed: int | None = None) -> None:
        if seed is not None:
            self.rng = np.random.default_rng(seed)
        if self._fixed is not None:
            self.positions = self._fixed[self._index % len(self._fixed)].copy()
            self._index += 1
        else:
            self.positions = self.random_layout(self.rng)
        self.candidates = self._candidates_for(self.positions)

    # ---- Goals ----------------------------------------------------------------

    def _candidates_for(self, positions: np.ndarray) -> list[Candidate]:
        found = []
        for ball in range(1, len(positions)):
            aims = rl.ghost_aims(positions, ball)
            for pocket in range(6):
                if aims["blocked"][pocket] or aims["cut"][pocket] > self.max_cut:
                    continue
                found.append(Candidate(ball, pocket, float(aims["phi"][pocket]), float(aims["cut"][pocket]),
                                       float(aims["cue_distance"][pocket]), float(aims["object_distance"][pocket]),
                                       float(aims["ease"][pocket])))
        found.sort(key=lambda c: -c.ease)
        return found[: self.max_candidates]

    def _norm(self, p) -> np.ndarray:
        return np.array([2.0 * p[0] / self.width - 1.0, 2.0 * p[1] / self.length - 1.0])

    def features(self) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """(candidates K x F, mask K, global G) for the current turn."""
        k = self.max_candidates
        feats = np.zeros((k, CANDIDATE_FEATURES), dtype=np.float32)
        mask = np.zeros(k, dtype=bool)
        for i, c in enumerate(self.candidates):
            feats[i] = [np.cos(np.radians(c.cut)), c.cue_distance / self.length, c.object_distance / self.length, c.ease,
                        *self._norm(self.positions[c.ball]), *self._norm(self.mouths[c.pocket])]
            mask[i] = True
        glob = np.array([*self._norm(self.positions[0]), (len(self.positions) - 1) / self.balls], dtype=np.float32)
        return feats, mask, glob

    # ---- Playing --------------------------------------------------------------

    def play(self, candidate: Candidate, executor) -> tuple[float, bool, dict]:
        """Executes a goal; returns (reward, episode over, info)."""
        strike = np.asarray(executor.strike(self.positions, candidate.ball, candidate.pocket), dtype=np.float64)
        return self.play_strike(strike, candidate.ball, candidate.pocket)

    def play_strike(self, strike: np.ndarray, ball: int = -1, pocket: int = -1) -> tuple[float, bool, dict]:
        """Plays a physical strike (with this env's execution noise)."""
        strike = np.asarray(strike, dtype=np.float64).copy()
        if self.noise[0] > 0.0:
            strike[1] += self.rng.normal(0.0, self.noise[0])
        if self.noise[1] > 0.0:
            strike[0] = float(np.clip(strike[0] * (1.0 + self.rng.normal(0.0, self.noise[1])), 0.5, 7.0))
        out = rl.simulate(self.positions, strike)
        pocketed = out["pocket_of"] >= 0
        scratch = bool(out["scratch"])
        potted = int(pocketed[1:].sum())
        reward = float(potted) - (1.0 if scratch else 0.0)
        info = {"potted": potted, "scratch": scratch,
                "intended": bool(ball > 0 and pocketed[ball] and out["pocket_of"][ball] == pocket)}
        if scratch:
            return reward, True, info
        remaining = [0] + [i for i in range(1, len(self.positions)) if not pocketed[i]]
        self.positions = np.asarray(out["final_positions"])[remaining]
        if len(self.positions) == 1:
            return reward, True, {**info, "cleared": True}
        if potted == 0:
            return reward, True, info
        self.candidates = self._candidates_for(self.positions)
        if not self.candidates:
            return reward, True, {**info, "no_shot": True}
        return reward, False, info

    def run_episode(self, strategy, executor, seed: int | None = None) -> dict:
        self.reset(seed)
        total, shots, cleared = 0.0, 0, False
        while True:
            choice = strategy.choose(self)
            reward, done, info = self.play(self.candidates[choice], executor)
            total += reward
            shots += 1
            if done:
                cleared = bool(info.get("cleared", False))
                return {"return": total, "shots": shots, "cleared": cleared, "balls": self.balls}
