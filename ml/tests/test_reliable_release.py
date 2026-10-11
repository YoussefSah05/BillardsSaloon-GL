import hashlib
import json

import pytest

from billiards_ml import reliable_validate as release


def test_failed_prediction_gate_preserves_shipped_model(tmp_path, monkeypatch):
    output = tmp_path / "run"
    output.mkdir()
    shipped = tmp_path / "shipped.onnx"
    shipped.write_bytes(b"current release")
    candidate = output / "value.onnx"
    candidate.write_bytes(b"candidate")
    monkeypatch.setattr(release, "OUTPUT", output)
    monkeypatch.setattr(release, "PREVIOUS", shipped)
    old_hash = hashlib.sha256(shipped.read_bytes()).hexdigest()
    new_hash = hashlib.sha256(candidate.read_bytes()).hexdigest()
    (output / "training.json").write_text(json.dumps(dict(model_sha256=new_hash,
        previous_model_sha256=old_hash, inference_ms=.3, onnx_max_difference=1e-7, weaker_opponent_value_delta=.15)))
    previous = dict(accuracy=.7, log_loss=.6, brier=.2, ece_15=.04)
    new = dict(accuracy=.69, log_loss=.61, brier=.21, ece_15=.03)
    (output / "predictions.json").write_text(json.dumps(dict(model_sha256=new_hash,
        previous_model_sha256=old_hash, new=new, previous=previous, frames=900,
        by_discipline={"NINE_BALL":dict(new=new, previous=previous)})))
    # No match/adaptation files exist: an inferior predictor must be rejected
    # before any installation or attempt to read the later release inputs.
    with pytest.raises(RuntimeError, match="shipped model preserved"):
        release.install(None)
    assert shipped.read_bytes() == b"current release"
    gate = json.loads((output / "gate.json").read_text())
    assert not gate["installed"]
    assert not gate["checks"]["fresh_accuracy"]
    assert gate["checks"]["calibration"]
