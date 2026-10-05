#pragma once

#include "ai/ai_profile.h"
#include "ai/ai_table.h"
#include "ai/planner.h"
#include "gameplay/game_variant.h"
#include "gameplay/shot_input.h"
#include "rules/referee.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>

namespace BilliardsSaloon
{
    class MatchSession;
}

namespace BilliardsSaloon::Ai
{
    // What the player to act must decide next.
    enum class Decision
    {
        Break,       // the break shot
        Place,       // ball in hand: place the cue ball (then a Shot)
        Shot,
        Choice,      // answer the referee (Rules::optionsFor(frame.choice))
        FrameOver
    };

    struct StepResult
    {
        Rules::Verdict verdict;
        int shooter {0};          // who played
        int toAct {0};            // who acts next
        bool frameOver {false};
        int winner {-1};
    };

    // One frame at a time, headless and as fast as the simulator allows: a
    // shot is simulated, judged and applied at once (no real-time playback).
    // For self-play, data generation and evaluation; the game uses the same
    // session, simulator and referee, so what is learned here transfers.
    class Environment
    {
    public:
        explicit Environment(GameDiscipline discipline, std::uint32_t seed = 1);
        ~Environment();
        Environment(const Environment&) = delete;
        Environment& operator=(const Environment&) = delete;

        // A new frame with a freshly shuffled rack; `breaker` breaks.
        void reset(std::uint32_t seed, int breaker = 0);

        [[nodiscard]] Decision decision() const;
        [[nodiscard]] int toAct() const;
        [[nodiscard]] AiTable observe() const;
        [[nodiscard]] const Rules::FrameState& frame() const;

        // Plays a shot to rest. With ball in hand the cue ball must be placed
        // first (or it stays where it is, if that is legal).
        StepResult shoot(const ShotInput& input);
        bool place(const glm::vec2& position);
        bool choose(Rules::Option option);

        // Plays the classical AI's move for whoever is to act (any decision).
        // Returns the shot result for shots, or a default result otherwise.
        StepResult playClassical(const AiProfile& profile, std::uint32_t seed, bool withError = true,
                                 const ValueFunction& value = {});

        [[nodiscard]] int shotsPlayed() const { return m_shots; }

    private:
        GameDiscipline m_discipline;
        std::unique_ptr<MatchSession> m_session;
        int m_shots {0};
    };
}
