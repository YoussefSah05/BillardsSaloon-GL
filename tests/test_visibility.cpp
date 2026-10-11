#include "scene/visibility.h"
#include "scene/components.h"

#include <doctest/doctest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

using namespace Septhuit;

TEST_CASE("stationary transforms stay identical as the render interpolation fraction changes")
{
    TransformComponent transform;
    transform.position = {0.635f, -0.82f, 3.71f};
    transform.scale = {0.12f, 1.45f, 0.37f};
    transform.rotation = glm::angleAxis(0.37f, glm::vec3(0.0f, 1.0f, 0.0f));
    transform.syncPrevious();
    const glm::mat4 reference = composeInterpolatedMatrix(transform, 0.0f);
    for (int i = 0; i <= 100; ++i)
    {
        CHECK(composeInterpolatedMatrix(transform, static_cast<float>(i) / 100.0f) == reference);
    }
    transform.position.x += 1.0f;
    const auto halfway = interpolateTransform(transform, 0.5f);
    CHECK(halfway.position.x == doctest::Approx(transform.previousPosition.x + 0.5f));
}

namespace
{
    Bounds box(const glm::vec3& centre, float half)
    {
        Bounds b;
        b.add(centre - glm::vec3(half));
        b.add(centre + glm::vec3(half));
        return b;
    }

    // A camera at the origin looking down -z, 60° vertical field, square.
    Frustum testFrustum()
    {
        const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 50.0f);
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        return Frustum::fromViewProjection(projection * view);
    }
}

TEST_CASE("bounds of a mesh hold every vertex")
{
    MeshData mesh;
    mesh.quad({-1.0f, 0.0f, -2.0f}, {3.0f, 0.0f, -2.0f}, {3.0f, 0.5f, 4.0f}, {-1.0f, 0.5f, 4.0f});
    const Bounds b = boundsOf(mesh);
    CHECK(b.min.x == doctest::Approx(-1.0f));
    CHECK(b.max.x == doctest::Approx(3.0f));
    CHECK(b.min.z == doctest::Approx(-2.0f));
    CHECK(b.max.y == doctest::Approx(0.5f));
    CHECK(Bounds{}.empty());
}

TEST_CASE("transformed bounds follow translation, scale and rotation")
{
    const Bounds unit = box(glm::vec3(0.0f), 1.0f);

    const Bounds moved = transformBounds(unit, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f)));
    CHECK(moved.min.x == doctest::Approx(4.0f));
    CHECK(moved.max.x == doctest::Approx(6.0f));

    const Bounds scaled = transformBounds(unit, glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 3.0f)));
    CHECK(scaled.max.x == doctest::Approx(2.0f));
    CHECK(scaled.max.z == doctest::Approx(3.0f));

    // A 45° turn about y widens the box by sqrt(2) in x and z.
    const Bounds turned = transformBounds(unit, glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    CHECK(turned.max.x == doctest::Approx(std::sqrt(2.0f)).epsilon(1.0e-4));
    CHECK(turned.max.y == doctest::Approx(1.0f));
}

TEST_CASE("the frustum keeps what the camera sees and culls the rest")
{
    const Frustum frustum = testFrustum();

    CHECK(frustum.contains(glm::vec3(0.0f, 0.0f, -5.0f)));
    CHECK_FALSE(frustum.contains(glm::vec3(0.0f, 0.0f, 5.0f)));       // behind
    CHECK_FALSE(frustum.contains(glm::vec3(0.0f, 0.0f, -60.0f)));     // beyond far

    CHECK(frustum.intersects(box(glm::vec3(0.0f, 0.0f, -10.0f), 0.5f)));
    CHECK_FALSE(frustum.intersects(box(glm::vec3(0.0f, 0.0f, 10.0f), 0.5f)));
    // tan(30°) * 10 = 5.77: a box at x = 20 is far outside the side plane...
    CHECK_FALSE(frustum.intersects(box(glm::vec3(20.0f, 0.0f, -10.0f), 0.5f)));
    // ...and one straddling the edge is kept.
    CHECK(frustum.intersects(box(glm::vec3(6.0f, 0.0f, -10.0f), 0.5f)));
    // A huge box around the camera is always visible.
    CHECK(frustum.intersects(box(glm::vec3(0.0f), 100.0f)));
    CHECK_FALSE(frustum.intersects(Bounds{}));
}

TEST_CASE("screen coverage falls with distance")
{
    const float fov = glm::radians(60.0f);
    CHECK(screenCoverage(1.0f, 0.5f, fov) == doctest::Approx(1.0f));
    const float near = screenCoverage(0.5f, 2.0f, fov);
    const float far = screenCoverage(0.5f, 8.0f, fov);
    CHECK(near == doctest::Approx(4.0f * far));
    CHECK(far < near);
}
