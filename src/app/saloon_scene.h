#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "render/light_rig.h"
#include "scene/hall_geometry.h"

#include <glm/glm.hpp>

namespace BilliardsSaloon
{
    class MatchSession;

    struct EquipmentChoice;

    // The three lamps hanging over the table, in the chosen hall's lamp colour.
    [[nodiscard]] PointLightRig saloonLightRig(const glm::vec3& lampColor, HallLayout layout);

    // Replaces the hall's scenery (floor, walls, stands, lamps) with a layout.
    void rebuildHall(MatchSession& session, HallLayout layout);

    // Applies the chosen cloth, rail, trim, pocket and ball-set finishes to
    // a built scene (materials are changed in place).
    void applyEquipment(MatchSession& session, const EquipmentChoice& choice);

    // Adds the room, table frame and lamps, and gives the session's table and
    // balls their meshes and materials.
    void buildSaloonScene(MatchSession& session);

    // Creates the main camera entity in the session's registry.
    [[nodiscard]] Entity createMainCamera(Registry& registry);
}
