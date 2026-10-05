#include "ai/environment.h"

#include "ai/planner.h"
#include "gameplay/match_session.h"

namespace BilliardsSaloon::Ai
{
    Environment::Environment(GameDiscipline discipline, std::uint32_t seed)
        : m_discipline(discipline)
    {
        reset(seed);
    }

    Environment::~Environment() = default;

    void Environment::reset(std::uint32_t seed, int breaker)
    {
        MatchSettings settings;
        settings.seed = seed;
        settings.firstBreaker = breaker;
        m_session = std::make_unique<MatchSession>(variantFor(m_discipline), ShotInputTuning{}, settings);
        m_shots = 0;
    }

    Decision Environment::decision() const
    {
        const Rules::FrameState& frame = m_session->frame();
        if (m_session->frameOver())
        {
            return Decision::FrameOver;
        }
        if (frame.choice != Rules::Choice::None)
        {
            return Decision::Choice;
        }
        if (frame.phase == Rules::Phase::Break)
        {
            return Decision::Break;
        }
        if (m_session->placingCueBall())
        {
            return Decision::Place;
        }
        return Decision::Shot;
    }

    int Environment::toAct() const
    {
        const Rules::FrameState& frame = m_session->frame();
        return (frame.choice != Rules::Choice::None) ? frame.chooser : frame.shooter;
    }

    AiTable Environment::observe() const
    {
        return m_session->aiView();
    }

    const Rules::FrameState& Environment::frame() const
    {
        return m_session->frame();
    }

    StepResult Environment::shoot(const ShotInput& input)
    {
        StepResult result;
        result.shooter = m_session->frame().shooter;
        if (!m_session->playShot(input))
        {
            result.toAct = toAct();
            return result;
        }
        // Play the whole trajectory at once.
        m_session->step(1.0e6);
        ++m_shots;

        result.verdict = m_session->lastOutcome().verdict;
        result.toAct = toAct();
        result.frameOver = m_session->frameOver();
        result.winner = result.verdict.winner;
        return result;
    }

    bool Environment::place(const glm::vec2& position)
    {
        return m_session->placeCueBallAt(position);
    }

    bool Environment::choose(Rules::Option option)
    {
        return m_session->choose(option);
    }

    StepResult Environment::playClassical(const AiProfile& profile, std::uint32_t seed, bool withError)
    {
        std::mt19937 random(seed);
        switch (decision())
        {
            case Decision::Choice:
                (void)choose(planChoice(observe(), profile, seed));
                return StepResult{};
            case Decision::Place:
                (void)place(planCueBallPlacement(observe(), profile, seed));
                [[fallthrough]];
            case Decision::Shot:
            {
                const AiShot plan = planShot(observe(), profile, seed);
                return shoot(withError ? withExecutionError(plan.input, profile, random) : plan.input);
            }
            case Decision::Break:
            {
                const AiShot plan = planBreak(observe(), profile);
                return shoot(withError ? withExecutionError(plan.input, profile, random) : plan.input);
            }
            case Decision::FrameOver:
                break;
        }
        return StepResult{};
    }
}
