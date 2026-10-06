#include "core/asset_paths.h"
#include "scene/model_loader.h"

#include <doctest/doctest.h>

#include <filesystem>

using namespace BilliardsSaloon;

namespace
{
    std::size_t triangles(const MeshData& mesh)
    {
        return mesh.indices.size() / 3;
    }
}

TEST_CASE("every prop model loads with real-world size, materials and levels of detail")
{
    const std::filesystem::path folder = resolveAssetPath("models/props/dartboard/dartboard_1k.gltf").parent_path().parent_path();
    int loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        for (const auto& file : std::filesystem::directory_iterator(entry.path()))
        {
            if (file.path().extension() != ".gltf")
            {
                continue;
            }
            CAPTURE(file.path().string());
            const ModelData model = loadModel(file.path(), 3);
            ++loaded;

            REQUIRE_FALSE(model.parts.empty());
            REQUIRE_FALSE(model.materials.empty());
            // Metres: furniture and decor fit in a 3 m box and are not tiny.
            const glm::vec3 size = model.bounds.max - model.bounds.min;
            CHECK(glm::max(size.x, glm::max(size.y, size.z)) < 3.0f);
            CHECK(glm::max(size.x, glm::max(size.y, size.z)) > 0.05f);

            std::size_t full = 0;
            std::size_t coarse = 0;
            for (const ModelPart& part : model.parts)
            {
                full += triangles(*part.lods[0]);
                coarse += triangles(*part.lods[2]);
                REQUIRE(part.material < model.materials.size());
                REQUIRE(part.lods.size() == 3);
                CHECK(part.lods[0]->indices.size() % 3 == 0);
                // Each level has no more triangles than the one before.
                CHECK(triangles(*part.lods[1]) <= triangles(*part.lods[0]));
                CHECK(triangles(*part.lods[2]) <= triangles(*part.lods[1]));
            }
            // Detailed models lose most of their triangles at the coarsest level.
            if (full > 2000)
            {
                CHECK(coarse < full / 2);
            }

            const ModelMaterial& material = model.materials.front();
            if (material.baseColorMap)
            {
                CHECK(material.baseColorMap->width > 0);
                CHECK(material.baseColorMap->rgba.size() ==
                      static_cast<std::size_t>(material.baseColorMap->width * material.baseColorMap->height * 4));
            }
        }
    }
    CHECK(loaded >= 9);
}
