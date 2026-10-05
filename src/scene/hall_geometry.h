#pragma once

#include "scene/mesh_data.h"

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    enum class HallLayout
    {
        Arena,     // tournament arena: carpet, barriers, stands, canopy light
        Saloon     // panelled room with hanging globe lamps
    };

    [[nodiscard]] HallLayout hallLayoutFromName(const std::string& name);

    // Where the three lamps hang for a layout (the same as buildHall's).
    [[nodiscard]] std::array<glm::vec3, 3> hallLamps(HallLayout layout);

    // What a hall part is made of; the app turns these into materials.
    enum class HallMaterial
    {
        Carpet,
        PlayingCarpet,
        Barrier,
        GoldTrim,
        Stand,
        Seat,
        Wall,
        Furniture,
        CanopyBody,
        LampPanel,     // emissive light panels / globes
        Cable,
        WoodFloor,
        WoodPanel
    };

    struct HallPart
    {
        std::string name;
        HallMaterial material {HallMaterial::Wall};
        MeshData mesh;
        bool castsShadow {false};
    };

    struct HallGeometry
    {
        std::vector<HallPart> parts;
        // The three lamps over the table (game coordinates).
        std::array<glm::vec3, 3> lamps {};
    };

    // Builds the hall around a table whose rails reach tableHalfExtent (game
    // x, z), standing on a floor at floorY.
    [[nodiscard]] HallGeometry buildHall(HallLayout layout, const glm::vec2& tableHalfExtent, float floorY);
}
