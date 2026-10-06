#!/usr/bin/env bash
# Experiment 9 (Milestone 10): position play. SAC on PositionShotEnv with and
# without spin, 200k steps, 5 seeds; baselines via scripts.experiment9_baselines.
#   bash scripts/experiment9_position.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment9
{
  for seed in 1 2 3 4 5; do
    echo "spin $seed env.spin=true"
    echo "nospin $seed env.spin=false"
  done
} | xargs -P 6 -L 1 sh -c 'tag=$1; seed=$2; shift 2; uv run python -m billiards_rl.training.train \
    --config configs/sac_position.yaml --seed "$seed" --set "$@" \
    --name "experiment9/sac-position-${tag}-s${seed}" > "runs/experiment9/sac-position-${tag}-s${seed}.log" 2>&1' _
echo done
