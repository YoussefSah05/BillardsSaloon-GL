#include "rl/layouts.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace Septhuit::Rl
{
    namespace
    {
        constexpr double CUSHION_MARGIN = 0.005;    // m between a resting ball and the cushion line
        constexpr double BALL_GAP = 0.01;           // m between resting balls
        constexpr double POCKET_CLEARANCE = 0.03;   // m beyond a pocket's drop circle
        constexpr int MAX_TRIES = 100000;
    }

    const char* difficultyName(Difficulty difficulty)
    {
        switch (difficulty)
        {
            case Difficulty::Easy: return "easy";
            case Difficulty::Medium: return "medium";
            case Difficulty::Hard: return "hard";
            case Difficulty::None: return "none";
            case Difficulty::Any: return "any";
        }
        return "any";
    }

    Difficulty difficultyFromName(const std::string& name)
    {
        if (name == "easy") return Difficulty::Easy;
        if (name == "medium") return Difficulty::Medium;
        if (name == "hard") return Difficulty::Hard;
        if (name == "none") return Difficulty::None;
        if (name == "any") return Difficulty::Any;
        throw std::invalid_argument("Unknown difficulty '" + name + "' (easy, medium, hard, none, any).");
    }

    Difficulty classifyShot(const GhostShot& shot)
    {
        if ((shot.cutDegrees < 15.0) && (shot.cueDistance < 0.6) && (shot.objectDistance < 0.6))
        {
            return Difficulty::Easy;
        }
        if ((shot.cutDegrees < 45.0) && (shot.cueDistance < 1.2) && (shot.objectDistance < 1.2))
        {
            return Difficulty::Medium;
        }
        if (shot.cutDegrees < 70.0)
        {
            return Difficulty::Hard;
        }
        return Difficulty::None;
    }

    LayoutGenerator::LayoutGenerator(const Sim::Table& table, std::uint64_t seed, const Sim::BallParams& params)
        : m_table(table)
        , m_params(params)
        , m_mouths(pocketMouths(table))
        , m_random(seed)
    {
    }

    double LayoutGenerator::uniform(double lo, double hi)
    {
        // 53 random bits -> [0, 1), identical on every standard library.
        const double unit = static_cast<double>(m_random() >> 11) * 0x1.0p-53;
        return lo + (hi - lo) * unit;
    }

    glm::dvec2 LayoutGenerator::randomSpot()
    {
        const double inset = m_params.R + CUSHION_MARGIN;
        return {uniform(inset, m_table.width - inset), uniform(inset, m_table.length - inset)};
    }

    bool LayoutGenerator::legalSpot(const glm::dvec2& p, const std::vector<glm::dvec2>& others) const
    {
        const double inset = m_params.R + CUSHION_MARGIN;
        if ((p.x < inset) || (p.x > m_table.width - inset) || (p.y < inset) || (p.y > m_table.length - inset))
        {
            return false;
        }
        for (const Sim::Pocket& pocket : m_table.pockets)
        {
            if (glm::length(p - glm::dvec2(pocket.center.x, pocket.center.y)) < pocket.radius + m_params.R + POCKET_CLEARANCE)
            {
                return false;
            }
        }
        for (const glm::dvec2& other : others)
        {
            if (glm::length(p - other) < 2.0 * m_params.R + BALL_GAP)
            {
                return false;
            }
        }
        return true;
    }

    SingleBallLayout LayoutGenerator::analyse(const std::vector<glm::dvec2>& positions) const
    {
        SingleBallLayout layout;
        layout.positions = positions;
        // The easiest band over the clean lines to every pocket.
        for (const PocketMouth& mouth : m_mouths)
        {
            const GhostShot ghost = ghostShot(positions[0], positions[1], mouth.centre, m_params.R);
            if (pathBlocked(positions, positions[0], ghost.ghost, m_params.R, 0, 1))
            {
                continue;
            }
            const Difficulty band = classifyShot(ghost);
            const bool better = (band < layout.difficulty) ||
                                ((band == layout.difficulty) && (layout.pocket >= 0) && (shotEase(ghost) > shotEase(layout.ghost)));
            if ((band != Difficulty::None) && ((layout.pocket < 0) || better))
            {
                layout.difficulty = band;
                layout.pocket = mouth.pocket;
                layout.ghost = ghost;
            }
        }
        return layout;
    }

    SingleBallLayout LayoutGenerator::singleBallWithin(const ShotLimits& limits)
    {
        constexpr double PI = 3.14159265358979323846;
        const double R = m_params.R;
        for (int attempt = 0; attempt < MAX_TRIES; ++attempt)
        {
            const PocketMouth& mouth = m_mouths[static_cast<std::size_t>(m_random() % m_mouths.size())];

            // The object ball somewhere in front of the mouth (within 75
            // degrees of straight out), at a distance within the limits.
            const double outward = std::atan2(-mouth.inward.y, -mouth.inward.x) + uniform(-75.0, 75.0) * PI / 180.0;
            const double objectDistance = uniform(limits.minObjectDistance, std::max(limits.minObjectDistance, limits.maxObjectDistance));
            const glm::dvec2 object = mouth.centre + objectDistance * glm::dvec2(std::cos(outward), std::sin(outward));
            if (!legalSpot(object, {}))
            {
                continue;
            }

            // The cue ball behind the ghost ball, turned by the cut angle.
            const glm::dvec2 travel = glm::normalize(mouth.centre - object);
            const glm::dvec2 ghost = object - travel * (2.0 * R);
            const double cut = uniform(-limits.maxCutDegrees, limits.maxCutDegrees) * PI / 180.0;
            const glm::dvec2 approach(travel.x * std::cos(cut) - travel.y * std::sin(cut),
                                      travel.x * std::sin(cut) + travel.y * std::cos(cut));
            const double minCue = 2.0 * R + 0.02;
            const double cueDistance = uniform(minCue, std::max(minCue, limits.maxCueDistance));
            const glm::dvec2 cue = ghost - approach * cueDistance;
            if (!legalSpot(cue, {object}))
            {
                continue;
            }
            const std::vector<glm::dvec2> positions {cue, object};
            if (pathBlocked(positions, cue, ghost, R, 0, 1))
            {
                continue;
            }
            return analyse(positions);
        }
        throw std::runtime_error("No layout found within the curriculum limits.");
    }

    SingleBallLayout LayoutGenerator::singleBall(Difficulty band)
    {
        for (int attempt = 0; attempt < MAX_TRIES; ++attempt)
        {
            std::vector<glm::dvec2> positions;
            const glm::dvec2 cue = randomSpot();
            if (!legalSpot(cue, positions))
            {
                continue;
            }
            positions.push_back(cue);
            const glm::dvec2 object = randomSpot();
            if (!legalSpot(object, positions))
            {
                continue;
            }
            positions.push_back(object);

            SingleBallLayout layout = analyse(positions);
            if ((band == Difficulty::Any) || (layout.difficulty == band))
            {
                return layout;
            }
        }
        throw std::runtime_error(std::string("No ") + difficultyName(band) + " layout found.");
    }
}
