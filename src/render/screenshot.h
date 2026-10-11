#pragma once

#include <filesystem>

namespace Septhuit
{
    // Saves the current back buffer (before swapping) as a PNG, or as an
    // uncompressed BMP when the path ends in ".bmp" (fast, for recording).
    // Returns false and leaves no file on failure.
    [[nodiscard]] bool saveBackBufferImage(const std::filesystem::path& path, int width, int height);
}
