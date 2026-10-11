#pragma once

#include "scene/mesh_data.h"

#include <glm/glm.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Septhuit
{
    enum class HallLayout
    {
        Arena,     // tournament arena: carpet, barriers, stands, canopy light
        Saloon     // panelled room with hanging globe lamps
    };

    [[nodiscard]] HallLayout hallLayoutFromName(const std::string& name);

    // Where the three lamps hang for a layout (the same as buildHall's);
    // cheap, without building the hall.
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
        WoodPanel,
        Plaster,       // warm upper walls
        Ceiling,
        Beam,          // dark stained wood
        Brass,
        Mirror,
        WindowGlow,    // emissive window panes (evening light outside)
        BarTop,        // polished wood
        Canvas,        // pictures (tinted per part)
        SconceShade,   // emissive fabric shades of wall and pendant lights
        Leather,
        Glass,         // bottles (tinted per part)
        LedStrip       // emissive accent lines
    };

    struct HallPart
    {
        std::string name;
        HallMaterial material {HallMaterial::Wall};
        MeshData mesh;
        bool castsShadow {false};
        std::optional<glm::vec3> tint;   // overrides the material's colour (pictures, bottles)
    };

    // A practical light: sconces, ceiling fixtures, bar lights (see LocalLightComponent).
    struct HallLight
    {
        glm::vec3 position {0.0f};
        glm::vec3 color {1.0f};
        float intensity {1.0f};
        float range {4.0f};
        glm::vec3 direction {0.0f, -1.0f, 0.0f};
        float spotOuterCos {-2.0f};
        float spotInnerCos {-2.0f};
    };

    struct HallGeometry
    {
        std::vector<HallPart> parts;
        std::vector<HallLight> lights;
        // Where light probes for indirect light are placed: a grid of
        // probeCounts over the box probeMin..probeMax (inside the room).
        glm::vec3 probeMin {-2.0f, -0.7f, -1.5f};
        glm::vec3 probeMax {2.0f, 1.5f, 1.5f};
        glm::ivec3 probeCounts {4, 3, 3};
        // The room's box for parallax-corrected reflections, and where the
        // reflection cube map is captured from.
        glm::vec3 roomMin {-5.0f, -0.8f, -5.0f};
        glm::vec3 roomMax {5.0f, 3.0f, 5.0f};
        glm::vec3 reflectionCentre {0.0f, 0.9f, 0.0f};
        // The three lamps over the table (game coordinates).
        std::array<glm::vec3, 3> lamps {};
    };

    // Builds the hall around a table whose rails reach tableHalfExtent (game
    // x, z), standing on a floor at floorY.
    [[nodiscard]] HallGeometry buildHall(HallLayout layout, const glm::vec2& tableHalfExtent, float floorY);
}
