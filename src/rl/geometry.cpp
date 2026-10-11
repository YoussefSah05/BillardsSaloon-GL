#include "rl/geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Septhuit::Rl
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;

        glm::dvec2 planar(const glm::dvec3& v)
        {
            return {v.x, v.y};
        }

        double segmentDistance(const glm::dvec2& p, const glm::dvec2& a, const glm::dvec2& b)
        {
            const glm::dvec2 ab = b - a;
            const double length2 = glm::dot(ab, ab);
            const double t = (length2 > 0.0) ? std::clamp(glm::dot(p - a, ab) / length2, 0.0, 1.0) : 0.0;
            return glm::length(p - (a + t * ab));
        }
    }

    std::vector<PocketMouth> pocketMouths(const Sim::Table& table)
    {
        std::vector<PocketMouth> mouths;
        for (std::size_t p = 0; p < table.pockets.size(); ++p)
        {
            const glm::dvec2 centre = planar(table.pockets[p].center);

            // The two jaw tips nearest this pocket.
            std::vector<std::pair<double, std::size_t>> jaws;
            for (std::size_t j = 0; j < table.circular.size(); ++j)
            {
                jaws.emplace_back(glm::length(planar(table.circular[j].center) - centre), j);
            }
            std::sort(jaws.begin(), jaws.end());

            PocketMouth mouth;
            mouth.pocket = static_cast<int>(p);
            if (jaws.size() < 2)
            {
                mouth.centre = centre;
                mouths.push_back(mouth);
                continue;
            }
            const Sim::CircularCushion& j1 = table.circular[jaws[0].second];
            const Sim::CircularCushion& j2 = table.circular[jaws[1].second];
            const glm::dvec2 a = planar(j1.center);
            const glm::dvec2 b = planar(j2.center);
            mouth.centre = 0.5 * (a + b);
            const glm::dvec2 toPocket = centre - mouth.centre;
            mouth.inward = (glm::length(toPocket) > 1e-9) ? glm::normalize(toPocket) : glm::dvec2(0.0);
            mouth.halfWidth = 0.5 * glm::length(b - a) - 0.5 * (j1.radius + j2.radius);
            mouths.push_back(mouth);
        }
        return mouths;
    }

    GhostShot ghostShot(const glm::dvec2& cue, const glm::dvec2& object, const glm::dvec2& target, double R)
    {
        GhostShot shot;
        const glm::dvec2 toTarget = target - object;
        shot.objectDistance = glm::length(toTarget);
        const glm::dvec2 d = (shot.objectDistance > 1e-12) ? toTarget / shot.objectDistance : glm::dvec2(1.0, 0.0);
        shot.ghost = object - d * (2.0 * R);
        const glm::dvec2 aim = shot.ghost - cue;
        shot.cueDistance = glm::length(aim);
        shot.phiDegrees = std::atan2(aim.y, aim.x) * 180.0 / PI;
        if (shot.phiDegrees < 0.0)
        {
            shot.phiDegrees += 360.0;
        }
        const double cosine = (shot.cueDistance > 1e-12) ? glm::dot(aim / shot.cueDistance, d) : 1.0;
        shot.cutDegrees = std::acos(std::clamp(cosine, -1.0, 1.0)) * 180.0 / PI;
        return shot;
    }

    bool pathBlocked(const std::vector<glm::dvec2>& positions, const glm::dvec2& from, const glm::dvec2& to,
                     double R, int ignoreA, int ignoreB)
    {
        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            const int index = static_cast<int>(i);
            if ((index == ignoreA) || (index == ignoreB))
            {
                continue;
            }
            if (segmentDistance(positions[i], from, to) < 2.0 * R)
            {
                return true;
            }
        }
        return false;
    }

    double cueSpeedFor(const GhostShot& shot, const Sim::BallParams& params, const Sim::CueSpecs& cue, double arrival)
    {
        // Backwards from the pocket. A rolling ball slows at mu_r g, so
        // v_start^2 = v_end^2 + 2 mu_r g d. A ball struck without spin, or
        // hit at the centre, slides first and rolls at 5/7 of its speed.
        const double rollingDecel = params.u_r * params.g;
        const auto before = [&](double after, double distance) { return std::sqrt(after * after + 2.0 * rollingDecel * distance); };

        const double objectRolling = before(arrival, shot.objectDistance);
        const double objectLaunch = objectRolling * 7.0 / 5.0;
        // A rolling cue ball passes (1 + e) / 2 of its normal speed, cos(cut) of it.
        const double cosCut = std::max(std::cos(shot.cutDegrees * PI / 180.0), 0.2);
        const double cueAtContact = objectLaunch / (0.5 * (1.0 + params.e_b) * cosCut);
        const double cueLaunch = before(cueAtContact, shot.cueDistance) * 7.0 / 5.0;
        // A centre-ball hit gives the ball 2 V / (1 + m / M) (resolveStrike).
        return cueLaunch * (1.0 + params.m / cue.M) / 2.0;
    }

    double shotEase(const GhostShot& shot, double maxCutDegrees)
    {
        if (shot.cutDegrees > maxCutDegrees)
        {
            return 0.0;
        }
        const double c = std::cos(shot.cutDegrees * PI / 180.0);
        return c * c * std::exp(-(shot.cueDistance + shot.objectDistance) / 2.2);
    }

    std::optional<GeometryShot> geometryShot(const Sim::Table& table, const std::vector<PocketMouth>& mouths,
                                             const std::vector<glm::dvec2>& positions, int objectBall,
                                             const Sim::BallParams& params, double maxCutDegrees, int onlyPocket)
    {
        (void)table;
        std::optional<GeometryShot> best;
        const glm::dvec2 cue = positions.front();
        const glm::dvec2 object = positions[static_cast<std::size_t>(objectBall)];
        for (const PocketMouth& mouth : mouths)
        {
            if ((onlyPocket >= 0) && (mouth.pocket != onlyPocket))
            {
                continue;
            }
            const GhostShot ghost = ghostShot(cue, object, mouth.centre, params.R);
            const double ease = shotEase(ghost, maxCutDegrees);
            if (ease <= 0.0 ||
                pathBlocked(positions, cue, ghost.ghost, params.R, 0, objectBall) ||
                pathBlocked(positions, object, mouth.centre, params.R, 0, objectBall))
            {
                continue;
            }
            if (!best || (ease > best->ease))
            {
                GeometryShot shot;
                shot.pocket = mouth.pocket;
                shot.ghost = ghost;
                shot.ease = ease;
                shot.strike.phiDegrees = ghost.phiDegrees;
                shot.strike.speed = std::clamp(cueSpeedFor(ghost, params, Sim::CueSpecs{}), 0.5, 7.0);
                best = shot;
            }
        }
        return best;
    }
}
