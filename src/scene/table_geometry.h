#pragma once

#include "scene/mesh_data.h"
#include "sim/table.h"

#include <glm/glm.hpp>

#include <vector>

namespace BilliardsSaloon
{
    // Dimensions of the furniture around the playing surface, in metres. The
    // cushions themselves come from the simulator's table, so what the player
    // sees is exactly what the balls bounce off.
    struct TableStyle
    {
        float cushionTop {0.0420f};       // top of the cushion where it meets the rail
        float noseTop {0.0385f};          // top edge of the cushion face, just above the nose
        float railTop {0.0455f};
        float railWidth {0.125f};         // wood, from the cushion's back to the outside
        float railBottom {-0.035f};
        float apronDepth {0.150f};        // below the rail
        float apronInset {0.012f};
        float trimHeight {0.006f};        // metal line between rail and apron
        float floorY {-0.775f};           // cloth 30.5 in above the floor
        float legSize {0.16f};
        float pocketRimWidth {0.030f};    // leather around the pocket opening
        float pocketDepth {0.11f};
        float diamondRadius {0.0065f};
    };

    struct TableGeometry
    {
        MeshData cushions;      // cloth-covered rubber
        MeshData rails;         // wood tops and sides
        MeshData trim;          // metal line
        MeshData apron;
        MeshData legs;
        MeshData pocketRims;    // leather
        MeshData pocketCups;    // inside of the drops (faces inward)
        MeshData diamonds;      // sights
        glm::vec2 outerHalfExtent {0.0f};   // rail outside, game x and z
    };

    // Builds the table furniture in game coordinates (x along the length,
    // z across, y up, cloth at y = 0) around a simulator table.
    [[nodiscard]] TableGeometry buildTableGeometry(const Sim::Table& table, const TableStyle& style = {});
}
