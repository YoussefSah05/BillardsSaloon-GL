#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace BilliardsSaloon
{
    // CPU-side triangle mesh, built headless (no GL) and uploaded by the renderer.
    struct MeshVertex
    {
        glm::vec3 position {0.0f};
        glm::vec3 normal {0.0f, 1.0f, 0.0f};
        glm::vec2 uv {0.0f};
    };

    struct MeshData
    {
        std::vector<MeshVertex> vertices;
        std::vector<std::uint32_t> indices;

        // Appends a triangle (counter-clockwise seen from where normal points).
        void triangle(const MeshVertex& a, const MeshVertex& b, const MeshVertex& c)
        {
            const auto base = static_cast<std::uint32_t>(vertices.size());
            vertices.insert(vertices.end(), {a, b, c});
            indices.insert(indices.end(), {base, base + 1U, base + 2U});
        }

        // A flat quad a-b-c-d (counter-clockwise from the front).
        void quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec2& uvScale = glm::vec2(1.0f))
        {
            const glm::vec3 n = glm::normalize(glm::cross(b - a, d - a));
            const auto base = static_cast<std::uint32_t>(vertices.size());
            vertices.push_back({a, n, glm::vec2(0.0f, 0.0f) * uvScale});
            vertices.push_back({b, n, glm::vec2(1.0f, 0.0f) * uvScale});
            vertices.push_back({c, n, glm::vec2(1.0f, 1.0f) * uvScale});
            vertices.push_back({d, n, glm::vec2(0.0f, 1.0f) * uvScale});
            indices.insert(indices.end(), {base, base + 1U, base + 2U, base, base + 2U, base + 3U});
        }

        void append(const MeshData& other)
        {
            const auto base = static_cast<std::uint32_t>(vertices.size());
            vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
            for (const std::uint32_t index : other.indices)
            {
                indices.push_back(base + index);
            }
        }
    };
}
