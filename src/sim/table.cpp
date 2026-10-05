#include "sim/table.h"

#include <cmath>

// Geometry ported from pooltool's objects/table/layout.py (Apache-2.0):
// create_pocket_table_cushion_segments and create_pocket_table_pockets.
namespace BilliardsSaloon::Sim
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;
    }

    Table buildPocketTable(const PocketTableSpec& s)
    {
        const double cw = s.cushionWidth;
        const double ca = (s.cornerPocketAngle + 45.0) * PI / 180.0;
        const double sa = s.sidePocketAngle * PI / 180.0;
        const double pw = s.cornerPocketWidth;
        const double sw = s.sidePocketWidth;
        const double h = s.cushionHeight;
        const double rc = s.cornerJawRadius;
        const double rs = s.sideJawRadius;
        const double dc = rc / std::tan((PI / 2.0 + ca) / 2.0);
        const double ds = rs / std::tan((PI / 2.0 + sa) / 2.0);
        const double l = s.length;
        const double w = s.width;
        const double cos45 = std::cos(PI / 4.0);
        const double nose = s.cushionNoseRadius;

        Table table;
        table.length = l;
        table.width = w;
        table.cushionHeight = h;

        auto line = [&](double x1, double y1, double x2, double y2)
        {
            table.linear.push_back(LinearCushion{glm::dvec3(x1, y1, h), glm::dvec3(x2, y2, h), nose});
        };
        auto arc = [&](double x, double y, double radius)
        {
            table.circular.push_back(CircularCushion{glm::dvec3(x, y, h), radius});
        };

        // Long and short rails between the pockets.
        line(0, pw * cos45 + dc, 0, (l - sw) / 2 - ds);                 // 3
        line(0, (l + sw) / 2 + ds, 0, -pw * cos45 + l - dc);            // 6
        line(w, pw * cos45 + dc, w, (l - sw) / 2 - ds);                 // 15
        line(w, (l + sw) / 2 + ds, w, -pw * cos45 + l - dc);            // 12
        line(pw * cos45 + dc, 0, -pw * cos45 + w - dc, 0);              // 18
        line(pw * cos45 + dc, l, -pw * cos45 + w - dc, l);              // 9

        // Side-pocket jaws.
        line(-cw, (l + sw) / 2 - cw * std::sin(sa), -ds * std::cos(sa), (l + sw) / 2 - ds * std::sin(sa));          // 5
        line(-cw, (l - sw) / 2 + cw * std::sin(sa), -ds * std::cos(sa), (l - sw) / 2 + ds * std::sin(sa));          // 4
        line(w + cw, (l + sw) / 2 - cw * std::sin(sa), w + ds * std::cos(sa), (l + sw) / 2 - ds * std::sin(sa));     // 13
        line(w + cw, (l - sw) / 2 + cw * std::sin(sa), w + ds * std::cos(sa), (l - sw) / 2 + ds * std::sin(sa));     // 14

        // Corner-pocket jaws.
        line(pw * cos45 - cw * std::tan(ca), -cw, pw * cos45 - dc * std::sin(ca), -dc * std::cos(ca));                        // 1
        line(-cw, pw * cos45 - cw * std::tan(ca), -dc * std::cos(ca), pw * cos45 - dc * std::sin(ca));                        // 2
        line(pw * cos45 - cw * std::tan(ca), cw + l, pw * cos45 - dc * std::sin(ca), l + dc * std::cos(ca));                  // 8
        line(-cw, -pw * cos45 + cw * std::tan(ca) + l, -dc * std::cos(ca), -pw * cos45 + l + dc * std::sin(ca));              // 7
        line(cw + w, -pw * cos45 + cw * std::tan(ca) + l, w + dc * std::cos(ca), -pw * cos45 + l + dc * std::sin(ca));        // 11
        line(-pw * cos45 + cw * std::tan(ca) + w, cw + l, -pw * cos45 + w + dc * std::sin(ca), l + dc * std::cos(ca));        // 10
        line(cw + w, pw * cos45 - cw * std::tan(ca), w + dc * std::cos(ca), pw * cos45 - dc * std::sin(ca));                  // 16
        line(-pw * cos45 + cw * std::tan(ca) + w, -cw, -pw * cos45 + w + dc * std::sin(ca), -dc * std::cos(ca));              // 17

        // Rounded jaw tips.
        arc(pw * cos45 + dc, -rc, rc);                    // 1t
        arc(-rc, pw * cos45 + dc, rc);                    // 2t
        arc(-rs, l / 2 - sw / 2 - ds, rs);                // 4t
        arc(-rs, l / 2 + sw / 2 + ds, rs);                // 5t
        arc(-rc, l - (pw * cos45 + dc), rc);              // 7t
        arc(pw * cos45 + dc, l + rc, rc);                 // 8t
        arc(w - pw * cos45 - dc, l + rc, rc);             // 10t
        arc(w + rc, l - (pw * cos45 + dc), rc);           // 11t
        arc(w + rs, l / 2 + sw / 2 + ds, rs);             // 13t
        arc(w + rs, l / 2 - sw / 2 - ds, rs);             // 14t
        arc(w + rc, pw * cos45 + dc, rc);                 // 16t
        arc(w - pw * cos45 - dc, -rc, rc);                // 17t

        // Pockets: corner centres move diagonally outward with depth.
        const double cD = s.cornerPocketDepth / std::sqrt(2.0);
        const double sD = s.sidePocketDepth;
        const double cr = s.cornerPocketRadius;
        const double sr = s.sidePocketRadius;
        table.pockets = {
            Pocket{glm::dvec3(-cD, -cD, 0.0), cr},
            Pocket{glm::dvec3(-sD, l / 2, 0.0), sr},
            Pocket{glm::dvec3(-cD, l + cD, 0.0), cr},
            Pocket{glm::dvec3(w + cD, -cD, 0.0), cr},
            Pocket{glm::dvec3(w + sD, l / 2, 0.0), sr},
            Pocket{glm::dvec3(w + cD, l + cD, 0.0), cr},
        };

        return table;
    }
}
