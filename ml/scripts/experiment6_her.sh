#!/usr/bin/env bash
# Experiment 6: Hindsight Experience Replay on GoalShotEnv (SAC, 5 seeds).
# Same goal-conditioned environment and target-pocket reward with and without
# HER, so HER is the only difference. Absolute aim at 200k steps,
# ghost-relative aim at 100k steps.
#   bash scripts/experiment6_her.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment6
{
  for seed in 1 2 3 4 5; do
    echo "absolute-her $seed env.action_frame=absolute her.enabled=true training.total_timesteps=200000"
    echo "absolute-noher $seed env.action_frame=absolute her.enabled=false training.total_timesteps=200000"
    echo "ghost-her $seed env.action_frame=ghost_relative her.enabled=true training.total_timesteps=100000"
    echo "ghost-noher $seed env.action_frame=ghost_relative her.enabled=false training.total_timesteps=100000"
  done
} | xargs -P 6 -L 1 sh -c 'tag=$1; seed=$2; shift 2; uv run python -m billiards_rl.training.train \
    --config configs/sac_her.yaml --seed "$seed" --set "$@" \
    --name "experiment6/sac-goal-${tag}-s${seed}" > "runs/experiment6/sac-goal-${tag}-s${seed}.log" 2>&1' _
echo done
