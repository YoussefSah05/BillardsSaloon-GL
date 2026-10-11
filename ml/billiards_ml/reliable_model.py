"""Geometry-aware, gated graph Transformer ensemble (feature schema unchanged).

Independent frame-bootstrap members estimate disagreement; this is a diagnostic,
not a guaranteed error bound or an out-of-distribution detector. Calibration is
fitted on separate frames and embedded in the exported ONNX graph.
"""
from __future__ import annotations

import math
import torch
from torch import nn

from .adaptive_model import BALLS, BALL_FEATURES, CONTEXT


class RMSNorm(nn.Module):
    def __init__(self, width):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(width))

    def forward(self, x):
        return x * torch.rsqrt(x.square().mean(-1, keepdim=True) + 1e-6) * self.weight


class GatedGraphBlock(nn.Module):
    def __init__(self, width, heads):
        super().__init__()
        self.heads, self.depth = heads, width // heads
        self.norm1, self.norm2 = RMSNorm(width), RMSNorm(width)
        self.qkv = nn.Linear(width, 3 * width, bias=False)
        self.projection = nn.Linear(width, width, bias=False)
        self.gate_up = nn.Linear(width, 4 * width, bias=False)
        self.down = nn.Linear(2 * width, width, bias=False)

    def forward(self, tokens, bias, absent):
        batch, count, width = tokens.shape
        qkv = self.qkv(self.norm1(tokens)).reshape(batch, count, 3, self.heads, self.depth)
        q, k, v = qkv.permute(2, 0, 3, 1, 4).unbind(0)
        scores = (q @ k.transpose(-1, -2)) / math.sqrt(self.depth) + bias
        scores = scores.masked_fill(absent[:, None, None, :], -1e4)
        attended = (scores.softmax(-1) @ v).transpose(1, 2).reshape(batch, count, width)
        tokens = tokens + self.projection(attended)
        gate, up = self.gate_up(self.norm2(tokens)).chunk(2, -1)
        return tokens + self.down(nn.functional.silu(gate) * up)


class ReliableValueNet(nn.Module):
    def __init__(self, width=64, layers=2, heads=4):
        super().__init__()
        if width % heads or layers < 1:
            raise ValueError("width must divide into heads and layers must be positive")
        self.config = dict(width=width, layers=layers, heads=heads)
        self.pool_scale = math.sqrt(width)
        self.ball_in = nn.Linear(BALL_FEATURES, width)
        self.context_in = nn.Linear(CONTEXT, width)
        self.relation = nn.Sequential(nn.Linear(3, 16), nn.SiLU(), nn.Linear(16, heads))
        self.blocks = nn.ModuleList(GatedGraphBlock(width, heads) for _ in range(layers))
        self.norm = RMSNorm(width)
        # Context-conditioned learned pooling can focus on useful blockers and
        # targets, rather than treating every present ball equally.
        self.pool_query = nn.Linear(width, width, bias=False)
        self.pool_key = nn.Linear(width, width, bias=False)
        self.head = nn.Sequential(nn.Linear(width * 3 + CONTEXT, 128), nn.SiLU(), nn.Linear(128, 3))
        self.skill_value = nn.Linear(16, 1, bias=False)

    def forward(self, features):
        balls = features[:, :BALLS * BALL_FEATURES].reshape(-1, BALLS, BALL_FEATURES)
        context = features[:, BALLS * BALL_FEATURES:]
        present = balls[:, :, 0] > .5
        # Zero absent features before any arithmetic, not just at readout.
        balls = torch.where(present[:, :, None], balls, torch.zeros_like(balls))
        tokens = torch.cat([self.context_in(context)[:, None], self.ball_in(balls)], 1)
        delta = (balls[:, :, None, 1:3] - balls[:, None, :, 1:3]).abs()
        separation = (delta.square().sum(-1, keepdim=True) + 1e-8).sqrt()
        spatial_bias = self.relation(torch.cat([delta, separation], -1)).permute(0, 3, 1, 2)
        # Context has no physical coordinates. Pad both context edge directions.
        bias = nn.functional.pad(spatial_bias, (1, 0, 1, 0))
        absent = torch.cat([torch.zeros_like(present[:, :1]), ~present], 1)
        for block in self.blocks:
            tokens = block(tokens, bias, absent)
        tokens = self.norm(tokens)
        query = self.pool_query(tokens[:, :1])
        weights = (query * self.pool_key(tokens)).sum(-1) / self.pool_scale
        weights = weights.masked_fill(absent, -1e4).softmax(-1)
        pooled = (tokens * weights[:, :, None]).sum(1)
        legal = present.float()[:, :, None] * balls[:, :, 3:4]
        target = (tokens[:, 1:] * legal).sum(1) / legal.sum(1).clamp(min=1)
        logits = self.head(torch.cat([tokens[:, 0], pooled, target, context], -1))
        return torch.cat([logits[:, :1] + self.skill_value(context[:, -16:]), logits[:, 1:]], -1)


class CalibratedEnsemble(nn.Module):
    def __init__(self, members, temperature=1.):
        super().__init__()
        if len(members) < 2 or not .25 <= temperature <= 4:
            raise ValueError("need at least two members and a bounded positive temperature")
        self.members = nn.ModuleList(members)
        self.register_buffer("temperature", torch.tensor(float(temperature)))

    def forward(self, features):
        probabilities = torch.stack([net(features)[:, 0].sigmoid() for net in self.members])
        mean = probabilities.mean(0).clamp(1e-6, 1 - 1e-6)
        calibrated = ((mean.log() - torch.log1p(-mean)) / self.temperature).sigmoid()
        disagreement = ((probabilities - mean[None]).square().mean(0)).sqrt()
        return calibrated, disagreement
