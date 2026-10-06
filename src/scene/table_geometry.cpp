#include "scene/table_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        // Simulator plan (x across, y along, origin at a corner) to game
        // (x along, z across, centred), at height y.
        struct Frame
        {
            float length;
            float width;

            [[nodiscard]] glm::vec3 operator()(const glm::dvec2& sim, float height) const
            {
                return glm::vec3(static_cast<float>(sim.y) - 0.5f * length, height, static_cast<float>(sim.x) - 0.5f * width);
            }
        };

        // Cloth weave and wood grain are driven by uv: cloth uses the playing
        // surface's 0..1 range; wood runs its grain along u.
        glm::vec2 clothUv(const glm::vec3& p, const Frame& frame)
        {
            return glm::vec2(p.x / frame.length + 0.5f, p.z / frame.width + 0.5f);
        }

        // A triangle that faces up (+y), whatever order the points come in.
        void upTriangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, const Frame& frame)
        {
            glm::vec3 n = glm::cross(b - a, c - a);
            if (n.y < 0.0f)
            {
                std::swap(b, c);
                n = -n;
            }
            if (glm::length(n) < 1.0e-12f)
            {
                return;
            }
            n = glm::normalize(n);
            mesh.triangle({a, n, clothUv(a, frame)}, {b, n, clothUv(b, frame)}, {c, n, clothUv(c, frame)});
        }

        // A wall quad from a to b between the given heights, facing away from `inside`.
        void wall(MeshData& mesh, const glm::vec3& a, const glm::vec3& b, float bottom, const glm::vec3& inside, const Frame& frame)
        {
            glm::vec3 a0(a.x, bottom, a.z), b0(b.x, bottom, b.z), a1 = a, b1 = b;
            glm::vec3 n = glm::cross(b0 - a0, a1 - a0);
            const glm::vec3 mid = 0.25f * (a0 + b0 + a1 + b1);
            if (glm::dot(n, mid - inside) < 0.0f)
            {
                std::swap(a0, b0);
                std::swap(a1, b1);
                n = -n;
            }
            if (glm::length(n) < 1.0e-12f)
            {
                return;
            }
            n = glm::normalize(n);
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({a0, n, clothUv(a0, frame)});
            mesh.vertices.push_back({b0, n, clothUv(b0, frame)});
            mesh.vertices.push_back({b1, n, clothUv(b1, frame)});
            mesh.vertices.push_back({a1, n, clothUv(a1, frame)});
            mesh.indices.insert(mesh.indices.end(), {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
        }

        // An axis-aligned box, all faces outward; uv along the longer side.
        void box(MeshData& mesh, const glm::vec3& lo, const glm::vec3& hi, bool bottom = false)
        {
            const glm::vec3 c[8] = {
                {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
            const glm::vec2 s(std::max(hi.x - lo.x, hi.z - lo.z) * 2.0f, 1.0f);
            mesh.quad(c[4], c[5], c[6], c[7], s);   // +z
            mesh.quad(c[1], c[0], c[3], c[2], s);   // -z
            mesh.quad(c[5], c[1], c[2], c[6], s);   // +x
            mesh.quad(c[0], c[4], c[7], c[3], s);   // -x
            mesh.quad(c[3], c[7], c[6], c[2], s);   // +y
            if (bottom)
            {
                mesh.quad(c[0], c[1], c[5], c[4], s);
            }
        }

        double angleOf(const glm::dvec2& v)
        {
            return std::atan2(v.y, v.x);
        }

        // Points on the shorter arc of a circle from a to b (both on the circle).
        std::vector<glm::dvec2> arcPoints(const glm::dvec2& centre, const glm::dvec2& a, const glm::dvec2& b, int steps)
        {
            const double radius = glm::length(a - centre);
            const double a0 = angleOf(a - centre);
            double delta = angleOf(b - centre) - a0;
            while (delta > PI) delta -= 2.0 * PI;
            while (delta < -PI) delta += 2.0 * PI;
            std::vector<glm::dvec2> points;
            for (int i = 1; i < steps; ++i)
            {
                const double t = a0 + delta * static_cast<double>(i) / static_cast<double>(steps);
                points.push_back(centre + radius * glm::dvec2(std::cos(t), std::sin(t)));
            }
            return points;
        }

        glm::dvec2 plan(const glm::dvec3& p)
        {
            return glm::dvec2(p.x, p.y);
        }

        // The cushion between two pockets, as a convex outline in plan:
        // jaw back, jaw front, rounded tip, nose line, tip, jaw front, jaw back.
        struct CushionRun
        {
            std::vector<glm::dvec2> outline;
            std::vector<bool> back;   // vertex on the rail side (higher top)
        };

        CushionRun cushionRun(const Sim::Table& table, std::size_t run)
        {
            const Sim::LinearCushion& nose = table.linear[run];
            const std::array<glm::dvec2, 2> ends {plan(nose.p1), plan(nose.p2)};

            CushionRun result;
            for (std::size_t side = 0; side < 2; ++side)
            {
                const glm::dvec2 end = ends[side];

                // The jaw tip touching this end of the nose line...
                const Sim::CircularCushion* tip = nullptr;
                double best = std::numeric_limits<double>::max();
                for (const Sim::CircularCushion& circle : table.circular)
                {
                    const double d = std::abs(glm::length(plan(circle.center) - end) - circle.radius);
                    if (d < best)
                    {
                        best = d;
                        tip = &circle;
                    }
                }

                // ...and the jaw face that ends on that tip (jaws run back to front).
                const Sim::LinearCushion* jaw = nullptr;
                best = std::numeric_limits<double>::max();
                for (std::size_t i = 6; i < table.linear.size(); ++i)
                {
                    const double d = std::abs(glm::length(plan(table.linear[i].p2) - plan(tip->center)) - tip->radius);
                    if (d < best)
                    {
                        best = d;
                        jaw = &table.linear[i];
                    }
                }

                const std::vector<glm::dvec2> arc = arcPoints(plan(tip->center), plan(jaw->p2), end, 8);
                std::vector<glm::dvec2> piece {plan(jaw->p1), plan(jaw->p2)};
                piece.insert(piece.end(), arc.begin(), arc.end());
                piece.push_back(end);
                std::vector<bool> isBack(piece.size(), false);
                isBack.front() = true;

                if (side == 1)
                {
                    std::reverse(piece.begin(), piece.end());
                    std::reverse(isBack.begin(), isBack.end());
                }
                result.outline.insert(result.outline.end(), piece.begin(), piece.end());
                result.back.insert(result.back.end(), isBack.begin(), isBack.end());
            }
            return result;
        }

        void buildCushion(MeshData& mesh, const CushionRun& run, const Frame& frame, const TableStyle& style)
        {
            std::vector<glm::vec3> top;
            glm::vec3 centre(0.0f);
            for (std::size_t i = 0; i < run.outline.size(); ++i)
            {
                top.push_back(frame(run.outline[i], run.back[i] ? style.cushionTop : style.noseTop));
                centre += top.back();
            }
            centre /= static_cast<float>(top.size());
            const glm::vec3 inside(centre.x, 0.5f * style.noseTop, centre.z);

            for (std::size_t i = 0; i < top.size(); ++i)
            {
                const std::size_t j = (i + 1) % top.size();
                upTriangle(mesh, centre, top[i], top[j], frame);
                // The back edge (jaw back to jaw back) is hidden against the rail.
                if (!(run.back[i] && run.back[j]))
                {
                    wall(mesh, top[i], top[j], 0.0f, inside, frame);
                }
            }
        }

        bool insideBackLine(const glm::vec2& p, const glm::vec2& halfBack)
        {
            return (std::abs(p.x) < halfBack.x) && (std::abs(p.y) < halfBack.y);
        }
    }

    TableGeometry buildTableGeometry(const Sim::Table& table, const TableStyle& style)
    {
        TableGeometry geometry;
        const Frame frame {static_cast<float>(table.length), static_cast<float>(table.width)};
        const float halfLength = 0.5f * frame.length;
        const float halfWidth = 0.5f * frame.width;

        // The cushions' back line: where the jaws start, behind the noses.
        float cushionDepth = 0.0f;
        for (std::size_t i = 6; i < table.linear.size(); ++i)
        {
            const glm::vec3 back = frame(plan(table.linear[i].p1), 0.0f);
            cushionDepth = std::max(cushionDepth, std::max(std::abs(back.x) - halfLength, std::abs(back.z) - halfWidth));
        }
        const glm::vec2 halfBack(halfLength + cushionDepth, halfWidth + cushionDepth);
        const glm::vec2 halfOuter = halfBack + glm::vec2(style.railWidth);
        geometry.outerHalfExtent = halfOuter;

        // ---- Cushions ----------------------------------------------------------
        for (std::size_t run = 0; run < 6 && run < table.linear.size(); ++run)
        {
            buildCushion(geometry.cushions, cushionRun(table, run), frame, style);
        }

        // ---- Pockets: openings through the rail, leather rims, drops -------------
        std::vector<glm::vec3> pockets;
        std::vector<float> radii;
        for (const Sim::Pocket& pocket : table.pockets)
        {
            pockets.push_back(frame(plan(pocket.center), 0.0f));
            radii.push_back(static_cast<float>(pocket.radius));
        }
        const auto inPocket = [&](const glm::vec2& p, float grow)
        {
            for (std::size_t i = 0; i < pockets.size(); ++i)
            {
                if (glm::length(p - glm::vec2(pockets[i].x, pockets[i].z)) < radii[i] + grow)
                {
                    return true;
                }
            }
            return false;
        };

        // ---- Rails: the wood ring, with the pocket openings cut out --------------
        // A fine grid: cells inside a pocket opening are left out; the leather
        // rim drawn over the opening's edge hides the steps.
        constexpr float CELL = 0.01f;
        const int nx = static_cast<int>(std::ceil(2.0f * halfOuter.x / CELL));
        const int nz = static_cast<int>(std::ceil(2.0f * halfOuter.y / CELL));
        for (int ix = 0; ix < nx; ++ix)
        {
            for (int iz = 0; iz < nz; ++iz)
            {
                const float x0 = -halfOuter.x + static_cast<float>(ix) * CELL;
                const float z0 = -halfOuter.y + static_cast<float>(iz) * CELL;
                const float x1 = std::min(x0 + CELL, halfOuter.x);
                const float z1 = std::min(z0 + CELL, halfOuter.y);
                const glm::vec2 mid(0.5f * (x0 + x1), 0.5f * (z0 + z1));
                if (insideBackLine(mid, halfBack) || inPocket(mid, 0.0f))
                {
                    continue;
                }
                // Grain runs along each rail: across-the-rail coordinate in u.
                const bool longRail = std::abs(mid.y) >= halfBack.y;
                const auto uv = [&](float x, float z) { return longRail ? glm::vec2(z * 4.0f, x * 0.4f) : glm::vec2(x * 4.0f, z * 0.4f); };
                const glm::vec3 n(0.0f, 1.0f, 0.0f);
                const float y = style.railTop;
                const auto base = static_cast<std::uint32_t>(geometry.rails.vertices.size());
                geometry.rails.vertices.push_back({{x0, y, z0}, n, uv(x0, z0)});
                geometry.rails.vertices.push_back({{x0, y, z1}, n, uv(x0, z1)});
                geometry.rails.vertices.push_back({{x1, y, z1}, n, uv(x1, z1)});
                geometry.rails.vertices.push_back({{x1, y, z0}, n, uv(x1, z0)});
                geometry.rails.indices.insert(geometry.rails.indices.end(), {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
            }
        }
        // Outer sides of the rail.
        {
            const glm::vec3 lo(-halfOuter.x, style.railBottom, -halfOuter.y);
            const glm::vec3 hi(halfOuter.x, style.railTop, halfOuter.y);
            const glm::vec2 s(2.0f * halfOuter.x * 2.0f, 1.0f);
            geometry.rails.quad({lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}, s);
            geometry.rails.quad({hi.x, lo.y, lo.z}, {lo.x, lo.y, lo.z}, {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, s);
            geometry.rails.quad({hi.x, lo.y, hi.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, s);
            geometry.rails.quad({lo.x, lo.y, lo.z}, {lo.x, lo.y, hi.z}, {lo.x, hi.y, hi.z}, {lo.x, hi.y, lo.z}, s);
        }

        // Leather rims: a ring on the rail and a short wall down into the
        // drop, only where the rail is (outside the cushions' back line).
        constexpr int RIM_SEGMENTS = 72;
        for (std::size_t p = 0; p < pockets.size(); ++p)
        {
            const glm::vec2 c(pockets[p].x, pockets[p].z);
            // The rim starts inside the cut so it covers the rail grid's steps.
            const float rIn = radii[p] - 0.0125f;
            const float rOut = radii[p] + style.pocketRimWidth;
            const float y = style.railTop + 0.0012f;
            for (int i = 0; i < RIM_SEGMENTS; ++i)
            {
                const float a0 = 2.0f * PI * static_cast<float>(i) / RIM_SEGMENTS;
                const float a1 = 2.0f * PI * static_cast<float>(i + 1) / RIM_SEGMENTS;
                const glm::vec2 d0(std::cos(a0), std::sin(a0));
                const glm::vec2 d1(std::cos(a1), std::sin(a1));
                const glm::vec2 midOut = c + 0.5f * (d0 + d1) * rOut;
                const glm::vec2 midIn = c + 0.5f * (d0 + d1) * rIn;
                // Keep the rim within the rail ring.
                if ((insideBackLine(midOut, halfBack) && insideBackLine(midIn, halfBack)) ||
                    (std::abs(midOut.x) > halfOuter.x) || (std::abs(midOut.y) > halfOuter.y))
                {
                    continue;
                }
                const glm::vec3 i0(c.x + d0.x * rIn, y, c.y + d0.y * rIn), i1(c.x + d1.x * rIn, y, c.y + d1.y * rIn);
                const glm::vec3 o0(c.x + d0.x * rOut, y, c.y + d0.y * rOut), o1(c.x + d1.x * rOut, y, c.y + d1.y * rOut);
                upTriangle(geometry.pocketRims, i0, o0, o1, frame);
                upTriangle(geometry.pocketRims, i0, o1, i1, frame);
                // The inner wall faces the pocket's centre.
                wall(geometry.pocketRims, i0, i1, -0.004f, glm::vec3(c.x + 3.0f * (d0.x + d1.x) * rIn, 0.0f, c.y + 3.0f * (d0.y + d1.y) * rIn), frame);
            }

            // The drop: a cup seen from inside (walls face the centre), from
            // just under the cloth down to a dark bottom.
            const float top = -0.002f;
            const float bottom = -style.pocketDepth;
            for (int i = 0; i < RIM_SEGMENTS; ++i)
            {
                const float a0 = 2.0f * PI * static_cast<float>(i) / RIM_SEGMENTS;
                const float a1 = 2.0f * PI * static_cast<float>(i + 1) / RIM_SEGMENTS;
                const glm::vec3 p0(c.x + std::cos(a0) * radii[p], top, c.y + std::sin(a0) * radii[p]);
                const glm::vec3 p1(c.x + std::cos(a1) * radii[p], top, c.y + std::sin(a1) * radii[p]);
                // `inside` placed far outside makes the wall face inward.
                const glm::vec3 away(c.x + 10.0f * (std::cos(a0) + std::cos(a1)), 0.0f, c.y + 10.0f * (std::sin(a0) + std::sin(a1)));
                wall(geometry.pocketCups, p0, p1, bottom, away, frame);
                upTriangle(geometry.pocketCups, glm::vec3(c.x, bottom, c.y),
                           glm::vec3(p0.x, bottom, p0.z), glm::vec3(p1.x, bottom, p1.z), frame);
            }
        }

        // ---- Sights: diamonds along the rail centre lines --------------------------
        const float sightLine = 0.5f * (halfBack.y + halfOuter.y);
        const float sightEnd = 0.5f * (halfBack.x + halfOuter.x);
        const auto diamond = [&](float x, float z, bool alongX)
        {
            const float l = 2.0f * style.diamondRadius;
            const float w = style.diamondRadius;
            const float y = style.railTop + 0.0006f;
            const glm::vec3 c(x, y, z);
            const glm::vec3 u = alongX ? glm::vec3(l, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, l);
            const glm::vec3 v = alongX ? glm::vec3(0.0f, 0.0f, w) : glm::vec3(w, 0.0f, 0.0f);
            upTriangle(geometry.diamonds, c + u, c + v, c - u, frame);
            upTriangle(geometry.diamonds, c - u, c - v, c + u, frame);
        };
        for (int k = -3; k <= 3; ++k)
        {
            if (k == 0)
            {
                continue;   // the side pocket
            }
            const float x = static_cast<float>(k) * frame.length / 8.0f;
            diamond(x, sightLine, true);
            diamond(x, -sightLine, true);
        }
        for (int k = -1; k <= 1; ++k)
        {
            const float z = static_cast<float>(k) * frame.width / 4.0f;
            diamond(sightEnd, z, false);
            diamond(-sightEnd, z, false);
        }

        // ---- Trim, apron and legs ----------------------------------------------------
        const float trimTop = style.railBottom;
        const float trimBottom = trimTop - style.trimHeight;
        // Trim and apron are rings around the edge (not solid blocks), so the
        // pocket drops stay open and dark from above.
        const auto ring = [](MeshData& mesh, const glm::vec2& outer, float thickness, float bottom, float top)
        {
            const glm::vec2 inner = outer - glm::vec2(thickness);
            box(mesh, glm::vec3(-outer.x, bottom, inner.y), glm::vec3(outer.x, top, outer.y), true);
            box(mesh, glm::vec3(-outer.x, bottom, -outer.y), glm::vec3(outer.x, top, -inner.y), true);
            box(mesh, glm::vec3(inner.x, bottom, -inner.y), glm::vec3(outer.x, top, inner.y), true);
            box(mesh, glm::vec3(-outer.x, bottom, -inner.y), glm::vec3(-inner.x, top, inner.y), true);
        };
        ring(geometry.trim, halfOuter + glm::vec2(0.002f), 0.03f, trimBottom, trimTop);
        const float apronBottom = trimBottom - style.apronDepth;
        ring(geometry.apron, halfOuter - glm::vec2(style.apronInset), 0.03f, apronBottom, trimBottom);
        const float legX = halfOuter.x - 0.30f;
        const float legZ = halfOuter.y - 0.18f;
        for (const float sx : {-1.0f, 1.0f})
        {
            for (const float sz : {-1.0f, 1.0f})
            {
                const glm::vec3 c(sx * legX, 0.0f, sz * legZ);
                const float h = 0.5f * style.legSize;
                box(geometry.legs, glm::vec3(c.x - h, style.floorY, c.z - h), glm::vec3(c.x + h, apronBottom, c.z + h));
            }
        }

        return geometry;
    }
}
