#include "ai/features.h"

#include <algorithm>

namespace BilliardsSaloon::Ai
{
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
