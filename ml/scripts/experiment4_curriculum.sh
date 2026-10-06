#!/usr/bin/env bash
# Experiment 4: the curriculum. SAC with the absolute aim (300k steps) with and
# without the curriculum, and SAC with the ghost-relative aim plus curriculum
# (100k steps; compare with experiment 2's sac-ghost_relative), 5 seeds each.
#   bash scripts/experiment4_curriculum.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment4
{
  for seed in 1 2 3 4 5; do
    echo "absolute-curriculum $seed curriculum.enabled=true env.action_frame=absolute"
    echo "absolute-none $seed curriculum.enabled=false env.action_frame=absolute"
    echo "ghost-curriculum $seed curriculum.enabled=true env.action_frame=ghost_relative training.total_timesteps=100000"
  done
} | xargs -P 6 -L 1 sh -c 'tag=$1; seed=$2; shift 2; uv run python -m billiards_rl.training.train \
    --config configs/sac_curriculum.yaml --seed "$seed" --set "$@" \
    --name "experiment4/sac-${tag}-s${seed}" > "runs/experiment4/sac-${tag}-s${seed}.log" 2>&1' _
echo done
