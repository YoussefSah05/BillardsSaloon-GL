#!/usr/bin/env bash
# Experiment 8: reward the shot's expected success under aim noise.
# SAC, ghost-relative, positions+target, sparse_target, 100k steps, 5 seeds.
#   robust:        reward = mean of 16 replays with 0.25 deg aim noise
#   single-noisy:  the played shot itself has 0.25 deg noise (one sample)
# Both select checkpoints under 0.25 deg noise. Clean control: experiment 7's
# sac-ghost-target-100k.
#   bash scripts/experiment8_robust_reward.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment8
common='env.action_frame=ghost_relative env.obs_mode=positions+target env.reward=sparse_target eval.during_training_noise=[0.25,0.0]'
{
  for seed in 1 2 3 4 5; do
    echo "robust $seed env.smooth_samples=16 env.smooth_sigma=0.25"
    echo "single-noisy $seed env.noise=[0.25,0.0]"
  done
} | xargs -P 6 -L 1 sh -c 'tag=$1; seed=$2; shift 2; uv run python -m billiards_rl.training.train \
    --config configs/sac_single_ball.yaml --seed "$seed" --set '"$common"' "$@" \
    --name "experiment8/sac-${tag}-s${seed}" > "runs/experiment8/sac-${tag}-s${seed}.log" 2>&1' _
echo done
