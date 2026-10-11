#include "ai/value_model.h"
#include "ai/environment.h"
#include "core/asset_paths.h"
#include <doctest/doctest.h>
#include <cmath>
#include <fstream>

using namespace BilliardsSaloon;

TEST_CASE("bundled adaptive model loads and responds to opponent skill in native ONNX")
{
    const auto model = Ai::ValueModel::load(resolveAssetPath("models/adaptive_value.onnx"));
    REQUIRE(model);
    const auto own = Ai::findAiProfile("iris_lindqvist");
    const auto weak = Ai::findAiProfile("sam_whitlock");
    const auto strong = Ai::findAiProfile("viktor_hale");
    Ai::Environment env(GameDiscipline::NineBall, 5);
    (void)env.playClassical(own, 5);
    double difference = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        const double easy = model->winProbability(env.observe(), 0, own, weak);
        const double hard = model->winProbability(env.observe(), 0, own, strong);
        CHECK(std::isfinite(easy));
        CHECK(easy >= 0.0);
        CHECK(easy <= 1.0);
        difference += std::abs(easy-hard);
        CHECK(model->function(own, weak, 0)(env.observe(), 0) == doctest::Approx(easy));
        if (env.decision() == Ai::Decision::FrameOver) break;
        (void)env.playClassical(own, static_cast<std::uint32_t>(6+i), true, model->function(own, weak, 0));
    }
    CHECK(difference > .01);
    CHECK_FALSE(Ai::ValueModel::load("missing-adaptive-model.onnx"));
    const auto corrupt = std::filesystem::path(BS_TEST_SCRATCH_DIR) / "invalid-ai.onnx";
    std::filesystem::create_directories(corrupt.parent_path());
    { std::ofstream file(corrupt); file << "not an ONNX model"; }
    CHECK_FALSE(Ai::ValueModel::load(corrupt));
}
