"""Reproducible opponent-conditioned league / expert-iteration training.

python -m billiards_ml.adaptive_train generate --frames 2400 --workers 6
python -m billiards_ml.adaptive_train train --epochs 25
python -m billiards_ml.adaptive_train generate --frames 1200 --generation 1 --model runs/adaptive/value.onnx
python -m billiards_ml.adaptive_train train --epochs 25 --generation 1
python -m billiards_ml.adaptive_train evaluate --racks 40 --weight .25

Generation zero uses purposeful search opponents, never random actions. Later
generations mix learned search and classical search to avoid population collapse.
Training/validation/test and head-to-head racks have disjoint frame seeds.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import multiprocessing as mp
import os
from pathlib import Path
import time
import shutil
import importlib.metadata

import numpy as np

LADDER = ("sam_whitlock", "dani_reyes", "iris_lindqvist", "marta_okafor", "viktor_hale")
DISCIPLINES = ("EIGHT_BALL", "NINE_BALL", "TEN_BALL")
ROOT = Path("runs/adaptive")


def provenance(generation, config):
    project = Path(__file__).resolve().parents[2]
    sources = ("ml/billiards_ml/adaptive_model.py", "src/ai/features.cpp", "src/ai/planner.cpp",
               "src/sim/roots.cpp", "src/sim/simulate.cpp")
    datasets = []
    for g in range(generation+1):
        path = ROOT / f"league-g{g}.npz"
        meta = json.loads(path.with_suffix(".json").read_text())
        datasets.append(dict(generation=g, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                             **{k:meta[k] for k in ("frames", "completed", "positions", "seed", "model_sha256", "weight")}))
    return dict(config=config, datasets=datasets,
                frameworks={name: importlib.metadata.version(name) for name in ("torch", "numpy", "onnx", "onnxruntime")},
                source_sha256={path: hashlib.sha256((project/path).read_bytes()).hexdigest() for path in sources})


class OnnxValue:
    def __init__(self, path):
        import onnxruntime as ort
        ort.disable_telemetry_events()
        options = ort.SessionOptions()
        options.intra_op_num_threads = 1
        options.inter_op_num_threads = 1
        self.session = ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"])

    def __call__(self, features):
        return float(self.session.run(["win_probability"], {"features": np.asarray(features, dtype=np.float32)[None]})[0][0])


def make_profile(bs, level: int, rng, jitter: bool = True):
    profile = bs.profile(LADDER[level])
    if jitter:
        profile.aim_noise *= float(rng.uniform(.85, 1.15))
        profile.power_noise *= float(rng.uniform(.9, 1.1))
        profile.spin_noise *= float(rng.uniform(.9, 1.1))
    return profile


def generate_chunk(task):
    import billiards_sim as bs
    start, count, generation, model, weight, seed = task
    rng = np.random.default_rng(seed + start)
    value = OnnxValue(model) if model else None
    xs, ys, aux, masks, frames, players, steps, strata = [], [], [], [], [], [], [], []
    completed, shots = 0, 0
    for index in range(start, start + count):
        # Every 75 frames covers all 25 skill pairings in all 3 disciplines.
        pair = (index // 3) % 25
        levels = (pair // 5, pair % 5)
        discipline = index % 3
        frame = seed * 1_000_000 + generation * 100_000 + index
        rack = (frame * 7919) % (2**32)
        env = bs.Environment(getattr(bs.Discipline, DISCIPLINES[discipline]), 1)
        env.reset(rack, index % 2)
        profiles = [make_profile(bs, level, rng) for level in levels]
        # Half classical, half learned for each seat in expert iteration.
        values = [value if value and rng.random() < .5 else None for _ in range(2)]
        for profile in profiles:
            profile.value_weight = weight
        begin = len(xs)
        for move in range(250):
            if env.decision() == bs.Decision.FRAME_OVER:
                break
            player = env.to_act()
            record = env.decision() in (bs.Decision.SHOT, bs.Decision.PLACE)
            if record:
                for p in (0, 1):
                    xs.append(env.adaptive_features(p, profiles[p], profiles[1-p]))
                    players.append(p)
                    frames.append(frame)
                    steps.append(move)
                    strata.append((discipline, levels[p], levels[1-p]))
            result = env.play_profile(profiles[player], profiles[1-player], (rack + move) % 2**32, values[player])
            if record:
                for p in (0, 1):
                    ys.append(-1.)
                    aux.append((float(bool(result["potted"]) and result["foul"] == 0), float(result["foul"] != 0)))
                    masks.append(float(p == player))
            shots += 1
        winner = env.winner()
        if winner < 0:
            for array in (xs, ys, aux, masks, frames, players, steps, strata):
                del array[begin:]
            continue
        completed += 1
        for i in range(begin, len(xs)):
            ys[i] = float(players[i] == winner)
    return dict(x=np.asarray(xs, dtype=np.float32).reshape(-1, bs.ADAPTIVE_FEATURE_COUNT),
                y=np.asarray(ys, dtype=np.float32), aux=np.asarray(aux, dtype=np.float32).reshape(-1, 2),
                mask=np.asarray(masks, dtype=np.float32), frame=np.asarray(frames, dtype=np.int64),
                player=np.asarray(players, dtype=np.int8), step=np.asarray(steps, dtype=np.int16),
                strata=np.asarray(strata, dtype=np.int8).reshape(-1, 3), completed=completed, shots=shots)


def generate(args):
    ROOT.mkdir(parents=True, exist_ok=True)
    model = str(Path(args.model).resolve()) if args.model else None
    tasks = [(start, min(25, args.frames-start), args.generation, model, args.weight, args.seed)
             for start in range(0, args.frames, 25)]
    start, parts = time.perf_counter(), []
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for part in pool.imap_unordered(generate_chunk, tasks):
            parts.append(part)
            print(f"{sum(p['completed'] for p in parts)}/{args.frames} completed frames, "
                  f"{sum(len(p['y']) for p in parts)} positions, {time.perf_counter()-start:.1f}s", flush=True)
    data = {key: np.concatenate([p[key] for p in parts]) for key in parts[0] if key not in ("completed", "shots")}
    # Canonical ordering makes replay files and splits independent of worker completion order.
    order = np.lexsort((data["player"], data["step"], data["frame"]))
    data = {k: v[order] for k, v in data.items()}
    path = ROOT / f"league-g{args.generation}.npz"
    np.savez_compressed(path, **data)
    metadata = dict(frames=args.frames, completed=sum(p["completed"] for p in parts), positions=len(data["y"]),
                    seconds=time.perf_counter()-start, seed=args.seed, generation=args.generation,
                    model=model, model_sha256=hashlib.sha256(Path(model).read_bytes()).hexdigest() if model else None,
                    weight=args.weight, schema=1, skills=LADDER, disciplines=DISCIPLINES)
    path.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")


def split_frames(frames, seed=42):
    ids = np.unique(frames)
    if len(ids) < 10:
        raise ValueError("Need at least 10 independent frames for train/validation/test")
    ids = np.random.default_rng(seed).permutation(ids)
    n = max(1, len(ids)//10)
    return (~np.isin(frames, ids[:2*n]), np.isin(frames, ids[:n]), np.isin(frames, ids[n:2*n]))


def persistent_split(frames, path, seed):
    """Keep previous generations' train/validation/test memberships frozen.

    Otherwise a warm-started network would already have trained on positions
    that a new random split calls test data.
    """
    saved = json.loads(path.read_text()) if path.exists() else dict(seed=seed, roles={})
    if saved["seed"] != seed:
        raise ValueError("Use the original split seed, or a separate run directory")
    roles = saved["roles"]
    new = np.asarray([frame for frame in np.unique(frames) if str(int(frame)) not in roles])
    if len(new):
        partitions = split_frames(new, seed)
        for role, mask in zip(("train", "validation", "test"), partitions):
            roles.update({str(int(frame)): role for frame in new[mask]})
        path.write_text(json.dumps(saved, indent=2)+"\n")
    return tuple(np.asarray([roles[str(int(frame))] == role for frame in frames]) for role in ("train", "validation", "test"))


def td_lambda_targets(data, prediction, lam=.7):
    # Predictions/targets always use a fixed player's perspective: no sign
    # flip when that player loses the turn. Never cross frame or seat boundaries.
    target = data["y"].copy()
    for player in (0, 1):
        selected = np.flatnonzero(data["player"] == player)
        for frame in np.unique(data["frame"][selected]):
            rows = selected[data["frame"][selected] == frame]
            rows = rows[np.argsort(data["step"][rows])]
            future = float(data["y"][rows[-1]])
            for j in range(len(rows)-1, -1, -1):
                target[rows[j]] = future
                future = (1-lam) * prediction[rows[j]] + lam * future
    return target


def metrics(p, y):
    p = np.clip(p, 1e-6, 1-1e-6)
    return dict(log_loss=float(-(y*np.log(p)+(1-y)*np.log(1-p)).mean()),
                brier=float(np.square(p-y).mean()), accuracy=float(((p>.5)==(y>.5)).mean()))


def linear_reference(data, train_mask, val_mask, test_mask):
    import torch
    from torch import nn
    with torch.random.fork_rng(devices=[]):
        torch.manual_seed(91)
        net = nn.Linear(data["x"].shape[1], 1)
    x, y = torch.from_numpy(data["x"][train_mask]), torch.from_numpy(data["y"][train_mask])
    opt = torch.optim.LBFGS(net.parameters(), max_iter=120, line_search_fn="strong_wolfe")
    def closure():
        opt.zero_grad()
        loss = nn.functional.binary_cross_entropy_with_logits(net(x).squeeze(-1), y) + 1e-4*net.weight.square().sum()
        loss.backward()
        return loss
    opt.step(closure)
    with torch.no_grad():
        p = net(torch.from_numpy(data["x"])).squeeze(-1).sigmoid().numpy()
    return dict(test=metrics(p[test_mask], data["y"][test_mask]), validation=metrics(p[val_mask], data["y"][val_mask]),
                parameters=data["x"].shape[1]+1, regularization=1e-4)


def train(args):
    import torch
    from torch import nn
    from billiards_ml.adaptive_model import AdaptiveValueNet, WinProbability, FEATURE_COUNT
    torch.set_num_threads(args.threads)
    torch.manual_seed(args.seed)
    parts = [dict(np.load(ROOT / f"league-g{g}.npz")) for g in range(args.generation+1)]
    data = {k: np.concatenate([p[k] for p in parts]) for k in parts[0]}
    train_mask, val_mask, test_mask = persistent_split(data["frame"], ROOT / "splits.json", args.seed)
    net = AdaptiveValueNet()
    checkpoint = ROOT / "value.pt"
    if args.generation > 0:
        if not checkpoint.exists():
            raise ValueError("Expert iteration requires the previous generation checkpoint")
        net.load_state_dict(torch.load(checkpoint, weights_only=True)["state"])
    xt = torch.from_numpy(data["x"])
    yt = torch.from_numpy(data["y"])
    aux = torch.from_numpy(data["aux"])
    mask = torch.from_numpy(data["mask"])
    train_rows = torch.from_numpy(np.flatnonzero(train_mask))
    opt = torch.optim.AdamW(net.parameters(), lr=args.lr, weight_decay=1e-3)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.epochs, eta_min=args.lr*.05)
    history, best, stale, best_epoch = [], float("inf"), 0, 0

    def predict():
        net.eval()
        with torch.no_grad():
            return torch.cat([net(batch)[:, 0].sigmoid() for batch in xt.split(1024)]).numpy()

    if args.generation > 0:
        best = metrics(predict()[val_mask], data["y"][val_mask])["log_loss"]
        print(f"Previous generation validation loss on the expanded league: {best:.6f}", flush=True)

    started = time.perf_counter()
    for epoch in range(args.epochs):
        # Warm up on unbiased MC outcomes; later mix in TD(lambda), using a
        # frozen prediction snapshot rather than moving targets within a batch.
        td = torch.from_numpy(td_lambda_targets(data, predict())) if epoch >= 5 else yt
        net.train()
        order = train_rows[torch.randperm(len(train_rows))]
        losses = []
        for rows in order.split(args.batch):
            logits = net(xt[rows])
            target = .7*yt[rows] + .3*td[rows]
            value_loss = nn.functional.binary_cross_entropy_with_logits(logits[:, 0], target)
            shot_loss = nn.functional.binary_cross_entropy_with_logits(logits[:, 1:], aux[rows], reduction="none")
            shot_loss = (shot_loss * mask[rows, None]).sum(0) / mask[rows].sum().clamp(min=1)
            loss = value_loss + .3*shot_loss[0] + .15*shot_loss[1]
            opt.zero_grad(set_to_none=True)
            loss.backward()
            nn.utils.clip_grad_norm_(net.parameters(), 1.)
            opt.step()
            losses.append(float(loss.detach()))
        scheduler.step()
        prediction = predict()
        val = metrics(prediction[val_mask], data["y"][val_mask])
        row = dict(epoch=epoch+1, loss=float(np.mean(losses)), **val, seconds=time.perf_counter()-started)
        history.append(row)
        print(json.dumps(row), flush=True)
        if val["log_loss"] < best - 1e-4:
            best, stale, best_epoch = val["log_loss"], 0, epoch+1
            temporary = checkpoint.with_suffix(".tmp")
            torch.save(dict(state=net.state_dict(), config=net.config, schema=1), temporary)
            os.replace(temporary, checkpoint)
        else:
            stale += 1
        if stale >= 7:
            break
    net.load_state_dict(torch.load(checkpoint, weights_only=True)["state"])
    prediction = predict()
    exported = WinProbability(net.eval())
    model_path = ROOT / "value.onnx"
    torch.onnx.export(exported, (xt[:8],), str(model_path), input_names=["features"], output_names=["win_probability"],
                      dynamic_axes={"features": {0: "batch"}, "win_probability": {0: "batch"}},
                      opset_version=17, dynamo=False)
    onnx = OnnxValue(model_path)
    probe = xt[test_mask][:128].numpy()
    expected = prediction[test_mask][:128]
    actual = np.asarray([onnx(x) for x in probe])
    error = float(np.abs(expected-actual).max())
    if error > 1e-5:
        raise RuntimeError(f"ONNX parity failed: {error}")
    # Benchmark the single-position workload used by the game, after warm-up.
    for _ in range(20): onnx(probe[0])
    tick = time.perf_counter()
    for _ in range(500): onnx(probe[0])
    latency_ms = (time.perf_counter()-tick)*2
    stratified = {}
    for d, name in enumerate(DISCIPLINES):
        for skill in range(5):
            selected = test_mask & (data["strata"][:, 0] == d) & (data["strata"][:, 2] == skill)
            if selected.any(): stratified[f"{name}/{LADDER[skill]}"] = dict(positions=int(selected.sum()), **metrics(prediction[selected], data["y"][selected]))
    # Conditioning sensitivity evaluated on held-out boards, keeping own skill fixed.
    low, high = probe.copy(), probe.copy()
    low[:, -8:-5] = [1.1/1.2, .14/.16, .18/.2]
    high[:, -8:-5] = [.16/1.2, .04/.16, .05/.2]
    with torch.no_grad():
        skill_delta = float((exported(torch.from_numpy(low))-exported(torch.from_numpy(high))).mean())
    report = dict(schema=1, generation=args.generation, features=FEATURE_COUNT, best_epoch=best_epoch,
                  architecture=dict(**net.config, relation_bias=True, auxiliary_heads=["next_pot", "foul"]),
                  parameters=sum(p.numel() for p in net.parameters()), seed=args.seed,
                  train_frames=int(len(np.unique(data["frame"][train_mask]))),
                  validation_frames=int(len(np.unique(data["frame"][val_mask]))),
                  test_frames=int(len(np.unique(data["frame"][test_mask]))),
                  train_positions=int(train_mask.sum()), test=metrics(prediction[test_mask], data["y"][test_mask]),
                  validation=metrics(prediction[val_mask], data["y"][val_mask]),
                  base_rate=metrics(np.full(test_mask.sum(), data["y"][train_mask].mean()), data["y"][test_mask]),
                  stratified=stratified, weaker_opponent_value_delta=skill_delta,
                  onnx_max_difference=error, inference_ms=latency_ms,
                  model_sha256=hashlib.sha256(model_path.read_bytes()).hexdigest(), history=history)
    report["linear_baseline"] = linear_reference(data, train_mask, val_mask, test_mask)
    report["provenance"] = provenance(args.generation, dict(max_epochs=args.epochs, batch=args.batch, lr=args.lr,
             threads=args.threads, optimizer="AdamW", weight_decay=.001, gradient_clip=1., td_lambda=.7,
             monte_carlo_weight=.7, auxiliary_weights=[.3, .15], onnx_opset=17))
    (ROOT / "training.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k:v for k,v in report.items() if k not in ("history", "stratified")}, indent=2), flush=True)


def match_chunk(task):
    import billiards_sim as bs
    model, discipline, level, start, count, weight, phase = task
    value = OnnxValue(model) if weight > 0 else None
    results = []
    for rack in range(start, start+count):
        pair = []
        for learned_seat in (0, 1):
            seed = 800_000_000 + phase*10_000_000 + discipline*100_000 + level*1000 + rack
            env = bs.Environment(getattr(bs.Discipline, DISCIPLINES[discipline]), 1)
            env.reset(seed, 0)
            profiles = [bs.profile("iris_lindqvist"), bs.profile("iris_lindqvist")]
            profiles[1-learned_seat] = bs.profile(LADDER[level])
            profiles[learned_seat].value_weight = weight
            # Compare against exactly the same skill matchup with classical
            # scoring: paired seed/seat runs, rather than claiming a Pro wins
            # because its opponent happens to be Club.
            for move in range(250):
                if env.decision() == bs.Decision.FRAME_OVER: break
                p = env.to_act()
                env.play_profile(profiles[p], profiles[1-p], (seed*7919+move) % 2**32,
                                 value if weight > 0 and p == learned_seat else None)
            pair.append(-1 if env.winner() < 0 else int(env.winner() == learned_seat))
        results.append((rack, pair))
    return (discipline, level), results


def evaluate(args):
    model = str(Path(args.model or ROOT / "value.onnx").resolve())
    tasks = [(model, d, level, start, min(5, args.racks-start), args.weight, args.phase)
             for d in range(3) for level in (0, 2, 4) for start in range(0, args.racks, 5)]
    started, groups = time.perf_counter(), {}
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for key, pairs in pool.imap_unordered(match_chunk, tasks):
            groups.setdefault(key, []).extend(pairs)
            print(f"evaluated {sum(len(v)*2 for v in groups.values())} frames in {time.perf_counter()-started:.1f}s", flush=True)
    report = dict(weight=args.weight, racks_per_matchup=args.racks, phase=args.phase,
                  model_sha256=hashlib.sha256(Path(model).read_bytes()).hexdigest() if args.weight > 0 else None, matchups={})
    for (d, level), pairs in sorted(groups.items()):
        a = np.asarray([pair for _, pair in sorted(pairs)])
        completed = a[a>=0]
        report["matchups"][f"{DISCIPLINES[d]}/{LADDER[level]}"] = dict(wins=int(completed.sum()), frames=len(completed),
              unfinished=int((a<0).sum()), win_rate=float(completed.mean()) if len(completed) else None,
              paired_outcomes=a.tolist())
    path = ROOT / f"evaluation-phase{args.phase}-w{args.weight:g}.json"
    path.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({**report, "matchups": {k: {field: value for field, value in v.items() if field != "paired_outcomes"}
                                           for k, v in report["matchups"].items()}}, indent=2))


def comparison(learned, baseline):
    if learned["racks_per_matchup"] != baseline["racks_per_matchup"] or learned["phase"] != baseline["phase"]:
        raise ValueError("Head-to-head comparisons must use identical racks and phases")
    groups, differences = {}, []
    for key, result in learned["matchups"].items():
        if result["unfinished"] or baseline["matchups"][key]["unfinished"]:
            raise ValueError("Do not silently discard unfinished evaluation frames")
        delta = (np.asarray(result["paired_outcomes"]) - np.asarray(baseline["matchups"][key]["paired_outcomes"])).mean(1)
        differences.append(delta)
        groups.setdefault(key.split("/")[0], []).append(delta)
    delta = np.concatenate(differences)
    rng = np.random.default_rng(123)
    bootstrap = np.asarray([np.concatenate([rng.choice(part, len(part), replace=True) for part in differences]).mean()
                            for _ in range(5000)])
    return dict(paired_win_rate_change=float(delta.mean()),
                paired_bootstrap_95=[float(np.quantile(bootstrap, .025)), float(np.quantile(bootstrap, .975))],
                by_discipline={key: float(np.concatenate(parts).mean()) for key, parts in groups.items()})


def install(args):
    """Install only an evaluated artifact. This is a local repository write."""
    report = json.loads((ROOT / "training.json").read_text())
    baseline = json.loads((ROOT / "evaluation-phase1-w0.json").read_text())
    learned = json.loads((ROOT / f"evaluation-phase1-w{args.weight:g}.json").read_text())
    model = ROOT / "value.onnx"
    digest = hashlib.sha256(model.read_bytes()).hexdigest()
    if digest != report["model_sha256"] or digest != learned["model_sha256"]:
        raise ValueError("Evaluated, trained and exported model hashes must match")
    if args.weight <= 0:
        raise ValueError("A learned release must actually use the model")
    adaptation = json.loads((ROOT / "adaptation.json").read_text())
    calibration_path = Path("../assets/data/ai/skill_calibration.json")
    calibration_hash = hashlib.sha256(calibration_path.read_bytes()).hexdigest()
    estimator_hash = hashlib.sha256(Path("../src/ai/opponent_model.cpp").read_bytes()).hexdigest()
    if (adaptation["model_sha256"] != digest or adaptation["calibration_sha256"] != calibration_hash or
            adaptation["native_estimator_sha256"] != estimator_hash or adaptation["weight"] != args.weight):
        raise ValueError("Adaptation must be tested with the released model, estimator and calibration")
    change = comparison(learned, baseline)
    checks = dict(held_out_value=report["test"]["log_loss"] < report["base_rate"]["log_loss"]-.01,
                  nonlinear_value=report["test"]["log_loss"] < report["linear_baseline"]["test"]["log_loss"],
                  opponent_conditioning=report["weaker_opponent_value_delta"] > .02,
                  onnx_parity=report["onnx_max_difference"] < 1e-5,
                  cpu_budget=report["inference_ms"] < 1.,
                  evaluation_size=learned["racks_per_matchup"] >= 30,
                  strength_point_estimate=change["paired_win_rate_change"] >= -.02,
                  strength_interval=change["paired_bootstrap_95"][0] > -.08,
                  discipline_regressions=min(change["by_discipline"].values()) >= -.05,
                  adaptive_response=all(adaptation["matchups"][f"{game}/viktor_hale"]["curve"][-1]["skill"] >
                                        adaptation["matchups"][f"{game}/sam_whitlock"]["curve"][-1]["skill"]+.15
                                        for game in DISCIPLINES))
    result = dict(checks=checks, **change, interpretation="A guarded hybrid release; a strength gain requires a positive lower confidence bound.")
    (ROOT / "gate.json").write_text(json.dumps(result, indent=2)+"\n")
    if not all(checks.values()):
        raise RuntimeError(f"Model did not pass release checks: {checks}")
    destination = Path("../assets/models")
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(model, destination / "adaptive_value.onnx")
    card = dict(name="Opponent-conditioned relational pool value", version=1, training=report,
                release=result, learned_weight=args.weight,
                evaluation={"baseline": baseline, "learned": learned}, adaptation=adaptation,
                skill_calibration=json.loads(calibration_path.read_text()))
    (destination / "adaptive_ai.json").write_text(json.dumps(card, indent=2)+"\n")
    players_file = Path("../assets/data/ai/players.json")
    players = json.loads(players_file.read_text())
    for profile in players["players"]:
        profile["learnedValue"], profile["valueWeight"] = True, args.weight
    players_file.write_text(json.dumps(players, indent=2)+"\n")
    print(json.dumps(result, indent=2))


def adaptation_chunk(task):
    import billiards_sim as bs
    model, discipline, level, racks, weight = task
    value = OnnxValue(model)
    estimator = bs.OpponentModel()
    opponent = bs.profile(LADDER[level])
    personality = bs.profile("adaptive")
    personality.value_weight = weight
    curve, unfinished = [], 0
    for rack in range(racks):
        seed = 950_000_000 + discipline*100_000 + level*1000 + rack
        env = bs.Environment(getattr(bs.Discipline, DISCIPLINES[discipline]), 1)
        env.reset(seed, rack % 2)
        for move in range(250):
            decision = env.decision()
            if decision == bs.Decision.FRAME_OVER: break
            player = env.to_act()
            effective = estimator.adapt(personality)
            ease = 0.
            shot_seed = (seed*7919+move) % 2**32
            if player == 1 and decision == bs.Decision.SHOT:
                planned = env.plan_shot(LADDER[level], shot_seed)
                ease = env.pot_ease(planned.aim)
            result = env.play_profile(effective if player == 0 else opponent,
                         estimator.estimated_profile() if player == 0 else effective,
                         shot_seed, value if player == 0 else None)
            if player == 1:
                estimator.observe(ease, bool(result["potted"]), result["foul"] != 0, decision == bs.Decision.BREAK,
                                  getattr(bs.Discipline, DISCIPLINES[discipline]))
        unfinished += int(env.winner() < 0)
        effective = estimator.adapt(personality)
        curve.append(dict(frame=rack, skill=estimator.skill, aim_noise=effective.aim_noise,
                          observations=estimator.observations))
    return f"{DISCIPLINES[discipline]}/{LADDER[level]}", dict(curve=curve, unfinished=unfinished)


def adapt(args):
    model = str(Path(args.model or ROOT / "value.onnx").resolve())
    tasks = [(model, d, level, args.racks, args.weight) for d in range(3) for level in (0, 2, 4)]
    groups = {}
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for key, result in pool.imap_unordered(adaptation_chunk, tasks):
            groups[key] = result
            print(key, result["curve"][-1], flush=True)
    report = dict(model_sha256=hashlib.sha256(Path(model).read_bytes()).hexdigest(), racks=args.racks,
                  weight=args.weight, matchups=groups,
                  calibration_sha256=hashlib.sha256(Path("../assets/data/ai/skill_calibration.json").read_bytes()).hexdigest(),
                  native_estimator_sha256=hashlib.sha256(Path("../src/ai/opponent_model.cpp").read_bytes()).hexdigest(),
                  limitation="Purposeful bot opponents validate response direction; human skill-rating accuracy is not established.")
    (ROOT / "adaptation.json").write_text(json.dumps(report, indent=2)+"\n")


def calibration_chunk(task):
    import billiards_sim as bs
    start, count = task
    records = []
    for frame in range(start, start+count):
        d = frame % 3
        levels = ((frame//3) % 5, (frame//15) % 5)
        profiles = [bs.profile(LADDER[level]) for level in levels]
        seed = 620_000_000 + frame
        env = bs.Environment(getattr(bs.Discipline, DISCIPLINES[d]), 1)
        env.reset(seed, frame % 2)
        for move in range(250):
            decision = env.decision()
            if decision == bs.Decision.FRAME_OVER: break
            p = env.to_act()
            shot_seed = (seed*7919+move) % 2**32
            ease = 0.
            if decision == bs.Decision.SHOT:
                planned = env.plan_shot(LADDER[levels[p]], shot_seed)
                ease = env.pot_ease(planned.aim)
            result = env.play_profile(profiles[p], profiles[1-p], shot_seed)
            if ease >= .02:
                records.append((frame, d, levels[p]/4, ease, float(bool(result["potted"])), float(result["foul"] != 0)))
    return records


def calibrate(args):
    """Fit monotone pot/foul likelihoods for the native Bayesian skill filter."""
    import torch
    from torch import nn
    torch.set_num_threads(1)
    tasks = [(start, min(15, args.frames-start)) for start in range(0, args.frames, 15)]
    records = []
    with mp.get_context("spawn").Pool(args.workers) as pool:
        for part in pool.imap_unordered(calibration_chunk, tasks):
            records.extend(part)
    data = np.asarray(sorted(records), dtype=np.float32)
    np.save(ROOT / "skill-calibration.npy", data)
    train_mask, validation, test = split_frames(data[:, 0].astype(np.int64), 91)
    validation |= test
    features = torch.from_numpy(np.column_stack([data[:, 2], np.log(data[:, 3])]))
    discipline = torch.from_numpy(data[:, 1].astype(np.int64))
    pot, foul = torch.from_numpy(data[:, 4]), torch.from_numpy(data[:, 5])
    report = dict(version=1, source="Purposeful direct-pot attempts under the game's noisy classical search",
                  frames=args.frames, attempts=len(data), holdout_frames=len(np.unique(data[validation, 0])), seed_domain=620_000_000)
    for name, label, selected, sign in (("pot", pot, train_mask & (data[:, 5]==0), 1),
                                        ("foul", foul, train_mask, -1)):
        intercept = nn.Parameter(torch.full((3,), 1.5 if name == "pot" else -2.2))
        raw = nn.Parameter(torch.zeros(2))
        opt = torch.optim.Adam([intercept, raw], lr=.03)
        rows = torch.from_numpy(np.flatnonzero(selected))
        for _ in range(600):
            coefficient = sign * nn.functional.softplus(raw)
            logit = intercept[discipline[rows]] + (features[rows]*coefficient).sum(-1)
            loss = nn.functional.binary_cross_entropy_with_logits(logit, label[rows])
            opt.zero_grad(); loss.backward(); opt.step()
        coefficient = (sign*nn.functional.softplus(raw)).detach().numpy()
        held = validation & ((data[:, 5]==0) if name == "pot" else True)
        with torch.no_grad():
            prediction = (intercept[discipline] + (features*torch.from_numpy(coefficient)).sum(-1)).sigmoid().numpy()
        report[name] = dict(intercepts=intercept.detach().tolist(), skill=float(coefficient[0]), log_ease=float(coefficient[1]),
                            validation=metrics(prediction[held], label.numpy()[held]))
    path = Path("../assets/data/ai/skill_calibration.json")
    path.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["generate", "train", "evaluate", "install", "adapt", "calibrate"])
    parser.add_argument("--frames", type=int, default=2400)
    parser.add_argument("--workers", type=int, default=6)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--generation", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--epochs", type=int, default=25)
    parser.add_argument("--batch", type=int, default=512)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--model")
    parser.add_argument("--weight", type=float, default=.25)
    parser.add_argument("--racks", type=int, default=40)
    parser.add_argument("--phase", type=int, default=0, help="0 tuning, 1 disjoint final evaluation")
    args = parser.parse_args()
    if args.frames <= 0 or args.workers <= 0 or args.threads <= 0 or args.racks <= 0 or args.epochs <= 0:
        parser.error("counts must be positive")
    if not 0 <= args.weight <= 1 or args.generation < 0 or args.lr <= 0:
        parser.error("weight must be 0..1, generation nonnegative and learning rate positive")
    globals()[args.command](args)


if __name__ == "__main__":
    main()
