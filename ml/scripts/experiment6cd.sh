#!/usr/bin/env bash
# Experiment 6d: what makes the goal-conditioned ghost-relative policy better?
#   A: positions only, target-pocket reward (sparse_target)
#   B: target mouth as input (positions+target), any-pocket reward (sparse)
#   (Exp. 2 sac-ghost_relative = neither; Exp. 6 sac-goal-ghost-noher = both)
# Experiment 6c: departure-direction goals, absolute aim, with and without HER.
#   bash scripts/experiment6cd.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment6d runs/experiment6c
{
  for seed in 1 2 3 4 5; do
    echo "sac_single_ball 6d sac-ghost-targetreward $seed env.action_frame=ghost_relative env.reward=sparse_target"
    echo "sac_single_ball 6d sac-ghost-targetinput $seed env.action_frame=ghost_relative env.obs_mode=positions+target"
    echo "sac_her 6c sac-direction-absolute-her $seed env.action_frame=absolute env.goal_type=direction her.enabled=true training.total_timesteps=200000"
    echo "sac_her 6c sac-direction-absolute-noher $seed env.action_frame=absolute env.goal_type=direction her.enabled=false training.total_timesteps=200000"
  done
} | xargs -P 6 -L 1 sh -c 'cfg=$1; exp=$2; tag=$3; seed=$4; shift 4; uv run python -m billiards_rl.training.train \
    --config "configs/${cfg}.yaml" --seed "$seed" --set "$@" \
    --name "experiment${exp}/${tag}-s${seed}" > "runs/experiment${exp}/${tag}-s${seed}.log" 2>&1' _
echo done
