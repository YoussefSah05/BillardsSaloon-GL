#pragma once

#include "ai/ai_profile.h"
#include "gameplay/game_variant.h"
#include <array>

namespace Septhuit::Ai
{
    // Match-local, smoothed skill evidence. Only recognisable pot attempts
    // count as misses; a legal safety is not evidence of weak execution.
    class OpponentModel
    {
    public:
        void reset();
        void observe(float potEase, bool potted, bool foul, bool breakShot,
                     GameDiscipline discipline = GameDiscipline::NineBall);
        [[nodiscard]] float skill() const { return m_skill; }
        [[nodiscard]] int observations() const { return m_observations; }
        [[nodiscard]] AiProfile estimatedProfile() const;
        [[nodiscard]] AiProfile adapt(const AiProfile& personality) const;
    private:
        float m_skill {0.5f};
        int m_observations {0};
        std::array<float, 5> m_belief {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};
    };

    [[nodiscard]] AiProfile profileAtSkill(float skill);
}
