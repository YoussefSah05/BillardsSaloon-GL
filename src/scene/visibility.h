#pragma once

#include "scene/mesh_data.h"

#include <glm/glm.hpp>

#include <array>
#include <limits>

namespace Septhuit
{
    // An axis-aligned bounding box. Empty (min > max) until a point is added.
    struct Bounds
    {
        glm::vec3 min {std::numeric_limits<float>::max()};
        glm::vec3 max {std::numeric_limits<float>::lowest()};

        [[nodiscard]] bool empty() const { return (min.x > max.x) || (min.y > max.y) || (min.z > max.z); }
        [[nodiscard]] glm::vec3 centre() const { return 0.5f * (min + max); }
        [[nodiscard]] glm::vec3 extent() const { return 0.5f * (max - min); }

        void add(const glm::vec3& p)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        void add(const Bounds& other)
        {
            if (!other.empty())
            {
                add(other.min);
                add(other.max);
            }
        }
    };

    [[nodiscard]] Bounds boundsOf(const MeshData& mesh);

    // The box that holds `local` after the affine transform `model`
    // (Arvo's method: exact for the transformed box, tight for rotations of 90°).
    [[nodiscard]] Bounds transformBounds(const Bounds& local, const glm::mat4& model);

    // The six planes of a view-projection frustum, normals pointing inwards:
    // a point p is inside when dot(plane.xyz, p) + plane.w >= 0 for all six.
    struct Frustum
    {
        std::array<glm::vec4, 6> planes {};

        // Gribb & Hartmann: the planes are sums and differences of the
        // matrix's rows (OpenGL clip space, -w <= x, y, z <= w).
        [[nodiscard]] static Frustum fromViewProjection(const glm::mat4& viewProjection);

        // False only when the box is entirely outside one plane. Boxes near a
        // corner may pass although outside (conservative: never culls a visible box).
        [[nodiscard]] bool intersects(const Bounds& box) const;

        [[nodiscard]] bool contains(const glm::vec3& point) const;
    };

    // How large a sphere of `radius` at `distance` looks: its projected
    // diameter as a fraction of the screen height, for a vertical field of view.
    [[nodiscard]] float screenCoverage(float radius, float distance, float verticalFovRadians);
}
