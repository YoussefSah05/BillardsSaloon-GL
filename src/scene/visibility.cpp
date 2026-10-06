#include "scene/visibility.h"

#include <algorithm>
#include <cmath>

namespace BilliardsSaloon
{
    Bounds boundsOf(const MeshData& mesh)
    {
        Bounds bounds;
        for (const MeshVertex& vertex : mesh.vertices)
        {
            bounds.add(vertex.position);
        }
        return bounds;
    }

    Bounds transformBounds(const Bounds& local, const glm::mat4& model)
    {
        if (local.empty())
        {
            return local;
        }
        // Centre moves with the transform; each world half-extent is the sum
        // of the local half-extents weighted by |matrix entries| (Arvo 1990).
        const glm::vec3 centre = glm::vec3(model * glm::vec4(local.centre(), 1.0f));
        const glm::vec3 half = local.extent();
        glm::vec3 worldHalf(0.0f);
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                worldHalf[row] += std::abs(model[column][row]) * half[column];
            }
        }
        Bounds world;
        world.min = centre - worldHalf;
        world.max = centre + worldHalf;
        return world;
    }

    Frustum Frustum::fromViewProjection(const glm::mat4& m)
    {
        // glm is column-major: row i is (m[0][i], m[1][i], m[2][i], m[3][i]).
        const auto row = [&](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
        const glm::vec4 r0 = row(0);
        const glm::vec4 r1 = row(1);
        const glm::vec4 r2 = row(2);
        const glm::vec4 r3 = row(3);

        Frustum frustum;
        frustum.planes = {r3 + r0, r3 - r0, r3 + r1, r3 - r1, r3 + r2, r3 - r2};
        for (glm::vec4& plane : frustum.planes)
        {
            const float length = glm::length(glm::vec3(plane));
            if (length > 0.0f)
            {
                plane /= length;
            }
        }
        return frustum;
    }

    bool Frustum::intersects(const Bounds& box) const
    {
        if (box.empty())
        {
            return false;
        }
        const glm::vec3 centre = box.centre();
        const glm::vec3 half = box.extent();
        for (const glm::vec4& plane : planes)
        {
            // The box's projected radius on the plane normal.
            const glm::vec3 n(plane);
            const float radius = half.x * std::abs(n.x) + half.y * std::abs(n.y) + half.z * std::abs(n.z);
            if (glm::dot(n, centre) + plane.w < -radius)
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::contains(const glm::vec3& point) const
    {
        return std::all_of(planes.begin(), planes.end(),
                           [&](const glm::vec4& plane) { return glm::dot(glm::vec3(plane), point) + plane.w >= 0.0f; });
    }

    float screenCoverage(float radius, float distance, float verticalFovRadians)
    {
        if (distance <= radius)
        {
            return 1.0f;
        }
        return radius / (distance * std::tan(0.5f * verticalFovRadians));
    }
}
