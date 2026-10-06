#!/usr/bin/env bash
# Experiment 7 (Milestone 9): elite regression and AWR versus SAC, all on
# SingleShotEnv with the target mouth as input and target-pocket reward.
#   bash scripts/experiment7_elite.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment7
{
  for seed in 1 2 3 4 5; do
    echo "train_elite elite_single_ball elite-ghost-100k $seed algorithm=elite env.action_frame=ghost_relative training.total_timesteps=100000"
    echo "train_elite elite_single_ball awr-ghost-100k $seed algorithm=awr env.action_frame=ghost_relative training.total_timesteps=100000"
    echo "train_elite elite_single_ball elite-ghost-1m $seed algorithm=elite env.action_frame=ghost_relative training.total_timesteps=1000000 training.eval_every=50000"
    echo "train_elite elite_single_ball elite-absolute-1m $seed algorithm=elite env.action_frame=absolute training.total_timesteps=1000000 training.eval_every=50000"
    echo "train_elite elite_single_ball awr-absolute-1m $seed algorithm=awr env.action_frame=absolute training.total_timesteps=1000000 training.eval_every=50000"
    echo "train sac_single_ball sac-ghost-target-100k $seed env.action_frame=ghost_relative env.obs_mode=positions+target env.reward=sparse_target"
  done
} | xargs -P 6 -L 1 sh -c 'module=$1; cfg=$2; tag=$3; seed=$4; shift 4; uv run python -m "billiards_rl.training.${module}" \
    --config "configs/${cfg}.yaml" --seed "$seed" --set "$@" \
    --name "experiment7/${tag}-s${seed}" > "runs/experiment7/${tag}-s${seed}.log" 2>&1' _
echo done
