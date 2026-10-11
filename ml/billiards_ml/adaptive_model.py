"""Opponent-conditioned relational value network, schema version 1.

The input is encoded by C++ in both training and the shipping game. A set of
ball tokens attends to geometry and to a token containing rules and BOTH
players' execution/personality profiles. Relative distance biases attention.
Auxiliary next-pot/foul heads give the encoder less noisy shot-level targets.
"""
from __future__ import annotations

import math
import torch
from torch import nn

BALLS, BALL_FEATURES, CONTEXT = 16, 14, 32
FEATURE_COUNT = BALLS * BALL_FEATURES + CONTEXT


class RelationalBlock(nn.Module):
    def __init__(self, width: int, heads: int):
        super().__init__()
        self.heads, self.depth = heads, width // heads
        self.norm1 = nn.LayerNorm(width)
        self.qkv = nn.Linear(width, width * 3)
        self.projection = nn.Linear(width, width)
        self.norm2 = nn.LayerNorm(width)
        self.ff = nn.Sequential(nn.Linear(width, width * 2), nn.GELU(), nn.Linear(width * 2, width))

    def forward(self, tokens, bias, absent):
        batch, count, width = tokens.shape
        qkv = self.qkv(self.norm1(tokens)).reshape(batch, count, 3, self.heads, self.depth)
        q, k, v = qkv.permute(2, 0, 3, 1, 4).unbind(0)
        scores = (q @ k.transpose(-1, -2)) / math.sqrt(self.depth) + bias
        scores = scores.masked_fill(absent[:, None, None, :], -1e4)
        attended = (scores.softmax(-1) @ v).transpose(1, 2).reshape(batch, count, width)
        tokens = tokens + self.projection(attended)
        return tokens + self.ff(self.norm2(tokens))


class AdaptiveValueNet(nn.Module):
    def __init__(self, width: int = 64, layers: int = 3, heads: int = 4):
        super().__init__()
        self.config = dict(width=width, layers=layers, heads=heads)
        self.ball_in = nn.Sequential(nn.Linear(BALL_FEATURES, width), nn.GELU())
        self.context_in = nn.Sequential(nn.Linear(CONTEXT, width), nn.GELU())
        self.relation = nn.Sequential(nn.Linear(3, 16), nn.GELU(), nn.Linear(16, heads))
        self.blocks = nn.ModuleList(RelationalBlock(width, heads) for _ in range(layers))
        self.norm = nn.LayerNorm(width)
        self.head = nn.Sequential(nn.Linear(width * 3 + CONTEXT, 128), nn.GELU(), nn.Linear(128, 3))
        # Direct player-conditioning path helps the value learn skill mismatch
        # without forcing attention to reconstruct those global variables.
        self.skill_value = nn.Linear(16, 1, bias=False)

    def forward(self, features):
        balls = features[:, :BALLS * BALL_FEATURES].reshape(-1, BALLS, BALL_FEATURES)
        context = features[:, BALLS * BALL_FEATURES:]
        present = balls[:, :, 0] > 0.5
        tokens = torch.cat([self.context_in(context)[:, None], self.ball_in(balls)], 1)
        positions = torch.cat([torch.zeros_like(balls[:, :1, 1:3]), balls[:, :, 1:3]], 1)
        delta = (positions[:, :, None] - positions[:, None, :]).abs()
        distance = (delta.square().sum(-1, keepdim=True) + 1e-8).sqrt()
        relation = torch.cat([delta, distance], -1)
        bias = self.relation(relation).permute(0, 3, 1, 2)
        # The context token has no spatial location; no distance bias on its edges.
        spatial = torch.cat([torch.zeros_like(present[:, :1]), torch.ones_like(present)], 1)
        bias = bias * (spatial[:, None, :, None] & spatial[:, None, None, :]).float()
        absent = torch.cat([torch.zeros_like(present[:, :1]), ~present], 1)
        for block in self.blocks:
            tokens = block(tokens, bias, absent)
        tokens = self.norm(tokens)
        mask = present.float().unsqueeze(-1)
        pooled = (tokens[:, 1:] * mask).sum(1) / mask.sum(1).clamp(min=1)
        legal = mask * balls[:, :, 3:4]
        target = (tokens[:, 1:] * legal).sum(1) / legal.sum(1).clamp(min=1)
        logits = self.head(torch.cat([tokens[:, 0], pooled, target, context], -1))
        value = logits[:, :1] + self.skill_value(context[:, -16:])
        return torch.cat([value, logits[:, 1:]], -1)


class WinProbability(nn.Module):
    def __init__(self, net: AdaptiveValueNet):
        super().__init__()
        self.net = net

    def forward(self, features):
        return self.net(features)[:, 0].sigmoid()
