#!/usr/bin/env bash
# Experiment 3: SAC (ghost-relative) with geometry observations and/or
# training under aim noise, plus PPO at equal wall-clock time (1M steps).
# The SAC positions/no-noise cell is Experiment 2's sac-ghost_relative group.
#   bash scripts/experiment3_observation_noise.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment3
{
  for seed in 1 2 3 4 5; do
    echo "sac geo-clean $seed env.obs_mode=positions+geometry env.noise=[0.0,0.0]"
    echo "sac pos-noisy $seed env.obs_mode=positions env.noise=[0.25,0.0]"
    echo "sac geo-noisy $seed env.obs_mode=positions+geometry env.noise=[0.25,0.0]"
    echo "ppo pos-clean-1m $seed env.obs_mode=positions training.total_timesteps=1000000 training.eval_every=50000"
  done
} | xargs -P 6 -L 1 sh -c 'algo=$1; tag=$2; seed=$3; shift 3; uv run python -m billiards_rl.training.train \
    --config "configs/${algo}_single_ball.yaml" --seed "$seed" --set env.action_frame=ghost_relative "$@" \
    --name "experiment3/${algo}-${tag}-s${seed}" > "runs/experiment3/${algo}-${tag}-s${seed}.log" 2>&1' _
echo done
