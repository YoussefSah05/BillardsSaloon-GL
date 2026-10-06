#!/usr/bin/env bash
# Experiment 1: SAC, absolute vs ghost-relative action frame, 5 seeds each.
#   bash scripts/experiment1_sac_frames.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment1
for frame in absolute ghost_relative; do
  for seed in 1 2 3 4 5; do
    echo "$frame $seed"
  done
done | xargs -P 5 -n 2 sh -c 'uv run python -m billiards_rl.training.train --config configs/sac_single_ball.yaml --seed "$2" \
    --set "env.action_frame=$1" --name "experiment1/sac-$1-s$2" > "runs/experiment1/sac-$1-s$2.log" 2>&1' _
echo done
