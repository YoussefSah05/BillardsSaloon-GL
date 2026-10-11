#pragma once

#include "ai/planner.h"

#include <filesystem>
#include <memory>

namespace Septhuit::Ai
{
    struct ValueEstimate
    {
        double probability {0.5};
        double modelDisagreement {0.0}; // ensemble standard deviation, not an error bound
        bool hasDisagreement {false};
    };
    // The learned value network through ONNX Runtime: features in,
    // P(player wins the frame) out, plus optional ensemble disagreement.
    // Supports legacy and opponent-conditioned features. Thread-safe.
    class ValueModel
    {
    public:
        // Nullptr (and a message) if the model is missing or invalid; the AI
        // then plays with its classical evaluation.
        [[nodiscard]] static std::unique_ptr<ValueModel> load(const std::filesystem::path& file);
        ~ValueModel();

        [[nodiscard]] double winProbability(const AiTable& table, int player) const;
        [[nodiscard]] double winProbability(const AiTable& table, int player,
                                            const AiProfile& own, const AiProfile& opponent) const;
        [[nodiscard]] ValueEstimate estimate(const AiTable& table, int player,
                                            const AiProfile& own, const AiProfile& opponent) const;

        // For the planner. The model must outlive the function.
        [[nodiscard]] ValueFunction function() const;
        [[nodiscard]] ValueFunction function(const AiProfile& own, const AiProfile& opponent, int ownSeat) const;

    private:
        ValueModel();
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
