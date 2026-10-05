#include "sim/roots.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double INF = std::numeric_limits<double>::infinity();

        // Size of the terms of p at t, used to judge "zero" relative to the
        // magnitudes that cancelled to produce p(t).
        double termScale(const Polynomial& p, double t)
        {
            double scale = 0.0;
            double power = 1.0;
            for (const double coefficient : p.c)
            {
                scale += std::abs(coefficient * power);
                power *= t;
            }
            return scale;
        }

        bool isZero(const Polynomial& p, double t, double value)
        {
            return std::abs(value) <= 1.0e-13 * termScale(p, t);
        }

        // Root in [a, b] where p(a) and p(b) have opposite signs.
        double solveBracketed(const Polynomial& p, const Polynomial& dp, double a, double b)
        {
            double fa = p(a);
            double x = 0.5 * (a + b);

            for (int iteration = 0; iteration < 200; ++iteration)
            {
                const double fx = p(x);
                if (fx == 0.0)
                {
                    return x;
                }

                // Keep the bracket.
                if ((fx < 0.0) == (fa < 0.0))
                {
                    a = x;
                    fa = fx;
                }
                else
                {
                    b = x;
                }

                if ((b - a) <= 4.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(x)))
                {
                    break;
                }

                // Newton step when it stays inside the bracket, else bisect.
                const double slope = dp(x);
                double next = (slope != 0.0) ? x - fx / slope : 0.5 * (a + b);
                if (!(next > a && next < b))
                {
                    next = 0.5 * (a + b);
                }
                x = next;
            }

            return x;
        }
    }

    double Polynomial::operator()(double t) const
    {
        // Horner's rule.
        return (((c[4] * t + c[3]) * t + c[2]) * t + c[1]) * t + c[0];
    }

    Polynomial Polynomial::derivative() const
    {
        Polynomial d;
        d.c = {c[1], 2.0 * c[2], 3.0 * c[3], 4.0 * c[4], 0.0};
        return d;
    }

    int Polynomial::degree() const
    {
        for (int i = 4; i >= 0; --i)
        {
            if (c[static_cast<std::size_t>(i)] != 0.0)
            {
                return i;
            }
        }
        return -1;
    }

    std::vector<double> realRootsInInterval(const Polynomial& p, double lo, double hi)
    {
        std::vector<double> roots;
        if (!(lo <= hi))
        {
            return roots;
        }

        const int degree = p.degree();
        if (degree <= 0)
        {
            return roots;   // constant: no isolated roots
        }

        if (degree == 1)
        {
            const double root = -p.c[0] / p.c[1];
            if (root >= lo && root <= hi)
            {
                roots.push_back(root);
            }
            return roots;
        }

        // Split [lo, hi] at the turning points; p is monotonic between them.
        const Polynomial dp = p.derivative();
        std::vector<double> knots {lo};
        for (const double critical : realRootsInInterval(dp, lo, hi))
        {
            if (critical > knots.back())
            {
                knots.push_back(critical);
            }
        }
        if (hi > knots.back())
        {
            knots.push_back(hi);
        }

        auto addRoot = [&](double root)
        {
            if (roots.empty() || (root - roots.back()) > 1.0e-12 * std::max(1.0, std::abs(root)))
            {
                roots.push_back(root);
            }
        };

        for (std::size_t i = 0; i < knots.size(); ++i)
        {
            const double a = knots[i];
            const double fa = p(a);

            // A knot that is (numerically) on zero: an endpoint root or a
            // double root at a turning point.
            if (isZero(p, a, fa))
            {
                addRoot(a);
                continue;
            }

            if (i + 1 < knots.size())
            {
                const double b = knots[i + 1];
                const double fb = p(b);
                if (!isZero(p, b, fb) && ((fa < 0.0) != (fb < 0.0)))
                {
                    addRoot(solveBracketed(p, dp, a, b));
                }
            }
        }

        return roots;
    }

    double firstClosingRoot(const Polynomial& p, double lo, double hi)
    {
        const Polynomial dp = p.derivative();

        for (const double root : realRootsInInterval(p, lo, hi))
        {
            if (root <= lo)
            {
                continue;
            }

            // Closing: p decreasing through zero. At a double root p only
            // touches zero (a graze); skip it.
            const double slope = dp(root);
            if (slope < 0.0)
            {
                return root;
            }
            if (slope == 0.0 || std::abs(slope) <= 1.0e-12 * termScale(dp, root))
            {
                // Inflection through zero still closes if p is negative just after.
                const double probe = root + 1.0e-9 * std::max(1.0, root);
                if (probe <= hi && p(probe) < 0.0)
                {
                    return root;
                }
            }
        }

        return INF;
    }
}
