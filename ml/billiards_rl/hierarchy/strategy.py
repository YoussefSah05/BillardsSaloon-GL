"""Strategic policies π_H: choose the goal (ball, pocket) for the next shot.

- RandomStrategy:   a uniformly random valid goal (the floor).
- EasiestStrategy:  the goal of greatest ease (the classical heuristic).
- LearnedStrategy:  a candidate scorer trained by policy gradient.

The learned scorer applies the same MLP to every candidate,
$\\ell_j = f_\\theta([x_j, g])$ with $x_j$ the candidate's features and $g$ the
global ones, then a softmax over valid candidates. It is
permutation-equivariant: it scores goals, not slots. Training is REINFORCE
with a mean baseline and an entropy bonus:

$$
\\nabla_\\theta J \\approx \\frac1N \\sum_{\\text{episodes}} \\sum_t (G_t - b)\\, \\nabla_\\theta \\log \\pi_\\theta(a_t \\mid s_t),
$$

with $G_t$ the balls potted from turn $t$ to the end of the rack (minus
scratches) and $b$ the batch mean of $G_t$.
"""

from __future__ import annotations

import numpy as np
import torch
from torch import nn

from billiards_rl.hierarchy.run_env import CANDIDATE_FEATURES, GLOBAL_FEATURES, RunEnv


class RandomStrategy:
    name = "random"

    def __init__(self, seed: int = 0):
        self.rng = np.random.default_rng(seed)

    def choose(self, env: RunEnv) -> int:
        return int(self.rng.integers(0, len(env.candidates)))


class EasiestStrategy:
    name = "easiest"

    def choose(self, env: RunEnv) -> int:
        return 0   # candidates are sorted by ease


class Scorer(nn.Module):
    def __init__(self, hidden: int = 64):
        super().__init__()
        self.net = nn.Sequential(nn.Linear(CANDIDATE_FEATURES + GLOBAL_FEATURES, hidden), nn.ReLU(),
                                 nn.Linear(hidden, hidden), nn.ReLU(), nn.Linear(hidden, 1))

    def forward(self, candidates: torch.Tensor, mask: torch.Tensor, glob: torch.Tensor) -> torch.Tensor:
        k = candidates.shape[-2]
        x = torch.cat([candidates, glob.unsqueeze(-2).expand(*candidates.shape[:-1], glob.shape[-1])], dim=-1)
        logits = self.net(x).squeeze(-1)
        return logits.masked_fill(~mask, -1e9)


class LearnedStrategy:
    name = "learned"

    def __init__(self, scorer: Scorer, greedy: bool = True, seed: int = 0):
        self.scorer = scorer
        self.greedy = greedy
        self.rng = np.random.default_rng(seed)
        self.trace: list[tuple[np.ndarray, np.ndarray, np.ndarray, int]] = []

    def choose(self, env: RunEnv) -> int:
        feats, mask, glob = env.features()
        with torch.no_grad():
            logits = self.scorer(torch.as_tensor(feats), torch.as_tensor(mask), torch.as_tensor(glob))
        if self.greedy:
            choice = int(torch.argmax(logits))
        else:
            p = torch.softmax(logits, -1).numpy().astype(np.float64)
            choice = int(self.rng.choice(len(p), p=p / p.sum()))
        self.trace.append((feats, mask, glob, choice))
        return choice


def train_strategy(env: RunEnv, executor, iterations: int, episodes_per_iteration: int = 64, lr: float = 3e-4,
                   entropy: float = 0.01, seed: int = 0, log=None, scorer: Scorer | None = None,
                   optimizer: torch.optim.Optimizer | None = None) -> tuple[Scorer, torch.optim.Optimizer]:
    """Trains (or continues training) a scorer; returns it with its optimizer."""
    torch.manual_seed(seed)
    scorer = scorer if scorer is not None else Scorer()
    optimizer = optimizer if optimizer is not None else torch.optim.Adam(scorer.parameters(), lr=lr)
    strategy = LearnedStrategy(scorer, greedy=False, seed=seed)
    rng = np.random.default_rng(seed)
    for iteration in range(iterations):
        steps, returns = [], []
        for _ in range(episodes_per_iteration):
            strategy.trace = []
            env.reset(int(rng.integers(0, 2**31)))
            rewards = []
            while True:
                choice = strategy.choose(env)
                reward, done, _ = env.play(env.candidates[choice], executor)
                rewards.append(reward)
                if done:
                    break
            to_go = np.cumsum(rewards[::-1])[::-1]
            steps.extend(strategy.trace)
            returns.extend(to_go.tolist())
            strategy.trace = []
        feats = torch.as_tensor(np.stack([s[0] for s in steps]))
        mask = torch.as_tensor(np.stack([s[1] for s in steps]))
        glob = torch.as_tensor(np.stack([s[2] for s in steps]))
        actions = torch.as_tensor([s[3] for s in steps])
        g = torch.as_tensor(returns, dtype=torch.float32)
        advantage = g - g.mean()
        logits = scorer(feats, mask, glob)
        log_probs = torch.log_softmax(logits, -1)
        chosen = log_probs.gather(-1, actions.unsqueeze(-1)).squeeze(-1)
        probs = torch.softmax(logits, -1)
        ent = -(probs * torch.where(mask, log_probs, torch.zeros_like(log_probs))).sum(-1).mean()
        loss = -(advantage * chosen).mean() - entropy * ent
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        if log is not None:
            log(iteration, float(g.mean()), float(ent))
    return scorer, optimizer
