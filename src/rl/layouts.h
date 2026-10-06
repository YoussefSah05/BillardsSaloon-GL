#pragma once

#include "rl/geometry.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace BilliardsSaloon::Rl
{
    // How hard a single-ball layout is, from its easiest clean pot line
    // (docs/rl/rl-system-design.md, Section 6). Bands are exclusive: a layout
    // is in the easiest band it satisfies.
    enum class Difficulty
    {
        Easy,     // cut < 15 degrees, cue and object ball each < 0.6 m from their targets
        Medium,   // cut < 45 degrees, distances < 1.2 m
        Hard,     // cut < 70 degrees, any distances
        None,     // no clean pot line at all (blocked, or every cut too thin)
        Any       // for generation: accept every legal layout
    };

    [[nodiscard]] const char* difficultyName(Difficulty difficulty);
    [[nodiscard]] Difficulty difficultyFromName(const std::string& name);

    // The band of one ghost-ball line.
    [[nodiscard]] Difficulty classifyShot(const GhostShot& shot);

    struct SingleBallLayout
    {
        std::vector<glm::dvec2> positions;   // [cue ball, object ball]
        Difficulty difficulty {Difficulty::None};
        int pocket {-1};                     // the easiest clean pocket, -1 if none
        GhostShot ghost;                     // its ghost-ball line
    };

    // Seeded random layouts. The same seed gives the same layouts on every
    // platform: uniform numbers come straight from the 64-bit generator's bits
    // (std::uniform_real_distribution differs between standard libraries).
    class LayoutGenerator
    {
    public:
        LayoutGenerator(const Sim::Table& table, std::uint64_t seed, const Sim::BallParams& params = {});

        // A cue ball and an object ball, both resting legally on the table
        // (inside the cushions, apart, clear of the pockets), in `band`.
        [[nodiscard]] SingleBallLayout singleBall(Difficulty band);

        // Analyses a given layout (for fixed evaluation states).
        [[nodiscard]] SingleBallLayout analyse(const std::vector<glm::dvec2>& positions) const;

        // True if a ball centred here rests legally, apart from `others`.
        [[nodiscard]] bool legalSpot(const glm::dvec2& p, const std::vector<glm::dvec2>& others) const;

    private:
        [[nodiscard]] double uniform(double lo, double hi);
        [[nodiscard]] glm::dvec2 randomSpot();

        Sim::Table m_table;
        Sim::BallParams m_params;
        std::vector<PocketMouth> m_mouths;
        std::mt19937_64 m_random;
    };
}
