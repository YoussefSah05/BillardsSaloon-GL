#include "scene/table_geometry.h"
#include "gameplay/sim_bridge.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace BilliardsSaloon;

namespace
{
    void checkMesh(const MeshData& mesh)
    {
        REQUIRE_FALSE(mesh.vertices.empty());
        CHECK(mesh.indices.size() % 3 == 0);
        for (const MeshVertex& v : mesh.vertices)
        {
            CHECK(std::abs(glm::length(v.normal) - 1.0f) < 1.0e-3f);
            CHECK(std::isfinite(v.position.x));
        }
        for (const std::uint32_t i : mesh.indices)
        {
            CHECK(i < mesh.vertices.size());
        }
    }
}

TEST_CASE("table geometry: every part is a valid mesh")
{
    const Sim::Table table = Sim::buildPocketTable(Sim::PocketTableSpec{});
    const TableGeometry geometry = buildTableGeometry(table);
    checkMesh(geometry.cushions);
    checkMesh(geometry.rails);
    checkMesh(geometry.trim);
    checkMesh(geometry.apron);
    checkMesh(geometry.legs);
    checkMesh(geometry.pocketRims);
    checkMesh(geometry.pocketCups);
    CHECK(geometry.diamonds.vertices.size() == 18 * 6);   // 18 sights, two triangles each
}

TEST_CASE("table geometry: the cushions' faces stand on the simulator's nose lines")
{
    const Sim::Table table = Sim::buildPocketTable(Sim::PocketTableSpec{});
    const TableGeometry geometry = buildTableGeometry(table);
    const float halfLength = 0.5f * static_cast<float>(table.length);
    const float halfWidth = 0.5f * static_cast<float>(table.width);

    // Nothing of the cushions reaches into the playing area.
    for (const MeshVertex& v : geometry.cushions.vertices)
    {
        const bool outsideLength = std::abs(v.position.x) >= halfLength - 1.0e-4f;
        const bool outsideWidth = std::abs(v.position.z) >= halfWidth - 1.0e-4f;
        CHECK((outsideLength || outsideWidth));
    }

    // And the cushion top faces up everywhere.
    for (const MeshVertex& v : geometry.cushions.vertices)
    {
        if (v.position.y > 0.03f)
        {
            CHECK(v.normal.y > -1.0e-3f);
        }
    }
}

TEST_CASE("table geometry: pocket openings are cut out of the rail")
{
    const Sim::Table table = Sim::buildPocketTable(Sim::PocketTableSpec{});
    const TableGeometry geometry = buildTableGeometry(table);
    for (const Sim::Pocket& pocket : table.pockets)
    {
        const glm::vec3 c = SimBridge::toGamePosition(pocket.center, table.length, table.width);
        for (const MeshVertex& v : geometry.rails.vertices)
        {
            if (v.normal.y > 0.5f)
            {
                // No rail-top cell has its middle inside an opening; corners may touch it.
                CHECK(glm::length(glm::vec2(v.position.x - c.x, v.position.z - c.z)) > static_cast<float>(pocket.radius) - 0.0142f);
            }
        }
    }
}

#include "scene/hall_geometry.h"

TEST_CASE("hall geometry: both layouts build valid meshes, with lamps over the table")
{
    for (const HallLayout layout : {HallLayout::Arena, HallLayout::Saloon})
    {
        const HallGeometry hall = buildHall(layout, glm::vec2(1.45f, 0.81f), -0.775f);
        REQUIRE_FALSE(hall.parts.empty());
        for (const HallPart& part : hall.parts)
        {
            CAPTURE(part.name);
            checkMesh(part.mesh);
        }
        for (const glm::vec3& lamp : hall.lamps)
        {
            CHECK(lamp.y >= 1.0f);              // WPA: at least 40 in above the bed
            CHECK(std::abs(lamp.x) < 1.27f);    // over the playing surface
        }
    }
    CHECK(hallLayoutFromName("saloon") == HallLayout::Saloon);
    CHECK(hallLayoutFromName("arena") == HallLayout::Arena);
}
