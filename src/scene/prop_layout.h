#pragma once

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    // A loaded model placed in a hall (assets/data/halls/props.json).
    struct PropPlacement
    {
        std::string model;              // asset path of the .gltf
        glm::vec3 position {0.0f};      // x, z on the floor plan; y above the floor
        float yawDegrees {0.0f};        // about +y; 90 turns the model's front (+z) to +x
        float scale {1.0f};
    };

    // The placements for one hall layout ("saloon", "arena"); empty when the
    // layout has none. Throws std::runtime_error on a malformed file.
    [[nodiscard]] std::vector<PropPlacement> loadPropLayout(const std::filesystem::path& file, const std::string& layout);
}
