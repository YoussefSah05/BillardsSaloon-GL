"""The value network: P(the player to move wins the frame | table)."""

from __future__ import annotations

import torch
from torch import nn

BALL_SLOTS = 16
BALL_FEATURES = 8
CONTEXT_FEATURES = 16
FEATURE_COUNT = BALL_SLOTS * BALL_FEATURES + CONTEXT_FEATURES


class ValueNet(nn.Module):
    """A small transformer over the set of balls on the table.

    Each ball is a token (its features plus a learned embedding of its slot, so
    the 1 and the 9 differ); the rules context is one more token, which is
    always present and whose output becomes the value. Balls off the table are
    masked out. Attention lets the network see interactions: blockers,
    clusters, the cue ball's relation to the next ball.
    """

    def __init__(self, width: int = 64, layers: int = 2, heads: int = 4) -> None:
        super().__init__()
        self.ball_in = nn.Linear(BALL_FEATURES, width)
        self.slot = nn.Embedding(BALL_SLOTS, width)
        self.context_in = nn.Linear(CONTEXT_FEATURES, width)
        layer = nn.TransformerEncoderLayer(width, heads, dim_feedforward=2 * width, dropout=0.0,
                                           batch_first=True, norm_first=True)
        self.encoder = nn.TransformerEncoder(layer, layers, enable_nested_tensor=False)
        self.head = nn.Sequential(nn.LayerNorm(width), nn.Linear(width, width), nn.GELU(), nn.Linear(width, 1))

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """x: [batch, FEATURE_COUNT] -> logits [batch]."""
        batch = x.shape[0]
        balls = x[:, : BALL_SLOTS * BALL_FEATURES].reshape(batch, BALL_SLOTS, BALL_FEATURES)
        context = x[:, BALL_SLOTS * BALL_FEATURES:]
        present = balls[:, :, 0] > 0.5

        slots = torch.arange(BALL_SLOTS, device=x.device)
        tokens = self.ball_in(balls) + self.slot(slots)[None]
        tokens = torch.cat([self.context_in(context)[:, None], tokens], dim=1)

        # Padding mask: True = ignore. The context token is never ignored.
        ignore = torch.cat([torch.zeros(batch, 1, dtype=torch.bool, device=x.device), ~present], dim=1)
        encoded = self.encoder(tokens, src_key_padding_mask=ignore)
        return self.head(encoded[:, 0]).squeeze(-1)


class ValueProbability(nn.Module):
    """The exported form: features in, win probability out."""

    def __init__(self, net: ValueNet) -> None:
        super().__init__()
        self.net = net

    def forward(self, features: torch.Tensor) -> torch.Tensor:
        return torch.sigmoid(self.net(features))
