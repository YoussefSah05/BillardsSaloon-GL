#pragma once

#include "ecs/registry.h"
#include "gameplay/game_variant.h"
#include "gameplay/match_state.h"
#include "gameplay/shot_result.h"

namespace BilliardsSaloon
{
    namespace Rules
    {
        void resolveShot(
            const GameVariantDefinition& variant,
            MatchState& matchState,
            Registry& registry,
            const ShotResult& shotResult);
    }
}