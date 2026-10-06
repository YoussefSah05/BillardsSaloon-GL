#pragma once

#include "scene/mesh_data.h"
#include "scene/visibility.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    // An 8-bit RGBA image, decoded on the CPU, uploaded by the renderer.
    struct ModelTexture
    {
        int width {0};
        int height {0};
        std::vector<std::uint8_t> rgba;
    };

    // glTF's metallic-roughness material: base colour (sRGB), a normal map,
    // and an occlusion-roughness-metal map (R, G, B as glTF packs them).
    struct ModelMaterial
    {
        glm::vec4 baseColor {1.0f};
        float metallic {1.0f};
        float roughness {1.0f};
        std::shared_ptr<const ModelTexture> baseColorMap;
        std::shared_ptr<const ModelTexture> normalMap;
        std::shared_ptr<const ModelTexture> ormMap;
        bool occlusionInOrm {false};   // the map's red channel is ambient occlusion
        bool alphaMask {false};        // cut out where the base colour's alpha is below 0.5 (leaves)
        bool doubleSided {false};      // thin surfaces seen from both sides
    };

    // One mesh of the model with its levels of detail: lods[0] is the
    // original, each next one about a third of the triangles of the last.
    struct ModelPart
    {
        std::vector<std::shared_ptr<const MeshData>> lods;
        std::size_t material {0};
    };

    struct ModelData
    {
        std::string name;
        std::vector<ModelPart> parts;
        std::vector<ModelMaterial> materials;
        Bounds bounds;
    };

    // Loads a glTF 2.0 file (.gltf with external or embedded buffers, or
    // .glb): every triangle primitive with node transforms baked in, its
    // material and textures, and `lodLevels` levels of detail made with
    // meshoptimizer's simplifier. Throws std::runtime_error on failure.
    // Headless: safe to call on a worker thread.
    [[nodiscard]] ModelData loadModel(const std::filesystem::path& path, int lodLevels = 3);

    // Builds a simplified copy of `mesh` with about `ratio` of its triangles,
    // keeping the shape within `error` (relative to the mesh's size).
    [[nodiscard]] MeshData simplifyMesh(const MeshData& mesh, float ratio, float error);
}
