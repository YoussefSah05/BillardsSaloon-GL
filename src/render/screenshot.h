#pragma once

#include <filesystem>

namespace BilliardsSaloon
{
    // Saves the current back buffer (before swapping) as a PNG.
    // Returns false and leaves no file on failure.
    [[nodiscard]] bool saveBackBufferPng(const std::filesystem::path& path, int width, int height);
}
