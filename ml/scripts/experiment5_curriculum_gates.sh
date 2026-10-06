#!/usr/bin/env bash
# Experiment 5: curriculum gates for the absolute aim (SAC, 300k steps, 5 seeds).
#   A: threshold 0.5 with 20% review of earlier stages
#   B: threshold 0.8, advance on a plateau (15k episodes without +0.02), 20% review
# Compare with experiment 4's sac-absolute-curriculum (threshold 0.8, no review).
#   bash scripts/experiment5_curriculum_gates.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment5
{
  for seed in 1 2 3 4 5; do
    echo "gate05-review $seed curriculum.threshold=0.5 curriculum.review_fraction=0.2"
    echo "plateau-review $seed curriculum.threshold=0.8 curriculum.plateau_episodes=15000 curriculum.review_fraction=0.2"
  done
} | xargs -P 6 -L 1 sh -c 'tag=$1; seed=$2; shift 2; uv run python -m billiards_rl.training.train \
    --config configs/sac_curriculum.yaml --seed "$seed" --set env.action_frame=absolute "$@" \
    --name "experiment5/sac-absolute-${tag}-s${seed}" > "runs/experiment5/sac-absolute-${tag}-s${seed}.log" 2>&1' _
echo done
