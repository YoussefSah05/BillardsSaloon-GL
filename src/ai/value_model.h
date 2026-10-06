#pragma once

#include "ai/planner.h"

#include <filesystem>
#include <memory>

namespace BilliardsSaloon::Ai
{
    // The learned value network (assets/models/value.onnx) through ONNX
    // Runtime: features in, P(player wins the frame) out. Thread-safe.
    class ValueModel
    {
    public:
        // Nullptr (and a message) if the model is missing or invalid; the AI
        // then plays with its classical evaluation.
        [[nodiscard]] static std::unique_ptr<ValueModel> load(const std::filesystem::path& file);
        ~ValueModel();

        [[nodiscard]] double winProbability(const AiTable& table, int player) const;

        // For the planner. The model must outlive the function.
        [[nodiscard]] ValueFunction function() const;

    private:
        ValueModel();
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
