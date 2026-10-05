#pragma once

#include <array>
#include <vector>

namespace BilliardsSaloon::Sim
{
    // A real polynomial of degree at most 4: c[0] + c[1]·t + … + c[4]·t⁴.
    struct Polynomial
    {
        std::array<double, 5> c {};

        [[nodiscard]] double operator()(double t) const;
        [[nodiscard]] Polynomial derivative() const;
        [[nodiscard]] int degree() const;   // -1 for the zero polynomial
    };

    // All real roots in [lo, hi], ascending. Double roots (where the curve only
    // touches zero) are included. Exact to near machine precision: the interval
    // is split at the polynomial's turning points, then each monotonic piece is
    // solved by safeguarded Newton–bisection.
    [[nodiscard]] std::vector<double> realRootsInInterval(const Polynomial& p, double lo, double hi);

    // The earliest t in (lo, hi] at which p falls from positive to zero, i.e.
    // a separation function reaching contact while closing. Grazing touches
    // (p stays non-negative) do not count. Returns +infinity if none.
    [[nodiscard]] double firstClosingRoot(const Polynomial& p, double lo, double hi);
}
