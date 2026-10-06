"""A value network for RunEnv states: expected balls still to be potted.

$V_\\theta(s) \\approx \\mathbb E\\big[\\sum_{t' \\ge t} r_{t'} \\mid s_t = s\\big]$
under the search player and its execution noise, where $r$ is +1 per ball
potted and −1 for a scratch.

Permutation-invariant over object balls (DeepSets):

$$
V_\\theta(s) = \\rho\\Big(g,\\ \\frac1n \\sum_{i=1}^{n} \\phi(x_i, g),\\ \\max_i \\phi(x_i, g)\\Big),
$$

with $g$ the global features (cue ball position, share of balls left) and
$x_i$ per object ball: its position, its offset from the cue ball, the
distance, the best ghost-ball ease over its open pockets and the share of
pockets open to it (clear line, cut below 75°).
"""

from __future__ import annotations

import numpy as np
import torch
from torch import nn

import billiards_sim as bs

rl = bs.rl
BALL_FEATURES = 7
GLOBAL_FEATURES = 3


class StateEncoder:
    def __init__(self, max_balls: int = 3, max_cut: float = 75.0):
        table = rl.table_info()
        self.width, self.length = float(table["width"]), float(table["length"])
        self.max_balls = max_balls
        self.max_cut = max_cut

    def encode(self, states: list[np.ndarray]) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """(balls N x B x F, mask N x B, global N x G)."""
        n = len(states)
        balls = np.zeros((n, self.max_balls, BALL_FEATURES), dtype=np.float32)
        mask = np.zeros((n, self.max_balls), dtype=bool)
        glob = np.zeros((n, GLOBAL_FEATURES), dtype=np.float32)
        scale = np.array([self.width, self.length])
        for i, positions in enumerate(states):
            cue = positions[0]
            glob[i] = [2 * cue[0] / self.width - 1, 2 * cue[1] / self.length - 1, (len(positions) - 1) / self.max_balls]
            for j in range(1, len(positions)):
                aims = rl.ghost_aims(positions, j)
                open_ = ~np.asarray(aims["blocked"]) & (np.asarray(aims["cut"]) <= self.max_cut)
                ease = float(np.max(np.where(open_, aims["ease"], 0.0)))
                d = (positions[j] - cue) / scale
                balls[i, j - 1] = [2 * positions[j][0] / self.width - 1, 2 * positions[j][1] / self.length - 1,
                                   d[0], d[1], float(np.hypot(*d)), ease, open_.mean()]
                mask[i, j - 1] = True
        return balls, mask, glob


class ValueNet(nn.Module):
    def __init__(self, hidden: int = 128):
        super().__init__()
        self.phi = nn.Sequential(nn.Linear(BALL_FEATURES + GLOBAL_FEATURES, hidden), nn.ReLU(),
                                 nn.Linear(hidden, hidden), nn.ReLU())
        self.rho = nn.Sequential(nn.Linear(2 * hidden + GLOBAL_FEATURES, hidden), nn.ReLU(),
                                 nn.Linear(hidden, 1))

    def forward(self, balls: torch.Tensor, mask: torch.Tensor, glob: torch.Tensor) -> torch.Tensor:
        x = torch.cat([balls, glob.unsqueeze(1).expand(-1, balls.shape[1], -1)], -1)
        h = self.phi(x)
        m = mask.unsqueeze(-1).float()
        count = m.sum(1).clamp(min=1.0)
        mean = (h * m).sum(1) / count
        top = h.masked_fill(~mask.unsqueeze(-1), -1e9).max(1).values
        top = torch.where(mask.any(1, keepdim=True), top, torch.zeros_like(top))
        return self.rho(torch.cat([mean, top, glob], -1)).squeeze(-1)


class LearnedValue:
    """value_fn for ExpectimaxPlayer: V(s') for a list of states."""

    def __init__(self, net: ValueNet, encoder: StateEncoder | None = None):
        self.net = net.eval()
        self.encoder = encoder or StateEncoder()

    def __call__(self, states: list[np.ndarray]) -> np.ndarray:
        balls, mask, glob = self.encoder.encode(states)
        with torch.no_grad():
            v = self.net(torch.as_tensor(balls), torch.as_tensor(mask), torch.as_tensor(glob))
        return v.numpy().astype(np.float64)


def fit_value(net: ValueNet, balls: np.ndarray, mask: np.ndarray, glob: np.ndarray, targets: np.ndarray,
              epochs: int = 40, batch: int = 256, lr: float = 1e-3, seed: int = 0,
              validation: float = 0.1) -> dict:
    """Mean-squared-error regression; keeps the epoch with the lowest held-out loss."""
    gen = torch.Generator().manual_seed(seed)
    n = len(targets)
    perm = torch.randperm(n, generator=gen)
    n_val = max(1, int(validation * n))
    val, train = perm[:n_val], perm[n_val:]
    tensors = [torch.as_tensor(a) for a in (balls, mask, glob)] + [torch.as_tensor(targets, dtype=torch.float32)]
    opt = torch.optim.Adam(net.parameters(), lr=lr)
    best, best_state, history = float("inf"), None, []
    for _ in range(epochs):
        net.train()
        order = train[torch.randperm(len(train), generator=gen)]
        for k in range(0, len(order), batch):
            idx = order[k:k + batch]
            b, m, g, y = (t[idx] for t in tensors)
            loss = ((net(b, m, g) - y) ** 2).mean()
            opt.zero_grad()
            loss.backward()
            opt.step()
        net.eval()
        with torch.no_grad():
            b, m, g, y = (t[val] for t in tensors)
            v = float(((net(b, m, g) - y) ** 2).mean())
        history.append(v)
        if v < best:
            best, best_state = v, {k: t.clone() for k, t in net.state_dict().items()}
    net.load_state_dict(best_state)
    variance = float(np.var(targets[val.numpy()]))
    return {"train_states": len(train), "val_mse": best, "val_target_variance": variance, "history": history}
