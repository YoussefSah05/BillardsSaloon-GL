#include "core/asset_paths.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>

using namespace BilliardsSaloon;

TEST_CASE("executableDirectory points at an existing directory")
{
    const std::filesystem::path directory = executableDirectory();
    CHECK_FALSE(directory.empty());
    CHECK(std::filesystem::is_directory(directory));
}

TEST_CASE("resolveAssetPath finds shipped shaders")
{
    const std::filesystem::path vertex = resolveAssetPath("shaders/basic.vert");
    CHECK(std::filesystem::is_regular_file(vertex));
    CHECK(vertex.filename() == "basic.vert");
}

TEST_CASE("resolveAssetPath throws for a missing asset")
{
    CHECK_THROWS_AS(static_cast<void>(resolveAssetPath("does/not/exist.bin")), std::runtime_error);
}
