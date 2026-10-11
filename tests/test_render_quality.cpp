#include "render/render_quality.h"

#include <doctest/doctest.h>

using namespace BilliardsSaloon;

TEST_CASE("scene resolution presets bound HiDPI pixel cost and preserve aspect ratio")
{
    const auto balanced = sceneRenderDimensions(3200, 1800, RenderQualityPreset::Balanced);
    CHECK(balanced.width == 1920);
    CHECK(balanced.height == 1080);
    const auto low = sceneRenderDimensions(3200, 1800, RenderQualityPreset::Low);
    CHECK(low.width == 1280);
    CHECK(low.height == 720);
    const auto high = sceneRenderDimensions(3200, 1800, RenderQualityPreset::High);
    CHECK(high.width == 3200);
    CHECK(high.height == 1800);

    const auto small = sceneRenderDimensions(800, 600, RenderQualityPreset::Balanced);
    CHECK(small.width == 800);
    CHECK(small.height == 600);
    const auto wide = sceneRenderDimensions(3840, 1600, RenderQualityPreset::Balanced);
    CHECK(wide.width == 1920);
    CHECK(wide.height == 800);
    const auto portrait = sceneRenderDimensions(1800, 3200, RenderQualityPreset::Balanced);
    CHECK(portrait.width == 1080);
    CHECK(portrait.height == 1920);
    const auto minimized = sceneRenderDimensions(0, 0, RenderQualityPreset::Low);
    CHECK(minimized.width == 1);
    CHECK(minimized.height == 1);
}
