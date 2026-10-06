#include "scene/hall_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <vector>

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

        // WPA: lamps at least 40 in above the bed.
        constexpr float ARENA_LAMP_Y = 1.02f;
        constexpr float SALOON_LAMP_Y = 1.58f;

        std::array<glm::vec3, 3> lampPositions(HallLayout layout)
        {
            if (layout == HallLayout::Saloon)
            {
                return {glm::vec3(-0.82f, SALOON_LAMP_Y, 0.0f), glm::vec3(0.0f, SALOON_LAMP_Y + 0.06f, 0.0f), glm::vec3(0.82f, SALOON_LAMP_Y, 0.0f)};
            }
            return {glm::vec3(-0.92f, ARENA_LAMP_Y, 0.0f), glm::vec3(0.0f, ARENA_LAMP_Y, 0.0f), glm::vec3(0.92f, ARENA_LAMP_Y, 0.0f)};
        }

        HallGeometry buildArena(const glm::vec2& table, float floorY)
        {
            HallGeometry hall;
            const float lampY = ARENA_LAMP_Y;
            hall.lamps = lampPositions(HallLayout::Arena);

            // The playing area inside the barriers.
            const glm::vec2 arena(3.9f, 2.7f);

            HallPart carpet {"Arena Carpet", HallMaterial::Carpet, {}, false, std::nullopt};
            floorQuad(carpet.mesh, -14.0f, -14.0f, 14.0f, 14.0f, floorY, 0.5f);
            HallPart playing {"Playing Area", HallMaterial::PlayingCarpet, {}, false, std::nullopt};
            floorQuad(playing.mesh, -arena.x, -arena.y, arena.x, arena.y, floorY + 0.003f, 0.5f);

            // Barrier boards with a gold cap line, open at the head end for the players.
            HallPart barriers {"Barriers", HallMaterial::Barrier, {}, true, std::nullopt};
            HallPart trim {"Barrier Trim", HallMaterial::GoldTrim, {}, false, std::nullopt};
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
            // One part per side, so a side behind the camera is culled.
            HallPart stands[3] = {{"Stands North", HallMaterial::Stand, {}, false, std::nullopt},
                                  {"Stands South", HallMaterial::Stand, {}, false, std::nullopt},
                                  {"Stands Foot", HallMaterial::Stand, {}, false, std::nullopt}};
            HallPart seats[3] = {{"Seats North", HallMaterial::Seat, {}, false, std::nullopt},
                                 {"Seats South", HallMaterial::Seat, {}, false, std::nullopt},
                                 {"Seats Foot", HallMaterial::Seat, {}, false, std::nullopt}};
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
                box(stands[0].mesh, {-arena.x - 1.0f, floorY, arena.y + near}, {arena.x + 1.0f, y, arena.y + far});
                box(stands[1].mesh, {-arena.x - 1.0f, floorY, -arena.y - far}, {arena.x + 1.0f, y, -arena.y - near});
                box(stands[2].mesh, {arena.x + near, floorY, -arena.y - 0.2f}, {arena.x + far, y, arena.y + 0.2f});

                const float seatDepth = 0.42f;
                const auto seatRow = [&](HallPart& row, float from, float to, bool alongX, float line, float back)
                {
                    for (float s = from; s + 0.48f <= to; s += 0.55f)
                    {
                        const float a = s;
                        const float b = s + 0.48f;
                        if (alongX)
                        {
                            const float z0 = std::min(line, line + back * seatDepth);
                            const float z1 = std::max(line, line + back * seatDepth);
                            box(row.mesh, {a, y, z0}, {b, y + 0.12f, z1});                                // seat
                            const float bz = line + back * seatDepth;
                            box(row.mesh, {a, y, std::min(bz, bz + back * 0.06f)}, {b, y + 0.48f, std::max(bz, bz + back * 0.06f)});   // back
                        }
                        else
                        {
                            const float x0 = std::min(line, line + back * seatDepth);
                            const float x1 = std::max(line, line + back * seatDepth);
                            box(row.mesh, {x0, y, a}, {x1, y + 0.12f, b});
                            const float bx = line + back * seatDepth;
                            box(row.mesh, {std::min(bx, bx + back * 0.06f), y, a}, {std::max(bx, bx + back * 0.06f), y + 0.48f, b});
                        }
                    }
                };
                seatRow(seats[0], -arena.x - 0.8f, arena.x + 0.8f, true, arena.y + near + 0.2f, 1.0f);
                seatRow(seats[1], -arena.x - 0.8f, arena.x + 0.8f, true, -arena.y - near - 0.2f, -1.0f);
                seatRow(seats[2], -arena.y, arena.y, false, arena.x + near + 0.2f, 1.0f);
            }

            // Dark walls closing the hall behind the stands and at the head end.
            HallPart walls {"Hall Walls", HallMaterial::Wall, {}, false, std::nullopt};
            const float wallX = arena.x + gap + depth * TIERS + 0.6f;
            const float wallZ = arena.y + gap + depth * TIERS + 0.6f;
            const float wallTop = floorY + 7.0f;
            wallQuad(walls.mesh, {-wallX, wallZ}, {wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {-wallX, -wallZ}, {wallX, -wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {wallX, -wallZ}, {wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});
            wallQuad(walls.mesh, {-wallX, -wallZ}, {-wallX, wallZ}, floorY, wallTop, {0.0f, 0.0f});

            // The scorer's desk at the head end and two players' chairs.
            HallPart furniture {"Officials", HallMaterial::Furniture, {}, true, std::nullopt};
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
            HallPart canopy {"Canopy", HallMaterial::CanopyBody, {}, false, std::nullopt};
            HallPart panels {"Lamp Panels", HallMaterial::LampPanel, {}, false, std::nullopt};
            HallPart cables {"Canopy Cables", HallMaterial::Cable, {}, false, std::nullopt};
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

            // A gold LED line along the inside of the barriers.
            HallPart led {"Barrier LED", HallMaterial::LedStrip, {}, false, std::nullopt};
            const float ledY0 = h - 0.1f;
            const float ledY1 = h - 0.07f;
            box(led.mesh, {-arena.x, ledY0, arena.y - 0.006f}, {arena.x, ledY1, arena.y});
            box(led.mesh, {-arena.x, ledY0, -arena.y}, {arena.x, ledY1, -arena.y + 0.006f});
            box(led.mesh, {arena.x - 0.006f, ledY0, -arena.y}, {arena.x, ledY1, arena.y});

            // A lighting truss over the arena. Its spots wash the three stands
            // and fill the playing area, so the hall reads around the table.
            HallPart truss {"Lighting Truss", HallMaterial::CanopyBody, {}, false, std::nullopt};
            HallPart fixtures {"Truss Fixtures", HallMaterial::LampPanel, {}, false, std::nullopt};
            const float trussY = floorY + 6.2f;
            const glm::vec2 ring(arena.x + 1.6f, arena.y + 1.6f);
            box(truss.mesh, {-ring.x, trussY, ring.y - 0.15f}, {ring.x, trussY + 0.3f, ring.y + 0.15f});
            box(truss.mesh, {-ring.x, trussY, -ring.y - 0.15f}, {ring.x, trussY + 0.3f, -ring.y + 0.15f});
            box(truss.mesh, {ring.x - 0.15f, trussY, -ring.y}, {ring.x + 0.15f, trussY + 0.3f, ring.y});
            box(truss.mesh, {-ring.x - 0.15f, trussY, -ring.y}, {-ring.x + 0.15f, trussY + 0.3f, ring.y});
            const glm::vec3 stage(1.0f, 0.93f, 0.84f);
            const auto spot = [&](const glm::vec3& at, const glm::vec3& target, float intensity, float range, float outerDegrees)
            {
                box(fixtures.mesh, at - glm::vec3(0.12f, 0.1f, 0.12f), at + glm::vec3(0.12f, 0.0f, 0.12f));
                HallLight light;
                light.position = at - glm::vec3(0.0f, 0.12f, 0.0f);
                light.color = stage;
                light.intensity = intensity;
                light.range = range;
                light.direction = glm::normalize(target - light.position);
                light.spotOuterCos = std::cos(glm::radians(outerDegrees));
                light.spotInnerCos = std::cos(glm::radians(outerDegrees * 0.6f));
                hall.lights.push_back(light);
            };
            const float standMiddle = gap + depth * 3.5f;
            const float standY = floorY + rise * 4.0f;
            for (const float x : {-3.0f, 0.0f, 3.0f})
            {
                spot({x, trussY, ring.y}, {x, standY, arena.y + standMiddle}, 9.0f, 11.0f, 45.0f);
                spot({x, trussY, -ring.y}, {x, standY, -arena.y - standMiddle}, 9.0f, 11.0f, 45.0f);
            }
            for (const float z : {-2.0f, 0.0f, 2.0f})
            {
                spot({ring.x, trussY, z}, {arena.x + standMiddle, standY, z}, 9.0f, 11.0f, 45.0f);
            }
            for (const glm::vec2& corner : {glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, 1.0f)})
            {
                const glm::vec3 at(corner.x * ring.x, trussY, corner.y * ring.y);
                spot(at, {corner.x * 2.2f, floorY, corner.y * 1.6f}, 16.0f, 10.0f, 50.0f);
            }

            // Probes over the playing area and the lower stands.
            hall.probeMin = glm::vec3(-arena.x - 2.5f, floorY + 0.25f, -arena.y - 2.5f);
            hall.probeMax = glm::vec3(arena.x + 2.5f, floorY + 3.2f, arena.y + 2.5f);
            hall.probeCounts = glm::ivec3(9, 3, 7);
            hall.roomMin = glm::vec3(-wallX, floorY, -wallZ);
            hall.roomMax = glm::vec3(wallX, wallTop, wallZ);
            hall.reflectionCentre = glm::vec3(0.0f, 0.7f, 0.0f);

            for (HallPart* part : {&carpet, &playing, &barriers, &trim, &led, &stands[0], &stands[1], &stands[2], &seats[0], &seats[1], &seats[2],
                                   &walls, &furniture, &canopy, &panels, &cables, &truss, &fixtures})
            {
                hall.parts.push_back(std::move(*part));
            }
            return hall;
        }

        // ---- Saloon helpers ----------------------------------------------------------

        // A vertical cylinder (or cone frustum) with caps, from y0 to y1.
        void cylinder(MeshData& mesh, const glm::vec3& base, float r0, float r1, float height, int segments = 12)
        {
            const float slope = (r0 - r1) / std::max(height, 1.0e-4f);
            for (int i = 0; i < segments; ++i)
            {
                const float a0 = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
                const float a1 = 2.0f * PI * static_cast<float>(i + 1) / static_cast<float>(segments);
                const glm::vec3 d0(std::cos(a0), 0.0f, std::sin(a0));
                const glm::vec3 d1(std::cos(a1), 0.0f, std::sin(a1));
                const glm::vec3 n0 = glm::normalize(d0 + glm::vec3(0.0f, slope, 0.0f));
                const glm::vec3 n1 = glm::normalize(d1 + glm::vec3(0.0f, slope, 0.0f));
                const glm::vec3 top = base + glm::vec3(0.0f, height, 0.0f);
                const MeshVertex b0 {base + d0 * r0, n0, {0.0f, 0.0f}};
                const MeshVertex b1 {base + d1 * r0, n1, {1.0f, 0.0f}};
                const MeshVertex t1 {top + d1 * r1, n1, {1.0f, 1.0f}};
                const MeshVertex t0 {top + d0 * r1, n0, {0.0f, 1.0f}};
                mesh.triangle(b0, t1, b1);
                mesh.triangle(b0, t0, t1);
                if (r1 > 0.0f)
                {
                    const glm::vec3 up(0.0f, 1.0f, 0.0f);
                    mesh.triangle({top, up, {0.5f, 0.5f}}, {top + d1 * r1, up, {0.0f, 0.0f}}, {top + d0 * r1, up, {0.0f, 0.0f}});
                }
                if (r0 > 0.0f)
                {
                    const glm::vec3 down(0.0f, -1.0f, 0.0f);
                    mesh.triangle({base, down, {0.5f, 0.5f}}, {base + d0 * r0, down, {0.0f, 0.0f}}, {base + d1 * r0, down, {0.0f, 0.0f}});
                }
            }
        }

        // A wall's own frame: `along` runs over the wall, `inward` into the room.
        struct WallFrame
        {
            bool alongX {true};
            float plane {0.0f};    // z (alongX) or x of the wall surface
            float inward {1.0f};   // +1 or -1

            [[nodiscard]] glm::vec3 at(float along, float y, float depth) const
            {
                const float c = plane + inward * depth;
                return alongX ? glm::vec3(along, y, c) : glm::vec3(c, y, along);
            }
        };

        void boxOn(MeshData& mesh, const WallFrame& w, float a0, float a1, float y0, float y1, float d0, float d1)
        {
            const glm::vec3 p = w.at(a0, y0, d0);
            const glm::vec3 q = w.at(a1, y1, d1);
            box(mesh, glm::min(p, q), glm::max(p, q));
        }

        // A flat face on the wall at `depth`, facing into the room.
        void faceOn(MeshData& mesh, const WallFrame& w, float a0, float a1, float y0, float y1, float depth, float uvPerMetre = 1.0f)
        {
            glm::vec3 p0 = w.at(a0, y0, depth), p1 = w.at(a1, y0, depth), p2 = w.at(a1, y1, depth), p3 = w.at(a0, y1, depth);
            const glm::vec3 inwards = w.at(0.0f, 0.0f, 1.0f) - w.at(0.0f, 0.0f, 0.0f);
            if (glm::dot(glm::cross(p1 - p0, p3 - p0), inwards) < 0.0f)
            {
                std::swap(p0, p1);
                std::swap(p2, p3);
            }
            mesh.quad(p0, p1, p2, p3, glm::vec2(std::abs(a1 - a0), y1 - y0) * uvPerMetre);
        }

        struct Opening
        {
            float a0;
            float a1;
            float y0;
            float y1;
        };

        // Wainscot, dado rail, skirting, plaster and crown moulding along one wall,
        // leaving `openings` (windows, door) clear.
        void dressWall(HallPart& panels, HallPart& plaster, const WallFrame& w, float a0, float a1,
                       float floorY, float ceilingY, const std::vector<Opening>& openings)
        {
            const float dado = floorY + 1.0f;
            const auto blocked = [&](float a, float y)
            {
                for (const Opening& o : openings)
                {
                    if ((a > o.a0) && (a < o.a1) && (y > o.y0) && (y < o.y1))
                    {
                        return true;
                    }
                }
                return false;
            };

            // Cut the wall into vertical strips at every opening edge.
            std::vector<float> cuts {a0, a1};
            for (const Opening& o : openings)
            {
                cuts.push_back(std::clamp(o.a0, a0, a1));
                cuts.push_back(std::clamp(o.a1, a0, a1));
            }
            std::sort(cuts.begin(), cuts.end());
            for (std::size_t i = 0; i + 1 < cuts.size(); ++i)
            {
                const float s0 = cuts[i];
                const float s1 = cuts[i + 1];
                if (s1 - s0 < 1.0e-3f)
                {
                    continue;
                }
                const float mid = 0.5f * (s0 + s1);
                // Rows: wainscot (floor..dado) and plaster (dado..ceiling),
                // each split around openings that overlap this strip.
                std::vector<float> rows {floorY, dado, ceilingY};
                for (const Opening& o : openings)
                {
                    if ((mid > o.a0) && (mid < o.a1))
                    {
                        rows.push_back(o.y0);
                        rows.push_back(o.y1);
                    }
                }
                std::sort(rows.begin(), rows.end());
                for (std::size_t r = 0; r + 1 < rows.size(); ++r)
                {
                    const float y0 = std::max(rows[r], floorY);
                    const float y1 = std::min(rows[r + 1], ceilingY);
                    if ((y1 - y0 < 1.0e-3f) || blocked(mid, 0.5f * (y0 + y1)))
                    {
                        continue;
                    }
                    if (y1 <= dado + 1.0e-4f)
                    {
                        faceOn(panels.mesh, w, s0, s1, y0, y1, 0.025f);
                    }
                    else
                    {
                        faceOn(plaster.mesh, w, s0, s1, y0, y1, 0.0f, 0.5f);
                    }
                }
            }

            // Raised panel frames on the wainscot: stiles every ~0.9 m, top and bottom rails.
            const float length = a1 - a0;
            const int bays = std::max(1, static_cast<int>(std::round(length / 0.9f)));
            for (int b = 0; b <= bays; ++b)
            {
                const float a = a0 + length * static_cast<float>(b) / static_cast<float>(bays);
                if (!blocked(a, floorY + 0.5f))
                {
                    boxOn(panels.mesh, w, std::max(a - 0.04f, a0), std::min(a + 0.04f, a1), floorY + 0.14f, dado - 0.04f, 0.0f, 0.04f);
                }
            }
            for (std::size_t i = 0; i + 1 < cuts.size(); ++i)
            {
                const float s0 = cuts[i];
                const float s1 = cuts[i + 1];
                if ((s1 - s0 < 1.0e-3f) || blocked(0.5f * (s0 + s1), floorY + 0.5f))
                {
                    continue;
                }
                boxOn(panels.mesh, w, s0, s1, floorY, floorY + 0.14f, 0.0f, 0.045f);            // skirting
                boxOn(panels.mesh, w, s0, s1, floorY + 0.14f, floorY + 0.2f, 0.0f, 0.04f);      // bottom rail
                boxOn(panels.mesh, w, s0, s1, dado - 0.1f, dado - 0.03f, 0.0f, 0.04f);          // top rail
                if (!blocked(0.5f * (s0 + s1), dado + 0.01f))
                {
                    boxOn(panels.mesh, w, s0, s1, dado - 0.03f, dado + 0.04f, 0.0f, 0.065f);    // dado rail
                }
            }
            boxOn(panels.mesh, w, a0, a1, ceilingY - 0.14f, ceilingY, 0.0f, 0.1f);              // crown moulding
        }

        HallGeometry buildSaloon(const glm::vec2& table, float floorY)
        {
            (void)table;
            HallGeometry hall;
            hall.lamps = lampPositions(HallLayout::Saloon);

            const float RX = 4.6f;
            const float RZ = 3.4f;
            const float ceiling = floorY + 3.3f;
            const glm::vec3 warm(1.0f, 0.74f, 0.47f);
            const glm::vec3 evening(0.62f, 0.68f, 0.85f);

            const auto part = [](const char* name, HallMaterial material, bool shadow = false)
            {
                HallPart p;
                p.name = name;
                p.material = material;
                p.castsShadow = shadow;
                return p;
            };
            std::vector<HallPart> parts;
            const auto keep = [&](HallPart&& p) { parts.push_back(std::move(p)); };

            // Floor and ceiling.
            HallPart floorPart = part("Saloon Floor", HallMaterial::WoodFloor);
            floorQuad(floorPart.mesh, -RX, -RZ, RX, RZ, floorY, 1.2f);
            keep(std::move(floorPart));

            HallPart ceilingPart = part("Ceiling", HallMaterial::Ceiling);
            ceilingPart.mesh.quad({-RX, ceiling, RZ}, {RX, ceiling, RZ}, {RX, ceiling, -RZ}, {-RX, ceiling, -RZ}, glm::vec2(2.0f * RX, 2.0f * RZ));
            HallPart beams = part("Ceiling Beams", HallMaterial::Beam);
            for (const float x : {-3.45f, -1.15f, 1.15f, 3.45f})
            {
                box(beams.mesh, {x - 0.11f, ceiling - 0.24f, -RZ}, {x + 0.11f, ceiling, RZ});
            }
            box(beams.mesh, {-RX, ceiling - 0.16f, -0.08f}, {RX, ceiling, 0.08f});
            keep(std::move(ceilingPart));
            keep(std::move(beams));

            // The four walls, each its own parts so they can be culled.
            const WallFrame windowWall {true, RZ, -1.0f};     // +z: three windows
            const WallFrame rackWall {true, -RZ, 1.0f};       // -z: cue rack and pictures
            const WallFrame barWall {false, RX, -1.0f};       // +x: the bar
            const WallFrame doorWall {false, -RX, 1.0f};      // -x: door and bench

            const std::vector<Opening> windows {
                {-3.15f, -2.05f, floorY + 1.08f, floorY + 2.7f},
                {-0.55f, 0.55f, floorY + 1.08f, floorY + 2.7f},
                {2.05f, 3.15f, floorY + 1.08f, floorY + 2.7f}};
            const std::vector<Opening> door {{-0.55f, 0.55f, floorY, floorY + 2.2f}};

            const auto dressed = [&](const char* panelName, const char* plasterName, const WallFrame& w, float extent, const std::vector<Opening>& openings)
            {
                HallPart panels = part(panelName, HallMaterial::WoodPanel);
                HallPart plaster = part(plasterName, HallMaterial::Plaster);
                dressWall(panels, plaster, w, -extent, extent, floorY, ceiling, openings);
                keep(std::move(panels));
                keep(std::move(plaster));
            };
            dressed("Window Wall Panels", "Window Wall", windowWall, RX, windows);
            dressed("Rack Wall Panels", "Rack Wall", rackWall, RX, {});
            dressed("Bar Wall Panels", "Bar Wall", barWall, RZ, {});
            dressed("Door Wall Panels", "Door Wall", doorWall, RZ, door);

            // Windows: frames, a sill, mullions, glowing panes, evening light coming in.
            HallPart frames = part("Window Frames", HallMaterial::Beam);
            HallPart panes = part("Window Panes", HallMaterial::WindowGlow);
            for (const Opening& o : windows)
            {
                faceOn(panes.mesh, windowWall, o.a0, o.a1, o.y0, o.y1, -0.06f);
                boxOn(frames.mesh, windowWall, o.a0 - 0.07f, o.a1 + 0.07f, o.y0 - 0.06f, o.y0, -0.06f, 0.1f);   // sill
                boxOn(frames.mesh, windowWall, o.a0 - 0.07f, o.a1 + 0.07f, o.y1, o.y1 + 0.08f, -0.06f, 0.05f);
                boxOn(frames.mesh, windowWall, o.a0 - 0.07f, o.a0, o.y0, o.y1, -0.06f, 0.05f);
                boxOn(frames.mesh, windowWall, o.a1, o.a1 + 0.07f, o.y0, o.y1, -0.06f, 0.05f);
                const float mid = 0.5f * (o.a0 + o.a1);
                boxOn(frames.mesh, windowWall, mid - 0.02f, mid + 0.02f, o.y0, o.y1, -0.06f, -0.02f);
                for (const float t : {0.33f, 0.66f})
                {
                    const float y = o.y0 + t * (o.y1 - o.y0);
                    boxOn(frames.mesh, windowWall, o.a0, o.a1, y - 0.02f, y + 0.02f, -0.06f, -0.02f);
                }
                HallLight light;
                light.position = windowWall.at(mid, 0.5f * (o.y0 + o.y1), 0.35f);
                light.color = evening;
                light.intensity = 0.9f;
                light.range = 4.5f;
                light.direction = glm::normalize(windowWall.at(mid, floorY, 2.0f) - light.position);
                light.spotOuterCos = std::cos(glm::radians(80.0f));
                light.spotInnerCos = std::cos(glm::radians(45.0f));
                hall.lights.push_back(light);
            }
            keep(std::move(frames));
            keep(std::move(panes));

            // Door: a panelled leaf in a frame, brass handle.
            HallPart doorPart = part("Door", HallMaterial::Beam);
            HallPart brass = part("Brass Fittings", HallMaterial::Brass);
            faceOn(doorPart.mesh, doorWall, -0.55f, 0.55f, floorY, floorY + 2.2f, 0.01f);
            for (const float a : {-0.62f, 0.55f})
            {
                boxOn(doorPart.mesh, doorWall, a, a + 0.07f, floorY, floorY + 2.27f, 0.0f, 0.06f);
            }
            boxOn(doorPart.mesh, doorWall, -0.62f, 0.62f, floorY + 2.2f, floorY + 2.27f, 0.0f, 0.06f);
            for (const float a : {-0.42f, 0.06f})
            {
                for (const float y : {0.25f, 1.2f})
                {
                    boxOn(doorPart.mesh, doorWall, a, a + 0.36f, floorY + y, floorY + y + 0.8f, 0.01f, 0.03f);
                }
            }
            boxOn(brass.mesh, doorWall, 0.38f, 0.48f, floorY + 1.02f, floorY + 1.06f, 0.03f, 0.09f);
            keep(std::move(doorPart));

            // ---- The bar along the foot wall ----------------------------------------
            HallPart counter = part("Bar Counter", HallMaterial::WoodPanel, true);
            HallPart barTop = part("Bar Top", HallMaterial::BarTop, true);
            HallPart backBar = part("Back Bar", HallMaterial::Beam);
            HallPart mirror = part("Bar Mirror", HallMaterial::Mirror);
            const float counterFront = 3.25f;
            const float counterBack = 3.85f;
            const float counterHalf = 2.3f;
            box(counter.mesh, {counterFront, floorY, -counterHalf}, {counterBack, floorY + 1.02f, counterHalf});
            box(barTop.mesh, {counterFront - 0.06f, floorY + 1.02f, -counterHalf - 0.05f}, {counterBack + 0.02f, floorY + 1.07f, counterHalf + 0.05f});
            // Panelled front: stiles every 0.6 m.
            for (float z = -counterHalf; z <= counterHalf + 1.0e-3f; z += 0.575f)
            {
                box(counter.mesh, {counterFront - 0.03f, floorY + 0.12f, z - 0.035f}, {counterFront, floorY + 0.95f, z + 0.035f});
            }
            // Brass foot rail on posts.
            box(brass.mesh, {counterFront - 0.24f, floorY + 0.18f, -counterHalf}, {counterFront - 0.2f, floorY + 0.22f, counterHalf});
            for (float z = -counterHalf + 0.3f; z < counterHalf; z += 1.1f)
            {
                box(brass.mesh, {counterFront - 0.23f, floorY + 0.22f, z - 0.015f}, {counterFront, floorY + 0.25f, z + 0.015f});
            }
            // Back bar: a low cabinet, shelves on the wall, a mirror between them.
            box(backBar.mesh, {RX - 0.5f, floorY, -counterHalf}, {RX - 0.02f, floorY + 0.95f, counterHalf});
            faceOn(mirror.mesh, barWall, -1.6f, 1.6f, floorY + 1.15f, floorY + 2.05f, 0.03f);
            for (const float y : {1.2f, 1.62f, 2.04f})
            {
                for (const float side : {-1.0f, 1.0f})
                {
                    boxOn(backBar.mesh, barWall, side > 0.0f ? 1.65f : -2.3f, side > 0.0f ? 2.3f : -1.65f, floorY + y - 0.025f, floorY + y, 0.02f, 0.3f);
                }
                boxOn(backBar.mesh, barWall, -1.6f, 1.6f, floorY + y - 0.02f, floorY + y, 0.02f, 0.18f);
            }
            boxOn(backBar.mesh, barWall, -2.35f, 2.35f, floorY + 2.4f, floorY + 2.52f, 0.0f, 0.34f);   // canopy
            for (const float a : {-2.35f, -1.62f, 1.62f, 2.3f})
            {
                boxOn(backBar.mesh, barWall, a, a + 0.05f, floorY + 0.95f, floorY + 2.4f, 0.0f, 0.3f);
            }
            // Bottles on the shelves, in three kinds of glass.
            const glm::vec3 glassTints[3] = {{0.10f, 0.32f, 0.12f}, {0.55f, 0.28f, 0.06f}, {0.75f, 0.78f, 0.74f}};
            HallPart bottles[3] = {part("Bottles Green", HallMaterial::Glass), part("Bottles Amber", HallMaterial::Glass), part("Bottles Clear", HallMaterial::Glass)};
            int kind = 0;
            for (const float y : {1.2f, 1.62f, 2.04f})
            {
                for (float a = -2.22f; a < 2.25f; a += 0.13f)
                {
                    if (std::abs(a) < 1.66f && y > 1.25f)
                    {
                        continue;   // the mirror's middle shelves keep glasses only
                    }
                    const float h = 0.24f + 0.08f * static_cast<float>((kind * 7) % 3);
                    const glm::vec3 base = barWall.at(a, floorY + y, 0.14f);
                    cylinder(bottles[kind % 3].mesh, base, 0.035f, 0.035f, h * 0.68f, 10);
                    cylinder(bottles[kind % 3].mesh, base + glm::vec3(0.0f, h * 0.68f, 0.0f), 0.035f, 0.012f, h * 0.18f, 10);
                    cylinder(bottles[kind % 3].mesh, base + glm::vec3(0.0f, h * 0.86f, 0.0f), 0.012f, 0.012f, h * 0.14f, 8);
                    ++kind;
                }
            }
            for (int i = 0; i < 3; ++i)
            {
                bottles[i].tint = glassTints[i];
                keep(std::move(bottles[i]));
            }
            keep(std::move(counter));
            keep(std::move(barTop));
            keep(std::move(backBar));
            keep(std::move(mirror));

            // Pendant lights over the bar.
            HallPart shades = part("Lamp Shades", HallMaterial::SconceShade);
            HallPart cords = part("Lamp Cords", HallMaterial::Cable);
            for (const float z : {-1.5f, 0.0f, 1.5f})
            {
                const glm::vec3 at(3.55f, floorY + 2.25f, z);
                cylinder(shades.mesh, at, 0.16f, 0.06f, 0.16f, 18);
                box(cords.mesh, at + glm::vec3(-0.004f, 0.16f, -0.004f), {at.x + 0.004f, ceiling, at.z + 0.004f});
                HallLight light;
                light.position = at - glm::vec3(0.0f, 0.02f, 0.0f);
                light.color = warm;
                light.intensity = 1.1f;
                light.range = 3.2f;
                light.spotOuterCos = std::cos(glm::radians(75.0f));
                light.spotInnerCos = std::cos(glm::radians(40.0f));
                hall.lights.push_back(light);
            }

            // ---- The rack wall: cue rack, scoreboard, pictures, sconces --------------
            HallPart rack = part("Cue Rack", HallMaterial::Beam);
            HallPart cues = part("Rack Cues", HallMaterial::BarTop);
            boxOn(rack.mesh, rackWall, -0.75f, 0.75f, floorY + 0.15f, floorY + 0.3f, 0.0f, 0.12f);
            boxOn(rack.mesh, rackWall, -0.75f, 0.75f, floorY + 1.25f, floorY + 1.37f, 0.0f, 0.1f);
            for (float a = -0.6f; a <= 0.61f; a += 0.15f)
            {
                const glm::vec3 base = rackWall.at(a, floorY + 0.3f, 0.06f);
                cylinder(cues.mesh, base, 0.014f, 0.007f, 1.42f, 8);
            }
            // Scoreboard: a wooden board with wires of beads.
            boxOn(rack.mesh, rackWall, -0.85f, 0.85f, floorY + 1.62f, floorY + 2.12f, 0.0f, 0.04f);
            for (const float y : {1.77f, 1.97f})
            {
                boxOn(brass.mesh, rackWall, -0.8f, 0.8f, floorY + y - 0.003f, floorY + y + 0.003f, 0.06f, 0.066f);
                for (int bead = 0; bead < 14; ++bead)
                {
                    const float a = -0.75f + 0.05f * static_cast<float>(bead) + ((bead >= 7) ? 0.6f : 0.0f);
                    const glm::vec3 c = rackWall.at(a, floorY + y, 0.063f);
                    box(rack.mesh, c - glm::vec3(0.02f), c + glm::vec3(0.02f));
                }
            }
            keep(std::move(rack));
            keep(std::move(cues));

            // Pictures: gilt frames around tinted canvases on the long walls.
            const glm::vec3 canvasTints[6] = {{0.42f, 0.28f, 0.16f}, {0.18f, 0.26f, 0.22f}, {0.50f, 0.36f, 0.20f},
                                              {0.22f, 0.20f, 0.30f}, {0.46f, 0.18f, 0.14f}, {0.30f, 0.34f, 0.26f}};
            HallPart pictureFrames = part("Picture Frames", HallMaterial::Brass);
            int picture = 0;
            const auto hang = [&](const WallFrame& w, float a, float width, float height, float centreY)
            {
                HallPart canvas = part("Picture", HallMaterial::Canvas);
                canvas.tint = canvasTints[picture++ % 6];
                const float y0 = centreY - 0.5f * height;
                const float y1 = centreY + 0.5f * height;
                faceOn(canvas.mesh, w, a - 0.5f * width, a + 0.5f * width, y0, y1, 0.03f);
                boxOn(pictureFrames.mesh, w, a - 0.5f * width - 0.06f, a + 0.5f * width + 0.06f, y0 - 0.06f, y0, 0.0f, 0.045f);
                boxOn(pictureFrames.mesh, w, a - 0.5f * width - 0.06f, a + 0.5f * width + 0.06f, y1, y1 + 0.06f, 0.0f, 0.045f);
                boxOn(pictureFrames.mesh, w, a - 0.5f * width - 0.06f, a - 0.5f * width, y0, y1, 0.0f, 0.045f);
                boxOn(pictureFrames.mesh, w, a + 0.5f * width, a + 0.5f * width + 0.06f, y0, y1, 0.0f, 0.045f);
                keep(std::move(canvas));
            };
            hang(rackWall, -2.4f, 0.9f, 0.65f, floorY + 1.85f);
            hang(rackWall, 2.4f, 0.9f, 0.65f, floorY + 1.85f);
            hang(rackWall, -3.9f, 0.45f, 0.6f, floorY + 1.8f);
            hang(rackWall, 3.9f, 0.45f, 0.6f, floorY + 1.8f);
            hang(doorWall, 2.0f, 0.8f, 1.0f, floorY + 1.85f);

            // Wall sconces: a brass plate, an arm and a fabric shade.
            const auto sconce = [&](const WallFrame& w, float a, float y)
            {
                boxOn(brass.mesh, w, a - 0.05f, a + 0.05f, y - 0.12f, y + 0.02f, 0.0f, 0.02f);
                boxOn(brass.mesh, w, a - 0.012f, a + 0.012f, y - 0.06f, y - 0.035f, 0.02f, 0.14f);
                const glm::vec3 shadeBase = w.at(a, y - 0.04f, 0.15f);
                cylinder(shades.mesh, shadeBase, 0.09f, 0.06f, 0.15f, 16);
                HallLight light;
                light.position = shadeBase + glm::vec3(0.0f, 0.08f, 0.0f);
                light.color = warm;
                light.intensity = 0.75f;
                light.range = 3.4f;
                hall.lights.push_back(light);
            };
            for (const float a : {-3.15f, -1.6f, 1.6f, 3.15f})
            {
                sconce(rackWall, a, floorY + 2.05f);
            }
            for (const float a : {-1.3f, 1.3f, -3.95f, 3.95f})
            {
                sconce(windowWall, a, floorY + 2.05f);
            }
            for (const float a : {-1.0f, 1.0f})
            {
                sconce(doorWall, a, floorY + 2.05f);
            }

            // Seating, clocks, the dartboard and plants are loaded models
            // (assets/data/halls/props.json).

            // Ceiling fixtures over the walkways around the table.
            for (const glm::vec3& at : {glm::vec3(-3.2f, 0.0f, -2.2f), glm::vec3(-3.2f, 0.0f, 2.2f), glm::vec3(1.8f, 0.0f, -2.6f), glm::vec3(1.8f, 0.0f, 2.6f)})
            {
                const glm::vec3 fixture(at.x, ceiling - 0.55f, at.z);
                cylinder(shades.mesh, fixture, 0.22f, 0.09f, 0.18f, 20);
                box(cords.mesh, fixture + glm::vec3(-0.004f, 0.18f, -0.004f), {fixture.x + 0.004f, ceiling, fixture.z + 0.004f});
                HallLight light;
                light.position = fixture - glm::vec3(0.0f, 0.02f, 0.0f);
                light.color = warm;
                light.intensity = 1.2f;
                light.range = 4.5f;
                light.spotOuterCos = std::cos(glm::radians(85.0f));
                light.spotInnerCos = std::cos(glm::radians(50.0f));
                hall.lights.push_back(light);
            }

            // The table's own lamp: three globes on a bar, hung from the ceiling.
            HallPart globes = part("Lamp Globes", HallMaterial::LampPanel);
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
                box(cords.mesh, {lamp.x - 0.004f, lamp.y + 0.15f, lamp.z - 0.004f}, {lamp.x + 0.004f, ceiling, lamp.z + 0.004f});
            }
            keep(std::move(globes));
            keep(std::move(shades));
            keep(std::move(cords));
            keep(std::move(brass));
            keep(std::move(pictureFrames));

            // Probes about every metre, kept clear of the walls and the ceiling.
            hall.probeMin = glm::vec3(-RX + 0.25f, floorY + 0.25f, -RZ + 0.25f);
            hall.probeMax = glm::vec3(RX - 0.25f, ceiling - 0.3f, RZ - 0.25f);
            hall.probeCounts = glm::ivec3(9, 3, 7);
            hall.roomMin = glm::vec3(-RX, floorY, -RZ);
            hall.roomMax = glm::vec3(RX, ceiling, RZ);
            hall.reflectionCentre = glm::vec3(0.0f, 0.9f, 0.0f);

            hall.parts = std::move(parts);
            return hall;
        }
    }

    std::array<glm::vec3, 3> hallLamps(HallLayout layout)
    {
        return lampPositions(layout);
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
