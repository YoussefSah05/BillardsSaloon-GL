"""Train the value network on self-play data, then export it to ONNX.

    uv run python -m billiards_ml.train_value --data data/selfplay.npz

Frames are split 90/10 by frame (never by position), so validation measures
generalisation to unseen frames. Reports log loss, accuracy and Brier score
against two baselines (the base rate, and logistic regression on the same
features), writes a checkpoint, an ONNX model checked against PyTorch, and a
metrics file for the model card.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import time

import numpy as np
import torch
from torch import nn

from billiards_ml.model import FEATURE_COUNT, ValueNet, ValueProbability


def metrics(p: np.ndarray, y: np.ndarray) -> dict[str, float]:
    p = np.clip(p, 1e-6, 1 - 1e-6)
    return {
        "log_loss": float(-np.mean(y * np.log(p) + (1 - y) * np.log(1 - p))),
        "accuracy": float(np.mean((p > 0.5) == (y > 0.5))),
        "brier": float(np.mean((p - y) ** 2)),
    }


def logistic_baseline(x_train, y_train, x_val, epochs: int = 30) -> np.ndarray:
    model = nn.Linear(FEATURE_COUNT, 1)
    optimiser = torch.optim.LBFGS(model.parameters(), max_iter=epochs)
    xt, yt = torch.from_numpy(x_train), torch.from_numpy(y_train)

    def closure():
        optimiser.zero_grad()
        loss = nn.functional.binary_cross_entropy_with_logits(model(xt).squeeze(-1), yt) + 1e-4 * model.weight.pow(2).sum()
        loss.backward()
        return loss

    optimiser.step(closure)
    with torch.no_grad():
        return torch.sigmoid(model(torch.from_numpy(x_val)).squeeze(-1)).numpy()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default="data/selfplay.npz")
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch", type=int, default=512)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--out", default="checkpoints/value")
    parser.add_argument("--onnx", default="../assets/models/value.onnx")
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    data = np.load(args.data)
    x, y, frames = data["x"], data["y"], data["frame"]

    unique = np.unique(frames)
    val_frames = set(rng.choice(unique, size=max(1, len(unique) // 10), replace=False).tolist())
    is_val = np.array([f in val_frames for f in frames])
    x_train, y_train, x_val, y_val = x[~is_val], y[~is_val], x[is_val], y[is_val]
    print(f"{len(y_train)} training positions, {len(y_val)} validation ({len(val_frames)} frames)")

    device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
    net = ValueNet().to(device)
    optimiser = torch.optim.AdamW(net.parameters(), lr=args.lr, weight_decay=1e-4)
    steps = args.epochs * math.ceil(len(y_train) / args.batch)
    schedule = torch.optim.lr_scheduler.OneCycleLR(optimiser, max_lr=args.lr, total_steps=steps)
    print(f"{sum(p.numel() for p in net.parameters())} parameters on {device}")

    xt = torch.from_numpy(x_train).to(device)
    yt = torch.from_numpy(y_train).to(device)
    xv = torch.from_numpy(x_val).to(device)

    def predict(xs: torch.Tensor) -> np.ndarray:
        net.eval()
        out = []
        with torch.no_grad():
            for i in range(0, len(xs), 4096):
                out.append(torch.sigmoid(net(xs[i:i + 4096])).cpu().numpy())
        net.train()
        return np.concatenate(out)

    best = math.inf
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    start = time.time()
    for epoch in range(args.epochs):
        order = torch.randperm(len(yt), device=device)
        total = 0.0
        for i in range(0, len(order), args.batch):
            index = order[i:i + args.batch]
            loss = nn.functional.binary_cross_entropy_with_logits(net(xt[index]), yt[index])
            optimiser.zero_grad()
            loss.backward()
            optimiser.step()
            schedule.step()
            total += loss.item() * len(index)
        val = metrics(predict(xv), y_val)
        print(f"epoch {epoch + 1:2d}  train {total / len(yt):.4f}  val {val['log_loss']:.4f}  "
              f"acc {val['accuracy']:.3f}  brier {val['brier']:.4f}  {time.time() - start:5.0f} s", flush=True)
        if val["log_loss"] < best:
            best = val["log_loss"]
            torch.save(net.state_dict(), args.out + ".pt")

    net.load_state_dict(torch.load(args.out + ".pt", map_location=device))
    final = metrics(predict(xv), y_val)
    base = metrics(np.full_like(y_val, y_train.mean()), y_val)
    logistic = metrics(logistic_baseline(x_train, y_train, x_val), y_val)
    print("validation   ", json.dumps(final))
    print("base rate    ", json.dumps(base))
    print("logistic reg.", json.dumps(logistic))

    # Export: features [batch, FEATURE_COUNT] -> win probability [batch].
    exported = ValueProbability(net.cpu().eval())
    os.makedirs(os.path.dirname(args.onnx), exist_ok=True)
    example = torch.from_numpy(x_val[:8])
    torch.onnx.export(exported, (example,), args.onnx, input_names=["features"], output_names=["win_probability"],
                      dynamic_axes={"features": {0: "batch"}, "win_probability": {0: "batch"}}, opset_version=17,
                      dynamo=False)

    import onnxruntime as ort
    session = ort.InferenceSession(args.onnx, providers=["CPUExecutionProvider"])
    check = x_val[:256]
    ours = exported(torch.from_numpy(check)).detach().numpy()
    theirs = session.run(None, {"features": check})[0]
    difference = float(np.max(np.abs(ours - theirs)))
    print(f"wrote {args.onnx}; ONNX Runtime matches PyTorch to {difference:.2e}")

    with open(args.out + ".metrics.json", "w") as f:
        json.dump({"validation": final, "base_rate": base, "logistic_regression": logistic,
                   "positions": {"train": int(len(y_train)), "validation": int(len(y_val))},
                   "frames_validation": len(val_frames), "onnx_max_difference": difference,
                   "parameters": sum(p.numel() for p in net.parameters()), "epochs": args.epochs}, f, indent=2)


if __name__ == "__main__":
    main()
