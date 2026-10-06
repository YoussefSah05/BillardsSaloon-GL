#include "scene/hall_geometry.h"

#include <cmath>
#include <initializer_list>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        // An axis-aligned box with outward faces; uv in metres along the faces.
        void box(MeshData& mesh, const glm::vec3& lo, const glm::vec3& hi)
        {
            const glm::vec3 c[8] = {
                {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
            const glm::vec3 size = hi - lo;
            mesh.quad(c[4], c[5], c[6], c[7], glm::vec2(size.x, size.y));
            mesh.quad(c[1], c[0], c[3], c[2], glm::vec2(size.x, size.y));
            mesh.quad(c[5], c[1], c[2], c[6], glm::vec2(size.z, size.y));
            mesh.quad(c[0], c[4], c[7], c[3], glm::vec2(size.z, size.y));
            mesh.quad(c[3], c[7], c[6], c[2], glm::vec2(size.x, size.z));
            mesh.quad(c[0], c[1], c[5], c[4], glm::vec2(size.x, size.z));
        }

        // A horizontal rectangle facing up.
        void floorQuad(MeshData& mesh, float x0, float z0, float x1, float z1, float y, float uvPerMetre)
        {
            mesh.quad({x0, y, z1}, {x1, y, z1}, {x1, y, z0}, {x0, y, z0},
                      glm::vec2((x1 - x0) * uvPerMetre, (z1 - z0) * uvPerMetre));
        }

        // A vertical wall from a to b (in plan), facing towards `facing`.
        void wallQuad(MeshData& mesh, const glm::vec2& a, const glm::vec2& b, float y0, float y1, const glm::vec2& facing)
        {
            glm::vec3 p0(a.x, y0, a.y), p1(b.x, y0, b.y), p2(b.x, y1, b.y), p3(a.x, y1, a.y);
            const glm::vec3 n = glm::cross(p1 - p0, p3 - p0);
            const glm::vec3 toFacing(facing.x - 0.5f * (a.x + b.x), 0.0f, facing.y - 0.5f * (a.y + b.y));
            if (glm::dot(n, toFacing) < 0.0f)
            {
                std::swap(p0, p1);
                std::swap(p2, p3);
            }
            mesh.quad(p0, p1, p2, p3, glm::vec2(glm::length(b - a), y1 - y0));
        }

        HallGeometry buildArena(const glm::vec2& table, float floorY)
        {
            HallGeometry hall;
            const float lampY = 1.02f;   // WPA: at least 40 in above the bed
            hall.lamps = {glm::vec3(-0.92f, lampY, 0.0f), glm::vec3(0.0f, lampY, 0.0f), glm::vec3(0.92f, lampY, 0.0f)};

            // The playing area inside the barriers.
            const glm::vec2 arena(3.9f, 2.7f);

            HallPart carpet {"Arena Carpet", HallMaterial::Carpet, {}, false};
            floorQuad(carpet.mesh, -14.0f, -14.0f, 14.0f, 14.0f, floorY, 0.5f);
            HallPart playing {"Playing Area", HallMaterial::PlayingCarpet, {}, false};
            floorQuad(playing.mesh, -arena.x, -arena.y, arena.x, arena.y, floorY + 0.003f, 0.5f);

            // Barrier boards with a gold cap line, open at the head end for the players.
            HallPart barriers {"Barriers", HallMaterial::Barrier, {}, true};
            HallPart trim {"Barrier Trim", HallMaterial::GoldTrim, {}, false};
            const float h = floorY + 0.82f;
            const float t = 0.05f;
            box(barriers.mesh, {-arena.x, floorY, arena.y}, {arena.x, h, arena.y + t});
            box(barriers.mesh, {-arena.x, floorY, -arena.y - t}, {arena.x, h, -arena.y});
            box(barriers.mesh, {arena.x, floorY, -arena.y}, {arena.x + t, h, arena.y});
            box(barriers.mesh, {-arena.x - t, floorY, -arena.y}, {-arena.x, h, -0.7f});
            box(barriers.mesh, {-arena.x - t, floorY, 0.7f}, {-arena.x, h, arena.y});
            box(trim.mesh, {-arena.x, h, arena.y - 0.005f}, {arena.x, h + 0.012f, arena.y + t + 0.005f});
            box(trim.mesh, {-arena.x, h, -arena.y - t - 0.005f}, {arena.x, h + 0.012f, -arena.y + 0.005f});
            box(trim.mesh, {arena.x - 0.005f, h, -arena.y}, {arena.x + t + 0.005f, h + 0.012f, arena.y});

            // Tiered stands on the two long sides and the foot end.
            HallPart stands {"Stands", HallMaterial::Stand, {}, false};
            HallPart seats {"Seats", HallMaterial::Seat, {}, false};
            constexpr int TIERS = 7;
            const float rise = 0.34f;
            const float depth = 0.85f;
            const float gap = 0.9f;
            for (int tier = 0; tier < TIERS; ++tier)
            {
                const float y = floorY + rise * static_cast<float>(tier + 1);
                const float near = gap + depth * static_cast<float>(tier);
                const float far = near + depth;
                // Long sides (z), then the foot end (x).
                box(stands.mesh, {-arena.x - 1.0f, floorY, arena.y + near}, {arena.x + 1.0f, y, arena.y + far});
                box(stands.mesh, {-arena.x - 1.0f, floorY, -arena.y - far}, {arena.x + 1.0f, y, -arena.y - near});
                box(stands.mesh, {arena.x + near, floorY, -arena.y - 0.2f}, {arena.x + far, y, arena.y + 0.2f});

                const float seatDepth = 0.42f;
                const auto seatRow = [&](float from, float to, bool alongX, float line, float back)
                {
                    for (float s = from; s + 0.48f <= to; s += 0.55f)
                    {
                        const float a = s;
                        const float b = s + 0.48f;
                        if (alongX)
                        {
                            const float z0 = std::min(line, line + back * seatDepth);
                            const float z1 = std::max(line, line + back * seatDepth);
                            box(seats.mesh, {a, y, z0}, {b, y + 0.12f, z1});                                // seat
                            const float bz = line + back * seatDepth;
                            box(seats.mesh, {a, y, std::min(bz, bz + back * 0.06f)}, {b, y + 0.48f, std::max(bz, bz + back * 0.06f)});   // back
                        }
                        else
                        {
                            const float x0 = std::min(line, line + back * seatDepth);
                            const float x1 = std::max(line, line + back * seatDepth);
                            box(seats.mesh, {x0, y, a}, {x1, y + 0.12f, b});
                            const float bx = line + back * seatDepth;
                            box(seats.mesh, {std::min(bx, bx + back * 0.06f), y, a}, {std::max(bx, bx + back * 0.06f), y + 0.48f, b});
                        }
                    }
                };
                seatRow(-arena.x - 0.8f, arena.x + 0.8f, true, arena.y + near + 0.2f, 1.0f);
                seatRow(-arena.x - 0.8f, arena.x + 0.8f, true, -arena.y - near - 0.2f, -1.0f);
                seatRow(-arena.y, arena.y, false, arena.x + near + 0.2f, 1.0f);
            }

            // Dark walls closing the hall behind the stands and at the head end.
            HallPart walls {"Hall Walls", HallMaterial::Wall, {}, false};
            const float wallX = arena.x + gap + depth * TIERS + 0.6f;
            const float wallZ = arena.y + gap + depth * TIERS + 0.6f;
            const float wallTop = floorY + 7.0f;
            wallQuad(walls.mesh, {-wallX, wallZ}, {wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {-wallX, -wallZ}, {wallX, -wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {wallX, -wallZ}, {wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {-wallX, -wallZ}, {-wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});

            // The scorer's desk at the head end and two players' chairs.
            HallPart furniture {"Officials", HallMaterial::Furniture, {}, true};
            box(furniture.mesh, {-arena.x + 0.35f, floorY, -2.2f}, {-arena.x + 1.0f, floorY + 0.75f, -0.9f});
            for (const float x : {-0.9f, 0.9f})
            {
                const float z = arena.y - 0.55f;
                box(furniture.mesh, {x - 0.24f, floorY + 0.42f, z - 0.22f}, {x + 0.24f, floorY + 0.50f, z + 0.22f});
                box(furniture.mesh, {x - 0.24f, floorY + 0.50f, z + 0.16f}, {x + 0.24f, floorY + 1.00f, z + 0.22f});
                for (const float dx : {-0.2f, 0.2f})
                {
                    for (const float dz : {-0.18f, 0.18f})
                    {
                        box(furniture.mesh, {x + dx - 0.02f, floorY, z + dz - 0.02f}, {x + dx + 0.02f, floorY + 0.42f, z + dz + 0.02f});
                    }
                }
            }

            // The canopy light: a long slim box hung over the table, with
            // three glowing panels underneath (the lamps sit just below them).
            HallPart canopy {"Canopy", HallMaterial::CanopyBody, {}, false};
            HallPart panels {"Lamp Panels", HallMaterial::LampPanel, {}, false};
            HallPart cables {"Canopy Cables", HallMaterial::Cable, {}, false};
            const float canopyLength = 2.0f * table.x * 0.95f;
            const float bottom = lampY + 0.03f;
            box(canopy.mesh, {-0.5f * canopyLength, bottom, -0.28f}, {0.5f * canopyLength, bottom + 0.11f, 0.28f});
            for (const glm::vec3& lamp : hall.lamps)
            {
                panels.mesh.quad({lamp.x + 0.36f, bottom - 0.002f, -0.22f}, {lamp.x + 0.36f, bottom - 0.002f, 0.22f},
                                 {lamp.x - 0.36f, bottom - 0.002f, 0.22f}, {lamp.x - 0.36f, bottom - 0.002f, -0.22f});
            }
            for (const float x : {-0.45f * canopyLength, 0.45f * canopyLength})
            {
                for (const float z : {-0.24f, 0.24f})
                {
                    box(cables.mesh, {x - 0.004f, bottom + 0.11f, z - 0.004f}, {x + 0.004f, floorY + 7.0f, z + 0.004f});
                }
            }

            for (HallPart* part : {&carpet, &playing, &barriers, &trim, &stands, &seats, &walls, &furniture, &canopy, &panels, &cables})
            {
                hall.parts.push_back(std::move(*part));
            }
            return hall;
        }

        HallGeometry buildSaloon(const glm::vec2& table, float floorY)
        {
            (void)table;
            HallGeometry hall;
            const float lampY = 1.58f;
            hall.lamps = {glm::vec3(-0.82f, lampY, 0.0f), glm::vec3(0.0f, lampY + 0.06f, 0.0f), glm::vec3(0.82f, lampY, 0.0f)};

            const glm::vec2 room(3.8f, 2.9f);
            const float ceiling = floorY + 3.1f;

            HallPart floorPart {"Saloon Floor", HallMaterial::WoodFloor, {}, false};
            floorQuad(floorPart.mesh, -room.x, -room.y, room.x, room.y, floorY, 1.2f);

            HallPart walls {"Saloon Walls", HallMaterial::Wall, {}, false};
            HallPart panelling {"Wainscot", HallMaterial::WoodPanel, {}, false};
            const glm::vec2 corners[4] = {{-room.x, -room.y}, {room.x, -room.y}, {room.x, room.y}, {-room.x, room.y}};
            for (int i = 0; i < 4; ++i)
            {
                const glm::vec2 a = corners[i];
                const glm::vec2 b = corners[(i + 1) % 4];
                wallQuad(walls.mesh, a, b, floorY + 1.05f, ceiling, {0.0f, 0.0f});
                // Wood panelling below a dado rail, standing a little proud of the wall.
                const glm::vec2 inward = -glm::normalize(0.5f * (a + b)) * 0.0f;
                const glm::vec2 along = glm::normalize(b - a);
                const glm::vec2 normal(along.y, -along.x);
                const glm::vec2 in = (glm::dot(normal, -(a + b)) > 0.0f) ? normal : -normal;
                const glm::vec2 a2 = a + in * 0.03f + inward;
                const glm::vec2 b2 = b + in * 0.03f + inward;
                wallQuad(panelling.mesh, a2, b2, floorY, floorY + 1.05f, {0.0f, 0.0f});
                // Dado rail.
                const glm::vec2 lo = glm::min(a, b + in * 0.06f);
                const glm::vec2 hi = glm::max(a + in * 0.06f, b);
                box(panelling.mesh, {std::min(lo.x, hi.x), floorY + 1.02f, std::min(lo.y, hi.y)},
                                    {std::max(lo.x, hi.x), floorY + 1.08f, std::max(lo.y, hi.y)});
            }

            HallPart ceilingPart {"Ceiling", HallMaterial::Wall, {}, false};
            ceilingPart.mesh.quad({-room.x, ceiling, -room.y}, {room.x, ceiling, -room.y}, {room.x, ceiling, room.y}, {-room.x, ceiling, room.y});

            // Cords up to the ceiling for the globe lamps.
            HallPart cables {"Lamp Cords", HallMaterial::Cable, {}, false};
            for (const glm::vec3& lamp : hall.lamps)
            {
                box(cables.mesh, {lamp.x - 0.004f, lamp.y + 0.04f, lamp.z - 0.004f}, {lamp.x + 0.004f, ceiling, lamp.z + 0.004f});
            }

            // The globes themselves (drawn as lamp panels).
            HallPart globes {"Lamp Globes", HallMaterial::LampPanel, {}, false};
            constexpr int SEGMENTS = 20;
            for (const glm::vec3& lamp : hall.lamps)
            {
                const float r = 0.09f;
                for (int i = 0; i < SEGMENTS; ++i)
                {
                    for (int j = 0; j < SEGMENTS / 2; ++j)
                    {
                        const auto point = [&](int u, int v)
                        {
                            const float theta = 2.0f * PI * static_cast<float>(u) / SEGMENTS;
                            const float phi = PI * static_cast<float>(v) / (SEGMENTS / 2);
                            const glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
                            return MeshVertex{lamp + glm::vec3(0.0f, 0.06f, 0.0f) + n * r, n, glm::vec2(0.0f)};
                        };
                        const MeshVertex a = point(i, j), b = point(i + 1, j), c = point(i + 1, j + 1), d = point(i, j + 1);
                        globes.mesh.triangle(a, b, c);
                        globes.mesh.triangle(a, c, d);
                    }
                }
            }

            for (HallPart* part : {&floorPart, &walls, &panelling, &ceilingPart, &cables, &globes})
            {
                hall.parts.push_back(std::move(*part));
            }
            return hall;
        }
    }

    std::array<glm::vec3, 3> hallLamps(HallLayout layout)
    {
        return buildHall(layout, glm::vec2(1.45f, 0.81f), -0.775f).lamps;
    }

    HallLayout hallLayoutFromName(const std::string& name)
    {
        return (name == "saloon") ? HallLayout::Saloon : HallLayout::Arena;
    }

    HallGeometry buildHall(HallLayout layout, const glm::vec2& tableHalfExtent, float floorY)
    {
        return (layout == HallLayout::Saloon) ? buildSaloon(tableHalfExtent, floorY) : buildArena(tableHalfExtent, floorY);
    }
}
