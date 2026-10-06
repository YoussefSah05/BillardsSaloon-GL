"""Elite regression and advantage-weighted regression for one-shot episodes.

Every episode is one shot: a contextual bandit with a continuous action. A
Gaussian policy $\\pi_\\theta(a \\mid s) = \\mathcal N(\\mu_\\theta(s), \\sigma_\\theta(s)^2)$
samples shots; the policy is then fitted to past shots by weighted maximum
likelihood,

$$
\\max_\\theta \\sum_i w_i \\log \\pi_\\theta(a_i \\mid s_i),
$$

with weights

- **elite** (filtered imitation, or reward-weighted regression with a 0/1
  weight): $w_i = 1$ if shot $i$ succeeded, else 0;
- **AWR** (Peng et al. 2019): $w_i = \\min(\\exp((r_i - V_\\phi(s_i))/\\beta), w_\\max)$,
  with $V_\\phi$ regressed on the rewards.

Regression puts the mean at the centre of the successful shots for each
state, which is the aim-centring that made simulator search robust
(Experiment 3), and the fitted spread shrinks where the policy is precise. A
small share of uniform actions keeps discovering new successes.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import torch
from torch import nn


@dataclass
class EliteConfig:
    method: str = "elite"          # "elite" or "awr"
    batch_episodes: int = 1000     # shots per collection round
    updates_per_round: int = 200   # gradient steps after each round
    minibatch: int = 256
    learning_rate: float = 1e-3
    hidden: tuple[int, ...] = (256, 256)
    explore_uniform: float = 0.1   # share of uniformly random shots
    log_std_min: float = -6.0      # sigma >= exp(-6) ~ 0.0025 of the half-range
    log_std_max: float = 0.0
    buffer_size: int = 500_000
    awr_beta: float = 0.2
    awr_max_weight: float = 20.0
    value_updates_per_round: int = 200


def mlp(sizes: list[int], last_activation: nn.Module | None = None) -> nn.Sequential:
    layers: list[nn.Module] = []
    for i in range(len(sizes) - 1):
        layers.append(nn.Linear(sizes[i], sizes[i + 1]))
        if i < len(sizes) - 2:
            layers.append(nn.ReLU())
    if last_activation is not None:
        layers.append(last_activation)
    return nn.Sequential(*layers)


class GaussianPolicy(nn.Module):
    def __init__(self, obs_size: int, action_size: int, hidden: tuple[int, ...], log_std_min: float, log_std_max: float):
        super().__init__()
        self.body = mlp([obs_size, *hidden, 2 * action_size])
        self.action_size = action_size
        self.log_std_min, self.log_std_max = log_std_min, log_std_max

    def forward(self, obs: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        out = self.body(obs)
        mean = torch.tanh(out[..., : self.action_size])
        log_std = self.log_std_min + (self.log_std_max - self.log_std_min) * torch.sigmoid(out[..., self.action_size:])
        return mean, log_std

    def log_prob(self, obs: torch.Tensor, action: torch.Tensor) -> torch.Tensor:
        mean, log_std = self(obs)
        z = (action - mean) / torch.exp(log_std)
        return (-0.5 * z * z - log_std - 0.5 * np.log(2 * np.pi)).sum(-1)


class EliteAgent:
    """Collects shots, keeps them, fits the policy by weighted likelihood."""

    def __init__(self, obs_size: int, action_size: int, config: EliteConfig, seed: int):
        self.config = config
        self.rng = np.random.default_rng(seed)
        torch.manual_seed(seed)
        self.policy = GaussianPolicy(obs_size, action_size, config.hidden, config.log_std_min, config.log_std_max)
        self.policy_optimizer = torch.optim.Adam(self.policy.parameters(), lr=config.learning_rate)
        self.value = mlp([obs_size, *config.hidden, 1]) if config.method == "awr" else None
        self.value_optimizer = torch.optim.Adam(self.value.parameters(), lr=config.learning_rate) if self.value else None
        size = config.buffer_size
        self.obs = np.zeros((size, obs_size), dtype=np.float32)
        self.actions = np.zeros((size, action_size), dtype=np.float32)
        self.rewards = np.zeros(size, dtype=np.float32)
        self.success = np.zeros(size, dtype=bool)
        self.count = 0
        self.position = 0
        self.action_size = action_size

    # ---- Acting ---------------------------------------------------------------

    def act(self, obs: np.ndarray, deterministic: bool = False) -> np.ndarray:
        with torch.no_grad():
            mean, log_std = self.policy(torch.as_tensor(obs, dtype=torch.float32))
        mean, std = mean.numpy(), np.exp(log_std.numpy())
        if deterministic:
            return mean
        if self.rng.random() < self.config.explore_uniform:
            return self.rng.uniform(-1.0, 1.0, self.action_size).astype(np.float32)
        return np.clip(mean + std * self.rng.standard_normal(self.action_size), -1.0, 1.0).astype(np.float32)

    def predict(self, obs, deterministic: bool = True):
        """SB3-style interface, for evaluate()."""
        return self.act(np.asarray(obs, dtype=np.float32), deterministic), None

    # ---- Learning -------------------------------------------------------------

    def store(self, obs, action, reward: float, success: bool) -> None:
        i = self.position
        self.obs[i], self.actions[i], self.rewards[i], self.success[i] = obs, action, reward, success
        self.position = (self.position + 1) % self.config.buffer_size
        self.count = min(self.count + 1, self.config.buffer_size)

    def update(self) -> dict[str, float]:
        cfg = self.config
        stats: dict[str, float] = {}
        n = self.count
        if cfg.method == "awr":
            obs_all = torch.as_tensor(self.obs[:n])
            rewards_all = torch.as_tensor(self.rewards[:n])
            for _ in range(cfg.value_updates_per_round):
                idx = torch.as_tensor(self.rng.integers(0, n, cfg.minibatch))
                loss = ((self.value(obs_all[idx]).squeeze(-1) - rewards_all[idx]) ** 2).mean()
                self.value_optimizer.zero_grad()
                loss.backward()
                self.value_optimizer.step()
            stats["value_loss"] = loss.item()
            candidates = np.arange(n)
        else:
            candidates = np.flatnonzero(self.success[:n])
            if len(candidates) == 0:
                return {"elites": 0.0}
        stats["elites"] = float(self.success[:n].sum())

        for _ in range(cfg.updates_per_round):
            idx = candidates[self.rng.integers(0, len(candidates), cfg.minibatch)]
            obs = torch.as_tensor(self.obs[idx])
            actions = torch.as_tensor(self.actions[idx])
            log_prob = self.policy.log_prob(obs, actions)
            if cfg.method == "awr":
                with torch.no_grad():
                    advantage = torch.as_tensor(self.rewards[idx]) - self.value(obs).squeeze(-1)
                    weights = torch.clamp(torch.exp(advantage / cfg.awr_beta), max=cfg.awr_max_weight)
                loss = -(weights * log_prob).mean()
            else:
                loss = -log_prob.mean()
            self.policy_optimizer.zero_grad()
            loss.backward()
            self.policy_optimizer.step()
        stats["policy_loss"] = loss.item()
        with torch.no_grad():
            _, log_std = self.policy(torch.as_tensor(self.obs[candidates[: min(len(candidates), 1000)]]))
        stats["mean_std"] = float(torch.exp(log_std).mean())
        return stats

    def save(self, path) -> None:
        torch.save({"policy": self.policy.state_dict(), "config": self.config.__dict__,
                    "obs_size": self.obs.shape[1], "action_size": self.action_size}, path)

    @classmethod
    def load(cls, path) -> "EliteAgent":
        data = torch.load(path, weights_only=False)
        config = EliteConfig(**{**data["config"], "hidden": tuple(data["config"]["hidden"]), "buffer_size": 1})
        agent = cls(data["obs_size"], data["action_size"], config, seed=0)
        agent.policy.load_state_dict(data["policy"])
        return agent
