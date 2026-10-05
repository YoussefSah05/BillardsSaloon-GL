#pragma once

#include <filesystem>

namespace BilliardsSaloon
{
    // Ball numbers 1-15 rasterised from a font into a 4 x 4 grid (cell n holds
    // number n; cell 0 is empty), as a single-channel texture for the ball shader.
    class NumberAtlas
    {
    public:
        static constexpr int GRID = 4;
        static constexpr int CELL = 128;

        explicit NumberAtlas(const std::filesystem::path& fontFile);
        ~NumberAtlas();
        NumberAtlas(const NumberAtlas&) = delete;
        NumberAtlas& operator=(const NumberAtlas&) = delete;

        [[nodiscard]] unsigned int texture() const { return m_texture; }

    private:
        unsigned int m_texture {0};
    };
}
