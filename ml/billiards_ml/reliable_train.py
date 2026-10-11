"""Train/calibrate a graph Transformer ensemble without touching the shipped AI.

Run from ml: python -m billiards_ml.reliable_train --epochs 12 --threads 4
Validation checkpoint selection and temperature fitting use disjoint FRAMES.
The frozen test partition is evaluated only after all choices are fixed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
from torch import nn

from .adaptive_train import ROOT, DISCIPLINES, LADDER, metrics, persistent_split, provenance
from .reliable_model import ReliableValueNet, CalibratedEnsemble

OUTPUT = Path("runs/reliable")


def calibration_error(p, y, bins=15):
    """Equal-width probability bins; diagnostic, not a calibration guarantee."""
    index = np.minimum((p * bins).astype(int), bins - 1)
    return float(sum(np.mean(index == i) * abs(p[index == i].mean() - y[index == i].mean())
                     for i in range(bins) if (index == i).any()))


def measure(p, y):
    return dict(**metrics(p, y), ece_15=calibration_error(p, y))


def calibration_partition(frames, validation, seed=712):
    ids = np.unique(frames[validation])
    if len(ids) < 4:
        raise ValueError("Need independent selection and calibration frames")
    ids = np.random.default_rng(seed).permutation(ids)
    calibration = validation & np.isin(frames, ids[:len(ids)//2])
    return validation & ~calibration, calibration


def frame_bootstrap_weights(frames, train, seed):
    ids = np.unique(frames[train])
    rng = np.random.default_rng(seed)
    counts = np.bincount(rng.integers(0, len(ids), len(ids)), minlength=len(ids))
    weights = np.zeros(len(frames), dtype=np.float32)
    weights[train] = counts[np.searchsorted(ids, frames[train])]
    return weights


def fit_temperature(probabilities, labels):
    # Fit one parameter on calibration frames only; bounded grid is stable,
    # deterministic and cannot accidentally create a negative temperature.
    p = torch.as_tensor(probabilities, dtype=torch.float64).clamp(1e-6, 1 - 1e-6)
    y = torch.as_tensor(labels, dtype=torch.float64)
    logit = torch.logit(p)
    grid = torch.logspace(np.log10(.25), np.log10(4), 401, dtype=torch.float64)
    losses = nn.functional.binary_cross_entropy_with_logits(logit[None] / grid[:, None],
                                                            y[None].expand(len(grid), -1), reduction="none").mean(1)
    return float(grid[losses.argmin()])


def predict(net, x):
    net.eval()
    with torch.no_grad():
        return torch.cat([net(batch)[:, 0].sigmoid() for batch in x.split(1024)]).numpy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--epochs", type=int, default=12)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--batch", type=int, default=512)
    args = parser.parse_args()
    if min(args.epochs, args.threads, args.batch) < 1:
        parser.error("counts must be positive")
    torch.set_num_threads(args.threads)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    parts = [dict(np.load(ROOT / f"league-g{g}.npz")) for g in range(2)]
    data = {k: np.concatenate([p[k] for p in parts]) for k in parts[0]}
    train, validation, test = persistent_split(data["frame"], ROOT / "splits.json", 42)
    selection, calibration = calibration_partition(data["frame"], validation)
    x, y = torch.from_numpy(data["x"]), torch.from_numpy(data["y"])
    aux, acting = torch.from_numpy(data["aux"]), torch.from_numpy(data["mask"])
    members, histories, predictions = [], [], []
    started = time.perf_counter()
    seeds = (142, 314, 2718)
    for member, seed in enumerate(seeds):
        torch.manual_seed(seed)
        net = ReliableValueNet()
        weight = torch.from_numpy(frame_bootstrap_weights(data["frame"], train, seed))
        rows = torch.from_numpy(np.flatnonzero(train & (weight.numpy() > 0)))
        optimizer = torch.optim.AdamW(net.parameters(), lr=3e-4, weight_decay=.003)
        scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, args.epochs, eta_min=1.5e-5)
        best, stale, history = float("inf"), 0, []
        checkpoint = OUTPUT / f"member-{member}.pt"
        for epoch in range(args.epochs):
            net.train()
            losses = []
            for batch in rows[torch.randperm(len(rows))].split(args.batch):
                logits = net(x[batch])
                value = nn.functional.binary_cross_entropy_with_logits(logits[:, 0], y[batch], reduction="none")
                auxiliary = nn.functional.binary_cross_entropy_with_logits(logits[:, 1:], aux[batch], reduction="none")
                w = weight[batch]
                value_loss = (value * w).sum() / w.sum().clamp(min=1)
                shot_weight = w * acting[batch]
                shot_loss = (auxiliary * shot_weight[:, None]).sum(0) / shot_weight.sum().clamp(min=1)
                loss = value_loss + .3 * shot_loss[0] + .15 * shot_loss[1]
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                nn.utils.clip_grad_norm_(net.parameters(), 1.)
                optimizer.step()
                losses.append(float(loss.detach()))
            scheduler.step()
            p = predict(net, x[selection])
            result = dict(member=member, seed=seed, epoch=epoch + 1, loss=float(np.mean(losses)),
                          selection=measure(p, data["y"][selection]), seconds=time.perf_counter() - started)
            history.append(result)
            print(json.dumps(result), flush=True)
            if result["selection"]["log_loss"] < best - 1e-4:
                best, stale = result["selection"]["log_loss"], 0
                temporary = checkpoint.with_suffix(".tmp")
                torch.save(dict(state=net.state_dict(), config=net.config, schema=2, seed=seed, epoch=epoch + 1), temporary)
                temporary.replace(checkpoint)
            else:
                stale += 1
            if stale >= 4:
                break
        net.load_state_dict(torch.load(checkpoint, weights_only=True)["state"])
        members.append(net.eval())
        histories.append(history)
        predictions.append(predict(net, x))
    mean = np.mean(predictions, axis=0)
    temperature = fit_temperature(mean[calibration], data["y"][calibration])
    ensemble = CalibratedEnsemble(members, temperature).eval()
    with torch.no_grad():
        outputs = [ensemble(batch) for batch in x.split(1024)]
        p = torch.cat([out[0] for out in outputs]).numpy()
        disagreement = torch.cat([out[1] for out in outputs]).numpy()
    path = OUTPUT / "value.onnx"
    torch.onnx.export(ensemble, (x[:8],), str(path), input_names=["features"],
                      output_names=["win_probability", "model_disagreement"],
                      dynamic_axes={"features": {0: "batch"}, "win_probability": {0: "batch"},
                                    "model_disagreement": {0: "batch"}}, opset_version=17, dynamo=False)
    from .adaptive_train import OnnxValue
    runtime = OnnxValue(path)
    probe = data["x"][test][:128]
    actual = runtime.session.run(None, {"features": probe})
    parity = max(float(np.abs(actual[0] - p[test][:128]).max()),
                 float(np.abs(actual[1] - disagreement[test][:128]).max()))
    for _ in range(20): runtime(probe[0])
    tick = time.perf_counter()
    for _ in range(500): runtime(probe[0])
    latency = (time.perf_counter() - tick) * 2
    old = OnnxValue("../assets/models/adaptive_value.onnx")
    old_p = np.concatenate([old.session.run(["win_probability"], {"features": batch})[0]
                            for batch in np.array_split(data["x"][test], 20)])
    # Cluster bootstrap: positions within one frame are highly correlated.
    frames = data["frame"][test]
    labels = data["y"][test]
    difference = []
    for frame in np.unique(frames):
        mask = frames == frame
        difference.append(float((((p[test][mask] > .5) == labels[mask]) -
                                 ((old_p[mask] > .5) == labels[mask]).astype(float)).mean()))
    rng = np.random.default_rng(911)
    ci = np.quantile(rng.choice(difference, (5000, len(difference)), replace=True).mean(1), [.025, .975])
    low, high = torch.from_numpy(probe.copy()), torch.from_numpy(probe.copy())
    low[:, -8:-5], high[:, -8:-5] = torch.tensor([1.1/1.2, .14/.16, .18/.2]), torch.tensor([.16/1.2, .04/.16, .05/.2])
    with torch.no_grad():
        skill_delta = float((ensemble(low)[0] - ensemble(high)[0]).mean())
    stratified = {}
    for d, name in enumerate(DISCIPLINES):
        for level in range(5):
            selected = test & (data["strata"][:, 0] == d) & (data["strata"][:, 2] == level)
            if selected.any():
                stratified[f"{name}/{LADDER[level]}"] = dict(positions=int(selected.sum()),
                                                          **measure(p[selected], data["y"][selected]))
    report = dict(schema=2, name="Calibrated gated graph Transformer ensemble", members=len(members), seeds=seeds,
                  architecture=dict(**members[0].config, normalization="RMSNorm", feed_forward="SwiGLU",
                                    pooling="context-conditioned attention", relation_bias=True),
                  parameters=sum(parameter.numel() for net in members for parameter in net.parameters()),
                  temperature=temperature, features=256, test=measure(p[test], labels),
                  previous_model=measure(old_p, labels), previous_model_sha256=hashlib.sha256(Path("../assets/models/adaptive_value.onnx").read_bytes()).hexdigest(),
                  selection=measure(p[selection], data["y"][selection]), calibration=measure(p[calibration], data["y"][calibration]),
                  uncalibrated_test=measure(mean[test], labels), accuracy_frame_bootstrap_95=ci.tolist(),
                  frame_mean_accuracy_change=float(np.mean(difference)), stratified=stratified,
                  train_frames=len(np.unique(data["frame"][train])), selection_frames=len(np.unique(data["frame"][selection])),
                  calibration_frames=len(np.unique(data["frame"][calibration])), test_frames=len(np.unique(data["frame"][test])),
                  train_positions=int(train.sum()), disagreement_mean=float(disagreement[test].mean()),
                  weaker_opponent_value_delta=skill_delta, onnx_max_difference=parity, inference_ms=latency,
                  model_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), histories=histories,
                  provenance=provenance(1, dict(epochs=args.epochs, batch=args.batch, threads=args.threads,
                          optimizer="AdamW", lr=.0003, weight_decay=.003, gradient_clip=1., bootstrap_unit="frame",
                          targets="Monte Carlo frame winners", auxiliary_weights=[.3, .15], onnx_opset=17)),
                  limitations=["Model disagreement is not an error bound or certified OOD detection.",
                               "Prediction accuracy is about eventual frame winners, not pot success.",
                               "This existing test set was reported for the prior model; new match seeds are required before release."])
    project = Path(__file__).resolve().parents[2]
    for source in ("ml/billiards_ml/reliable_model.py", "ml/billiards_ml/reliable_train.py"):
        report["provenance"]["source_sha256"][source] = hashlib.sha256((project / source).read_bytes()).hexdigest()
    report["provenance"]["split_sha256"] = hashlib.sha256((ROOT / "splits.json").read_bytes()).hexdigest()
    report["provenance"]["selection_frame_ids"] = np.unique(data["frame"][selection]).tolist()
    report["provenance"]["calibration_frame_ids"] = np.unique(data["frame"][calibration]).tolist()
    (OUTPUT / "training.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k:v for k,v in report.items() if k not in ("histories", "stratified", "provenance")}, indent=2), flush=True)
    if parity > 1e-5 or latency >= 1:
        raise RuntimeError("Export parity or CPU budget failed; do not install this model")


if __name__ == "__main__":
    main()
