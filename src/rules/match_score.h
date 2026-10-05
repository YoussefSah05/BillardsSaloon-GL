#pragma once

#include <array>

namespace BilliardsSaloon::Rules
{
    enum class BreakOrder
    {
        Alternate,      // players take turns to break (WPA default for most events)
        WinnerBreaks
    };

    // A race to N frames.
    struct MatchScore
    {
        int raceTo {1};
        BreakOrder order {BreakOrder::Alternate};
        std::array<int, 2> frames {0, 0};
        int lastBreaker {0};
        int winner {-1};

        [[nodiscard]] bool over() const { return winner >= 0; }

        // Records a frame; returns who breaks the next one.
        int recordFrame(int frameWinner, int frameBreaker)
        {
            ++frames[static_cast<std::size_t>(frameWinner)];
            lastBreaker = frameBreaker;
            if (frames[static_cast<std::size_t>(frameWinner)] >= raceTo)
            {
                winner = frameWinner;
            }
            return (order == BreakOrder::WinnerBreaks) ? frameWinner : (frameBreaker == 0 ? 1 : 0);
        }

        // One frame from the match, for the broadcast "hill" graphic.
        [[nodiscard]] bool onTheHill(int player) const
        {
            return frames[static_cast<std::size_t>(player)] == raceTo - 1;
        }
    };
}
