import numpy as np
import torch

import billiards_sim as bs
from billiards_ml.adaptive_model import AdaptiveValueNet, WinProbability
from billiards_ml.adaptive_train import split_frames, persistent_split, td_lambda_targets


def sample():
    env = bs.Environment(bs.Discipline.NINE_BALL, 1)
    return env.adaptive_features(0, bs.profile("iris_lindqvist"), bs.profile("sam_whitlock"))


def test_relational_model_is_permutation_invariant_and_ignores_absent_balls():
    torch.manual_seed(1)
    net = WinProbability(AdaptiveValueNet()).eval()
    x = sample()
    permuted = x.copy()
    permuted[:224] = x[:224].reshape(16, 14)[np.random.default_rng(1).permutation(16)].reshape(-1)
    modified = x.copy()
    balls = modified[:224].reshape(16, 14)
    absent = balls[:, 0] == 0
    balls[absent, 1:] = 99.
    with torch.no_grad():
        outputs = net(torch.from_numpy(np.stack([x, permuted, modified])))
    assert torch.isfinite(outputs).all()
    assert torch.allclose(outputs, outputs[:1].expand(3), atol=1e-6)


def test_all_pocketed_is_finite_and_opponent_conditioning_has_gradients():
    net = AdaptiveValueNet()
    x = torch.from_numpy(sample())[None].requires_grad_()
    output = net(x)
    assert output.shape == (1, 3)
    output[:, 0].sum().backward()
    assert x.grad[0, -8:].abs().sum() > 0
    empty = x.detach().clone()
    empty[:, :224] = 0
    assert torch.isfinite(net(empty)).all()


def test_frame_splits_do_not_leak_positions_and_td_stays_in_same_seat():
    frames = np.repeat(np.arange(20), 6)
    train, val, test = split_frames(frames)
    assert not set(frames[train]) & set(frames[val])
    assert not set(frames[train]) & set(frames[test])
    assert not set(frames[val]) & set(frames[test])
    data = dict(frame=np.array([1, 1, 1, 1, 2, 2]), player=np.array([0, 1, 0, 1, 0, 1]),
                step=np.array([0, 0, 1, 1, 0, 0]), y=np.array([1., 0., 1., 0., 0., 1.]))
    result = td_lambda_targets(data, np.array([.5, .5, .8, .2, .1, .9]), .7)
    assert np.allclose(result, [.94, .06, 1., 0., 0., 1.])


def test_expert_iteration_keeps_previous_frame_partitions(tmp_path):
    path = tmp_path / "splits.json"
    old = np.repeat(np.arange(20), 2)
    first = persistent_split(old, path, 42)
    expanded = np.concatenate([old, np.repeat(np.arange(20, 40), 2)])
    second = persistent_split(expanded, path, 42)
    for a, b in zip(first, second):
        assert np.array_equal(a, b[:len(old)])
