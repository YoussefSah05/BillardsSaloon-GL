#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "gameplay/shot_result.h"

namespace BilliardsSaloon
{
    namespace Physics
    {
        void stepBilliardsWorld(
            Registry& registry,
            double deltaTimeSeconds,
            ShotResult& shotResult);

        bool anyBallInMotion(Registry& registry);
    }
}