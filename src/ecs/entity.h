#pragma once

#include <cstdint>
#include <limits>

namespace BilliardsSaloon
{
    struct Entity
    {
        static constexpr std::uint32_t INVALID_INDEX = std::numeric_limits<std::uint32_t>::max();

        std::uint32_t index {INVALID_INDEX};
        std::uint32_t generation {0};

        [[nodiscard]] bool isValid() const
        {
            return index != INVALID_INDEX; // index<INVALID_INDEX [0,MAX)
        }

        friend bool operator==(const Entity&, const Entity&) = default;
    };
}