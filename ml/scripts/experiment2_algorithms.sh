#!/usr/bin/env bash
# Experiment 2: SAC, TD3 and PPO with their SB3 defaults, both action frames,
# 5 seeds each, checkpoints selected on the validation suite.
#   bash scripts/experiment2_algorithms.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment2
for algo in sac td3 ppo; do
  for frame in absolute ghost_relative; do
    for seed in 1 2 3 4 5; do
      echo "$algo $frame $seed"
    done
  done
done | xargs -P 6 -n 3 sh -c 'uv run python -m billiards_rl.training.train --config "configs/$1_single_ball.yaml" --seed "$3" \
    --set "env.action_frame=$2" --name "experiment2/$1-$2-s$3" > "runs/experiment2/$1-$2-s$3.log" 2>&1' _
echo done
