#!/usr/bin/env bash
# Experiment 6b: HER with contact-gated goals (a shot that never touches the
# object ball achieves no goal). SAC, absolute aim, 200k steps, 5 seeds.
# Control: experiment 6's sac-goal-absolute-noher (gating changes nothing without HER).
#   bash scripts/experiment6b_her_contact.sh        (from ml/)
set -euo pipefail
mkdir -p runs/experiment6b
for seed in 1 2 3 4 5; do echo "$seed"; done | xargs -P 5 -I{} sh -c 'uv run python -m billiards_rl.training.train \
    --config configs/sac_her.yaml --seed {} --set env.action_frame=absolute env.require_contact=true her.enabled=true \
    training.total_timesteps=200000 --name "experiment6b/sac-goal-absolute-her-contact-s{}" \
    > "runs/experiment6b/sac-goal-absolute-her-contact-s{}.log" 2>&1'
echo done
