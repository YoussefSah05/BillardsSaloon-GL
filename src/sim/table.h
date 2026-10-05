#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace BilliardsSaloon::Sim
{
    // Pocket-table dimensions (metres, degrees). Defaults: WPA 9 ft table with
    // pooltool's pocket geometry.
    struct PocketTableSpec
    {
        double length {2.54};                 // playing surface, along y
        double width {1.27};                  // playing surface, along x
        double cushionWidth {2.0 * 0.0254};
        double cushionHeight {0.64 * 2.0 * 0.028575};   // nose ≈ 64% of ball diameter
        double cushionNoseRadius {0.001};
        double cornerPocketWidth {0.118};
        double cornerPocketAngle {5.3};
        double cornerPocketDepth {0.0417};
        double cornerPocketRadius {0.062};
        double cornerJawRadius {0.02095};
        double sidePocketWidth {0.137};
        double sidePocketAngle {7.14};
        double sidePocketDepth {0.0685};
        double sidePocketRadius {0.0645};
        double sideJawRadius {0.00795};
    };

    // A straight cushion: the nose is a cylinder of noseRadius along p1→p2
    // at height p1.z.
    struct LinearCushion
    {
        glm::dvec3 p1 {0.0};
        glm::dvec3 p2 {0.0};
        double noseRadius {0.001};
    };

    // A rounded jaw tip: a vertical cylinder.
    struct CircularCushion
    {
        glm::dvec3 center {0.0};   // z = cushion height
        double radius {0.0};
    };

    // A ball whose centre enters this circle drops into the pocket.
    struct Pocket
    {
        glm::dvec3 center {0.0};   // z = 0
        double radius {0.0};
    };

    struct Table
    {
        double length {0.0};
        double width {0.0};
        double cushionHeight {0.0};
        std::vector<LinearCushion> linear;
        std::vector<CircularCushion> circular;
        std::vector<Pocket> pockets;
    };

    // Origin at one corner of the playing surface; x across, y along, z up.
    [[nodiscard]] Table buildPocketTable(const PocketTableSpec& spec);
}
