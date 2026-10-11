#pragma once

#include <algorithm>
#include <cmath>

namespace Septhuit
{
    enum class RenderQualityPreset
    {
        Low,
        Balanced,
        High
    };

    struct RenderDimensions
    {
        int width {1};
        int height {1};
    };

    // Bound the 3D pixel cost on HiDPI/fullscreen displays. Presentation and
    // UI still use the native framebuffer; High keeps native scene resolution.
    [[nodiscard]] inline RenderDimensions sceneRenderDimensions(int width, int height, RenderQualityPreset quality)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (quality == RenderQualityPreset::High)
        {
            return {width, height};
        }
        const float maxLongEdge = (quality == RenderQualityPreset::Low) ? 1280.0f : 1920.0f;
        const float maxShortEdge = (quality == RenderQualityPreset::Low) ? 720.0f : 1080.0f;
        const float scale = std::min({1.0f, maxLongEdge / static_cast<float>(std::max(width, height)),
                                    maxShortEdge / static_cast<float>(std::min(width, height))});
        return {std::max(1, static_cast<int>(std::round(static_cast<float>(width) * scale))),
                std::max(1, static_cast<int>(std::round(static_cast<float>(height) * scale)))};
    }
}
