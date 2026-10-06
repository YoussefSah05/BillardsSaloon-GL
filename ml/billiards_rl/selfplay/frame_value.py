"""A frame-win value for the classical planner under full WPA rules (Milestone 13).

$V_\\theta(s, p) \\approx P(\\text{player } p \\text{ wins the frame} \\mid s)$
from the game's shared feature vector (`billiards_sim.FEATURE_COUNT` floats:
16 ball slots and the rules context, from $p$'s point of view). Fitted by
binary cross-entropy on self-play frame outcomes:

$$
\\mathcal L(\\theta) = -\\frac1N \\sum_i \\big[ z_i \\log V_\\theta(x_i) + (1 - z_i) \\log(1 - V_\\theta(x_i)) \\big],
$$

$z_i = 1$ if the player whose point of view $x_i$ takes won. Given to the
planner, $V$ replaces its hand-made scoring of every simulated outcome
(score $= 100\\,(2V - 1)$). Inference runs in NumPy: the planner calls it a
few hundred times per shot, one table at a time.
"""

from __future__ import annotations

import numpy as np
import torch
from torch import nn


class FrameValueNet(nn.Module):
    def __init__(self, features: int, hidden: int = 256, layers: int = 2):
        super().__init__()
        blocks, width = [], features
        for _ in range(layers):
            blocks += [nn.Linear(width, hidden), nn.ReLU()]
            width = hidden
        self.net = nn.Sequential(*blocks, nn.Linear(width, 1))

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x).squeeze(-1)   # logits


class NumpyValue:
    """The fitted network as a fast callable: features -> P(win)."""

    def __init__(self, net: FrameValueNet):
        linears = [m for m in net.net if isinstance(m, nn.Linear)]
        self.weights = [m.weight.detach().numpy().T.astype(np.float64) for m in linears]
        self.biases = [m.bias.detach().numpy().astype(np.float64) for m in linears]

    def logits(self, x: np.ndarray) -> np.ndarray:
        h = np.asarray(x, dtype=np.float64)
        for i, (w, b) in enumerate(zip(self.weights, self.biases)):
            h = h @ w + b
            if i < len(self.weights) - 1:
                h = np.maximum(h, 0.0)
        return h[..., 0]

    def __call__(self, features: np.ndarray) -> float:
        return float(1.0 / (1.0 + np.exp(-self.logits(features))))

    def state(self) -> dict:
        return {"weights": self.weights, "biases": self.biases}

    @classmethod
    def from_state(cls, state: dict) -> "NumpyValue":
        value = cls.__new__(cls)
        value.weights, value.biases = state["weights"], state["biases"]
        return value


def log_loss(p: np.ndarray, z: np.ndarray) -> float:
    p = np.clip(p, 1e-7, 1 - 1e-7)
    return float(-np.mean(z * np.log(p) + (1 - z) * np.log(1 - p)))


def fit_frame_value(x: np.ndarray, z: np.ndarray, frames: np.ndarray, hidden: int = 256, layers: int = 2,
                    epochs: int = 30, batch: int = 512, lr: float = 1e-3, weight_decay: float = 1e-4,
                    seed: int = 0, validation: float = 0.1) -> tuple[FrameValueNet, dict]:
    """Splits by frame (no frame in both sets); keeps the epoch with the lowest
    validation log loss. Reports the base rate and a logistic regression."""
    rng = np.random.default_rng(seed)
    ids = np.unique(frames)
    held = set(rng.choice(ids, max(1, int(validation * len(ids))), replace=False).tolist())
    is_val = np.array([f in held for f in frames])
    xt, zt = torch.as_tensor(x[~is_val]), torch.as_tensor(z[~is_val], dtype=torch.float32)
    xv, zv = torch.as_tensor(x[is_val]), z[is_val]

    def train(model: nn.Module) -> tuple[nn.Module, float]:
        torch.manual_seed(seed)
        opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=weight_decay)
        gen = torch.Generator().manual_seed(seed)
        best, best_state = float("inf"), None
        for _ in range(epochs):
            model.train()
            order = torch.randperm(len(zt), generator=gen)
            for k in range(0, len(order), batch):
                idx = order[k:k + batch]
                loss = nn.functional.binary_cross_entropy_with_logits(model(xt[idx]), zt[idx])
                opt.zero_grad()
                loss.backward()
                opt.step()
            model.eval()
            with torch.no_grad():
                v = log_loss(torch.sigmoid(model(xv)).numpy(), zv)
            if v < best:
                best, best_state = v, {k: t.clone() for k, t in model.state_dict().items()}
        model.load_state_dict(best_state)
        return model, best

    net, net_loss = train(FrameValueNet(x.shape[1], hidden, layers))
    _, linear_loss = train(FrameValueNet(x.shape[1], hidden, layers=0))
    base = float(zt.mean())
    with torch.no_grad():
        accuracy = float(((torch.sigmoid(net(xv)).numpy() > 0.5) == (zv > 0.5)).mean())
    return net, {"val_log_loss": net_loss, "logistic_log_loss": linear_loss,
                 "base_rate_log_loss": log_loss(np.full(len(zv), base), zv), "val_accuracy": accuracy,
                 "train_positions": int((~is_val).sum()), "val_positions": int(is_val.sum())}
