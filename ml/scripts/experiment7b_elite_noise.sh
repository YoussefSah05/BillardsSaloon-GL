#!/usr/bin/env bash
# Experiment 7b: elite regression trained under 0.25 degrees of aim noise
# (ghost-relative, 1M steps, 5 seeds): do successes under noise select wide windows?
#   bash scripts/experiment7b_elite_noise.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment7b
for seed in 1 2 3 4 5; do echo "$seed"; done | xargs -P 5 -I{} sh -c 'uv run python -m billiards_rl.training.train_elite \
    --config configs/elite_single_ball.yaml --seed {} --set algorithm=elite env.action_frame=ghost_relative "env.noise=[0.25,0.0]" \
    training.total_timesteps=1000000 training.eval_every=50000 --name "experiment7b/elite-ghost-1m-noisy-s{}" \
    > "runs/experiment7b/elite-ghost-1m-noisy-s{}.log" 2>&1'
echo done
