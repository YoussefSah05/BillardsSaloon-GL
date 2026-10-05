#include "sim/resolve.h"

#include <algorithm>
#include <cmath>

// Ported from pooltool's resolvers (Apache-2.0): stick_ball/instantaneous_point,
// stick_ball/squirt, ball_ball/frictional_inelastic (+ Alciatore friction) and
// ball_cushion/han_2005.
namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;

        // Rotation about z by angle (pooltool's coordinate_rotation).
        glm::dvec3 rotateZ(const glm::dvec3& v, double angle)
        {
            const double c = std::cos(angle);
            const double s = std::sin(angle);
            return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
        }

        glm::dvec3 surfaceVelocity(const glm::dvec3& v, const glm::dvec3& w, const glm::dvec3& d, double R)
        {
            return v + glm::cross(w, R * d);
        }

        glm::dvec3 tangentSurfaceVelocity(const glm::dvec3& v, const glm::dvec3& w, const glm::dvec3& d, double R)
        {
            return (v - glm::dot(v, d) * d) + glm::cross(w, R * d);
        }

        // Ball-ball friction falls with sliding speed (Alciatore, TP A-14).
        double alciatoreFriction(double relativeSurfaceSpeed)
        {
            return 9.951e-3 + 0.108 * std::exp(-1.088 * relativeSurfaceSpeed);
        }

        double squirtAngle(double ballMass, double endMass, double a)
        {
            const double massRatio = ballMass / endMass;
            const double A = 1.0 - a * a;
            return -std::atan2(2.5 * a * std::sqrt(A), 1.0 + massRatio + 2.5 * A);
        }
    }

    BallState resolveStrike(const BallState& ball, const CueStrike& strike, const BallParams& params, const CueSpecs& cue)
    {
        const double theta = strike.thetaDegrees * PI / 180.0;
        const double phi = strike.phiDegrees * PI / 180.0;
        const double R = params.R;
        const double m = params.m;

        // Contact point on the ball, rotated by the cue elevation.
        const double cueC = std::sqrt(std::max(0.0, 1.0 - strike.a * strike.a - strike.b * strike.b));
        const double a = R * strike.a;
        const double c = R * (std::cos(theta) * cueC - std::sin(theta) * strike.b);
        const double b = R * (std::sin(theta) * cueC + std::cos(theta) * strike.b);

        const double inertia = 0.4 * R * R;   // I / m
        const double sinT = std::sin(theta);
        const double cosT = std::cos(theta);
        const double temp = a * a + (b * cosT) * (b * cosT) + (c * sinT) * (c * sinT) - 2.0 * b * c * cosT * sinT;
        const double speed = 2.0 * strike.speed / (1.0 + m / cue.M + temp / inertia);

        const glm::dvec3 vBall = -speed * glm::dvec3(0.0, cosT, sinT);
        const glm::dvec3 wBall = (speed / inertia) * glm::dvec3(-c * sinT + b * cosT, a * sinT, -a * cosT);

        BallState next = ball;
        next.v = rotateZ(vBall, phi + PI / 2.0);
        next.w = rotateZ(wBall, phi + PI / 2.0);

        // Squirt: the cue ball leaves slightly off the cue line, away from the english.
        next.v = rotateZ(next.v, squirtAngle(m, cue.endMass, strike.a));
        next.v.z = 0.0;   // planar model
        next.s = classify(next, R);
        return next;
    }

    void resolveBallBall(BallState& ballA, BallState& ballB, const BallParams& params)
    {
        const double R = params.R;
        const glm::dvec3 delta = ballB.r - ballA.r;
        const glm::dvec3 normal = glm::normalize(glm::dvec3(delta.x, delta.y, 0.0));

        // Place the balls exactly in contact, plus a hair, along the normal.
        const glm::dvec3 middle = 0.5 * (ballA.r + ballB.r);
        ballA.r = middle - normal * (R + 0.5 * MIN_DIST);
        ballB.r = middle + normal * (R + 0.5 * MIN_DIST);

        const double friction = alciatoreFriction(glm::length(
            tangentSurfaceVelocity(ballA.v, ballA.w, normal, R) - tangentSurfaceVelocity(ballB.v, ballB.w, -normal, R)));
        const double e = params.e_b;

        // Work in a frame where the line of centres is +x.
        const double angle = std::atan2(normal.y, normal.x);
        glm::dvec3 v1 = rotateZ(ballA.v, -angle);
        glm::dvec3 w1 = rotateZ(ballA.w, -angle);
        glm::dvec3 v2 = rotateZ(ballB.v, -angle);
        glm::dvec3 w2 = rotateZ(ballB.w, -angle);
        const glm::dvec3 unitX(1.0, 0.0, 0.0);

        const double v1nFinal = 0.5 * ((1.0 - e) * v1.x + (1.0 + e) * v2.x);
        const double v2nFinal = 0.5 * ((1.0 + e) * v1.x + (1.0 - e) * v2.x);
        const double deltaNormal = std::abs(v2nFinal - v1nFinal);
        const double w1n = w1.x;
        const double w2n = w2.x;

        v1.x = 0.0;
        v2.x = 0.0;
        w1.x = 0.0;
        w2.x = 0.0;

        glm::dvec3 v1f = v1;
        glm::dvec3 w1f = w1;
        glm::dvec3 v2f = v2;
        glm::dvec3 w2f = w2;

        const glm::dvec3 contactSlip = surfaceVelocity(v1, w1, unitX, R) - surfaceVelocity(v2, w2, -unitX, R);
        bool sticks = glm::length(contactSlip) <= EPS;

        if (!sticks)
        {
            // Sliding friction for the whole impact.
            const glm::dvec3 dv1 = friction * deltaNormal * -glm::normalize(contactSlip);
            const glm::dvec3 dw1 = (2.5 / R) * glm::cross(unitX, dv1);
            v1f = v1 + dv1;
            w1f = w1 + dw1;
            v2f = v2 - dv1;
            w2f = w2 + dw1;

            const glm::dvec3 slipAfter = surfaceVelocity(v1f, w1f, unitX, R) - surfaceVelocity(v2f, w2f, -unitX, R);
            sticks = glm::dot(contactSlip, slipAfter) <= 0.0;   // friction would reverse the slip
        }

        if (sticks)
        {
            // The contact stops slipping during the impact.
            const glm::dvec3 dv1 = -(1.0 / 7.0) * (v1 - v2 + R * glm::cross(w1 + w2, unitX));
            const glm::dvec3 dw1 = -(5.0 / 14.0) * (glm::cross(unitX, v1 - v2) / R + w1 + w2);
            v1f = v1 + dv1;
            w1f = w1 + dw1;
            v2f = v2 - dv1;
            w2f = w2 + dw1;
        }

        v1f.x = v1nFinal;
        v2f.x = v2nFinal;
        w1f.x = w1n;
        w2f.x = w2n;

        ballA.v = rotateZ(v1f, angle);
        ballA.w = rotateZ(w1f, angle);
        ballB.v = rotateZ(v2f, angle);
        ballB.w = rotateZ(w2f, angle);
        ballA.v.z = 0.0;
        ballB.v.z = 0.0;
        ballA.s = classify(ballA, R);
        ballB.s = classify(ballB, R);
    }

    BallState resolveCushion(const BallState& ball, const glm::dvec3& normal, double cushionHeight, const BallParams& params)
    {
        const double R = params.R;
        const double m = params.m;

        // Cushion frame: the normal (towards the cushion) is +x.
        const double psi = std::atan2(normal.y, normal.x);
        glm::dvec3 v = rotateZ(ball.v, -psi);
        glm::dvec3 w = rotateZ(ball.w, -psi);

        if (v.x <= 0.0)
        {
            return ball;   // already leaving the cushion
        }

        const double e = params.e_c;
        const double mu = params.f_c;
        const double thetaA = std::asin(cushionHeight / R - 1.0);
        const double sinA = std::sin(thetaA);
        const double cosA = std::cos(thetaA);

        // Han (2005), equations 14–23.
        const double sx = v.x * sinA - v.z * cosA + R * w.y;
        const double sy = -v.y - R * w.z * cosA + R * w.x * sinA;
        const double c = -v.x * cosA;

        const double II = 0.4 * m * R * R;
        const double A = 3.5 / m;
        const double B = 1.0 / m;

        const double PzE = -(1.0 + e) * c / B;
        const double slip = std::sqrt(sx * sx + sy * sy);
        const double PzS = slip / A;

        double PxE = 0.0;
        double PyE = 0.0;
        if (PzS <= mu * PzE)
        {
            PxE = sx / A;   // the contact stops slipping
            PyE = sy / A;
        }
        else
        {
            PxE = mu * PzE * sx / slip;   // slides throughout
            PyE = mu * PzE * sy / slip;
        }

        const double PX = -PxE * sinA - PzE * cosA;
        const double PY = PyE;
        const double PZ = PxE * cosA - PzE * sinA;

        v.x += PX / m;
        v.y += PY / m;
        w.x += -R / II * PY * sinA;
        w.y += R / II * (PX * sinA - PZ * cosA);
        w.z += R / II * PY * cosA;

        BallState next = ball;
        next.v = rotateZ(v, psi);
        next.w = rotateZ(w, psi);
        next.v.z = 0.0;
        next.s = classify(next, R);
        return next;
    }

    BallState resolvePocket(const BallState& ball)
    {
        BallState next = ball;
        next.v = glm::dvec3(0.0);
        next.w = glm::dvec3(0.0);
        next.s = MotionState::Pocketed;
        return next;
    }
}
