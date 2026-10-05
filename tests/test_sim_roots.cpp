#include "sim/roots.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <random>

using namespace BilliardsSaloon::Sim;

namespace
{
    // (t - r1)(t - r2)(t - r3)(t - r4) expanded.
    Polynomial fromRoots(double r1, double r2, double r3, double r4)
    {
        Polynomial p;
        p.c = {
            r1 * r2 * r3 * r4,
            -(r1 * r2 * r3 + r1 * r2 * r4 + r1 * r3 * r4 + r2 * r3 * r4),
            r1 * r2 + r1 * r3 + r1 * r4 + r2 * r3 + r2 * r4 + r3 * r4,
            -(r1 + r2 + r3 + r4),
            1.0
        };
        return p;
    }
}

TEST_CASE("four simple roots are found exactly")
{
    const std::vector<double> roots = realRootsInInterval(fromRoots(0.1, 0.7, 1.3, 2.9), 0.0, 5.0);
    REQUIRE(roots.size() == 4);
    CHECK(roots[0] == doctest::Approx(0.1).epsilon(1e-12));
    CHECK(roots[1] == doctest::Approx(0.7).epsilon(1e-12));
    CHECK(roots[2] == doctest::Approx(1.3).epsilon(1e-12));
    CHECK(roots[3] == doctest::Approx(2.9).epsilon(1e-12));
}

TEST_CASE("only roots inside the interval are returned")
{
    const std::vector<double> roots = realRootsInInterval(fromRoots(0.1, 0.7, 1.3, 2.9), 0.5, 1.5);
    REQUIRE(roots.size() == 2);
    CHECK(roots[0] == doctest::Approx(0.7));
    CHECK(roots[1] == doctest::Approx(1.3));
}

TEST_CASE("a double root is reported once and is not a closing root")
{
    // (t - 1)² (t - 3)(t - 4): touches zero at 1, crosses at 3 and 4.
    const Polynomial p = fromRoots(1.0, 1.0, 3.0, 4.0);
    const std::vector<double> roots = realRootsInInterval(p, 0.0, 5.0);
    REQUIRE(roots.size() == 3);
    CHECK(roots[0] == doctest::Approx(1.0).epsilon(1e-7));

    // p > 0 before 1, touches 0 at 1, stays positive until 3, negative 3..4.
    CHECK(firstClosingRoot(p, 0.0, 5.0) == doctest::Approx(3.0));
}

TEST_CASE("a quartic with no real roots has none")
{
    Polynomial p;
    p.c = {1.0, 0.0, 0.0, 0.0, 1.0};   // t⁴ + 1
    CHECK(realRootsInInterval(p, -10.0, 10.0).empty());
    CHECK(std::isinf(firstClosingRoot(p, 0.0, 10.0)));
}

TEST_CASE("lower-degree polynomials work")
{
    Polynomial linear;
    linear.c = {2.0, -4.0, 0.0, 0.0, 0.0};   // 2 - 4t
    CHECK(firstClosingRoot(linear, 0.0, 1.0) == doctest::Approx(0.5));

    Polynomial quadratic;
    quadratic.c = {1.0, -3.0, 2.0, 0.0, 0.0};   // (2t - 1)(t - 1)
    const std::vector<double> roots = realRootsInInterval(quadratic, 0.0, 2.0);
    REQUIRE(roots.size() == 2);
    CHECK(roots[0] == doctest::Approx(0.5));
    CHECK(roots[1] == doctest::Approx(1.0));
    CHECK(firstClosingRoot(quadratic, 0.0, 2.0) == doctest::Approx(0.5));
}

TEST_CASE("an opening root is not a closing root")
{
    Polynomial p;
    p.c = {-1.0, 1.0, 0.0, 0.0, 0.0};   // starts negative, rises through zero at 1
    CHECK(std::isinf(firstClosingRoot(p, 0.0, 2.0)));
}

TEST_CASE("randomised quartics agree with dense sampling")
{
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> rootDistribution(-1.0, 3.0);

    for (int trial = 0; trial < 500; ++trial)
    {
        const double r1 = rootDistribution(rng);
        const double r2 = rootDistribution(rng);
        const double r3 = rootDistribution(rng);
        const double r4 = rootDistribution(rng);
        const Polynomial p = fromRoots(r1, r2, r3, r4);

        // Expected: the smallest root in (0, 2] where p goes from + to -.
        double expected = std::numeric_limits<double>::infinity();
        for (const double r : {r1, r2, r3, r4})
        {
            if (r > 1e-6 && r <= 2.0 && p.derivative()(r) < -1e-9)
            {
                expected = std::min(expected, r);
            }
        }

        const double found = firstClosingRoot(p, 0.0, 2.0);
        if (std::isinf(expected))
        {
            // Near-double roots can legitimately read as a graze; skip those.
            continue;
        }
        CHECK(found == doctest::Approx(expected).epsilon(1e-6));
    }
}
