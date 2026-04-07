#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"

namespace BilliardsSaloon
{
    namespace Physics
    {
        void stepBilliardsWorld(Registry& registry, double deltaTimeSeconds);
        bool anyBallInMotion(Registry& registry);
    }
}