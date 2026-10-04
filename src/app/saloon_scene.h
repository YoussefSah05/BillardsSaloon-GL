#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "render/light_rig.h"

namespace BilliardsSaloon
{
    class MatchSession;

    // The three lamps hanging over the table.
    [[nodiscard]] PointLightRig saloonLightRig();

    // Adds the room, table frame and lamps, and gives the session's table and
    // balls their meshes and materials.
    void buildSaloonScene(MatchSession& session);

    // Creates the main camera entity in the session's registry.
    [[nodiscard]] Entity createMainCamera(Registry& registry);
}
