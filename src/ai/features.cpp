#include "ai/features.h"
#include "ai/planner.h"

#include <algorithm>

namespace Septhuit::Ai
{
    std::vector<float> encodeAdaptiveTable(const AiTable& table, int perspective, const AiProfile& own, const AiProfile& opponent)
    {
        const std::vector<float> base = encodeTable(table, perspective);
        const auto geometry = ballGeometry(table);
        std::vector<float> features(ADAPTIVE_FEATURE_COUNT, 0.0f);
        for (std::size_t slot = 0; slot < BALL_SLOTS; ++slot)
        {
            std::copy_n(base.begin() + static_cast<std::ptrdiff_t>(slot * BALL_FEATURES), BALL_FEATURES,
                        features.begin() + static_cast<std::ptrdiff_t>(slot * ADAPTIVE_BALL_FEATURES));
        }
        for (std::size_t i = 0; i < table.numbers.size(); ++i)
        {
            const int number = table.numbers[i];
            if (number >= 0 && number < static_cast<int>(BALL_SLOTS))
            {
                std::copy(geometry[i].begin(), geometry[i].end(),
                    features.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(number) * ADAPTIVE_BALL_FEATURES + BALL_FEATURES));
            }
        }
        std::copy_n(base.begin() + static_cast<std::ptrdiff_t>(BALL_SLOTS * BALL_FEATURES), CONTEXT_FEATURES,
                    features.begin() + static_cast<std::ptrdiff_t>(BALL_SLOTS * ADAPTIVE_BALL_FEATURES));
        std::size_t offset = BALL_SLOTS * ADAPTIVE_BALL_FEATURES + CONTEXT_FEATURES;
        for (const AiProfile* profile : {&own, &opponent})
        {
            const std::array<float, PROFILE_FEATURES> encoded {
                profile->aimNoiseDegrees / 1.2f, profile->powerNoise / 0.16f, profile->spinNoise / 0.2f,
                profile->safetyBias / 8.0f, profile->powerStyle, profile->breakPower,
                static_cast<float>(profile->samples) / 12.0f, 1.0f}; // known profile; human estimate uses the same schema
            for (const float value : encoded)
            {
                features[offset++] = std::clamp(value, -2.0f, 2.0f);
            }
        }
        return features;
    }
    std::vector<float> encodeTable(const AiTable& table, int perspective)
    {
        std::vector<float> features(FEATURE_COUNT, 0.0f);
        const std::vector<int> onTable = table.objectBallsOnTable();

        // Legal targets for the perspective player (as if they were to shoot).
        Rules::FrameState mine = table.frame;
        mine.shooter = perspective;
        const std::vector<int> legal = Rules::legalFirstContacts(mine, onTable);
        const Rules::Group myGroup = table.frame.groups[static_cast<std::size_t>(perspective)];
        const Rules::Group theirGroup = table.frame.groups[static_cast<std::size_t>(Rules::otherPlayer(perspective))];
        const int game = Rules::gameBall(table.discipline);

        for (std::size_t i = 0; i < table.numbers.size(); ++i)
        {
            const int number = table.numbers[i];
            if ((number < 0) || (number >= static_cast<int>(BALL_SLOTS)) || table.pocketed[i])
            {
                continue;
            }
            float* slot = &features[static_cast<std::size_t>(number) * BALL_FEATURES];
            slot[0] = 1.0f;
            slot[1] = table.positions[i].x / (0.5f * table.length);
            slot[2] = table.positions[i].y / (0.5f * table.width);
            slot[3] = (std::find(legal.begin(), legal.end(), number) != legal.end()) ? 1.0f : 0.0f;
            slot[4] = ((myGroup != Rules::Group::None) && (Rules::groupOf(number) == myGroup)) ? 1.0f : 0.0f;
            slot[5] = ((theirGroup != Rules::Group::None) && (Rules::groupOf(number) == theirGroup)) ? 1.0f : 0.0f;
            slot[6] = (number == game) ? 1.0f : 0.0f;
            slot[7] = static_cast<float>(number) / 15.0f;
        }

        float* context = &features[BALL_SLOTS * BALL_FEATURES];
        context[static_cast<std::size_t>(table.discipline)] = 1.0f;                        // 0..2
        context[3 + static_cast<std::size_t>(table.frame.phase)] = 1.0f;                   // 3..7
        context[8 + static_cast<std::size_t>(table.frame.ballInHand)] = 1.0f;              // 8..10
        context[11] = (table.frame.shooter == perspective) ? 1.0f : 0.0f;
        context[12] = static_cast<float>(table.frame.consecutiveFouls[static_cast<std::size_t>(perspective)]) / 3.0f;
        context[13] = static_cast<float>(table.frame.consecutiveFouls[static_cast<std::size_t>(Rules::otherPlayer(perspective))]) / 3.0f;
        context[14] = table.frame.pushOutAvailable ? 1.0f : 0.0f;
        context[15] = (myGroup != Rules::Group::None) ? 1.0f : 0.0f;
        return features;
    }
}
