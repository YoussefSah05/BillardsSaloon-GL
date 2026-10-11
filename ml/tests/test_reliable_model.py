import numpy as np
import torch
import pytest

from billiards_ml.reliable_model import ReliableValueNet, CalibratedEnsemble
from billiards_ml.reliable_train import calibration_partition, frame_bootstrap_weights, fit_temperature
from test_adaptive_model import sample


def test_gated_graph_ensemble_preserves_ball_symmetries_and_empty_tables():
    torch.manual_seed(19)
    model = CalibratedEnsemble([ReliableValueNet(), ReliableValueNet()], .8).eval()
    x = sample()
    permutation = x.copy()
    permutation[:224] = x[:224].reshape(16, 14)[np.random.default_rng(1).permutation(16)].reshape(-1)
    absent = x.copy()
    balls = absent[:224].reshape(16, 14)
    balls[balls[:, 0] == 0, 1:] = 1e15
    with torch.no_grad():
        p, uncertainty = model(torch.from_numpy(np.stack([x, permutation, absent])))
        empty = torch.from_numpy(x.copy())[None]
        empty[:, :224] = 0
        empty_outputs = model(empty)
    for output in (p, uncertainty):
        assert torch.isfinite(output).all()
        assert torch.allclose(output, output[:1].expand_as(output), atol=1e-6)
    assert (p >= 0).all() and (p <= 1).all()
    assert (uncertainty >= 0).all() and (uncertainty <= .5).all()
    assert all(torch.isfinite(output).all() for output in empty_outputs)


def test_same_member_has_zero_disagreement_and_temperature_keeps_decisions():
    net = ReliableValueNet().eval()
    x = torch.from_numpy(sample())[None]
    with torch.no_grad():
        a, std = CalibratedEnsemble([net, net])(x)
        b, _ = CalibratedEnsemble([net, net], 2)(x)
    assert float(std) == 0
    assert (a > .5).item() == (b > .5).item()
    assert (b - .5).abs().item() <= (a - .5).abs().item()
    with pytest.raises(ValueError):
        CalibratedEnsemble([net], 0)


def test_bootstrap_and_calibration_never_split_a_frame():
    frames = np.repeat(np.arange(20), 5)
    train = frames < 12
    validation = frames >= 12
    selection, calibration = calibration_partition(frames, validation)
    assert not set(frames[selection]) & set(frames[calibration])
    assert np.array_equal(selection | calibration, validation)
    weights = frame_bootstrap_weights(frames, train, 91)
    assert not weights[~train].any()
    for frame in np.unique(frames):
        assert len(np.unique(weights[frames == frame])) == 1
    # Confident but incorrect predictions should become less confident.
    temperature = fit_temperature(np.array([.01, .99, .01, .99]), np.array([0, 0, 1, 1]))
    assert 1 < temperature <= 4
