// billiards_sim.rl: shot-level access to the simulator for reinforcement
// learning and baselines (docs/rl/rl-system-design.md, Section 4). Simulator
// frame: metres, origin at a corner of the playing surface, x across, y along.
// A strike is (speed m/s, phi deg, theta deg, a, b), as Sim::CueStrike.

#include "rl/geometry.h"
#include "rl/layouts.h"
#include "rl/shot_lab.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <string>

namespace py = pybind11;
using namespace BilliardsSaloon;

namespace
{
    const Sim::Table& standardTable()
    {
        static const Sim::Table table = Sim::buildPocketTable({});
        return table;
    }

    const std::vector<Rl::PocketMouth>& standardMouths()
    {
        static const std::vector<Rl::PocketMouth> mouths = Rl::pocketMouths(standardTable());
        return mouths;
    }

    const Sim::BallParams PARAMS;

    std::vector<glm::dvec2> toPositions(const py::array_t<double, py::array::c_style | py::array::forcecast>& array)
    {
        if ((array.ndim() != 2) || (array.shape(1) != 2) || (array.shape(0) < 1))
        {
            throw std::invalid_argument("positions must have shape (n, 2) with n >= 1 (row 0 is the cue ball)");
        }
        std::vector<glm::dvec2> positions(static_cast<std::size_t>(array.shape(0)));
        const auto view = array.unchecked<2>();
        for (py::ssize_t i = 0; i < array.shape(0); ++i)
        {
            positions[static_cast<std::size_t>(i)] = glm::dvec2(view(i, 0), view(i, 1));
        }
        return positions;
    }

    Sim::CueStrike toStrike(const double* s)
    {
        Sim::CueStrike strike;
        strike.speed = s[0];
        strike.phiDegrees = s[1];
        strike.thetaDegrees = s[2];
        strike.a = s[3];
        strike.b = s[4];
        return strike;
    }

    py::array_t<double> pointArray(const std::vector<glm::dvec2>& points)
    {
        py::array_t<double> array({static_cast<py::ssize_t>(points.size()), py::ssize_t{2}});
        auto out = array.mutable_unchecked<2>();
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            out(static_cast<py::ssize_t>(i), 0) = points[i].x;
            out(static_cast<py::ssize_t>(i), 1) = points[i].y;
        }
        return array;
    }

    py::dict outcomeDict(const Rl::ShotOutcome& outcome)
    {
        py::dict d;
        d["pocket_of"] = py::array_t<int>(static_cast<py::ssize_t>(outcome.pocketOf.size()), outcome.pocketOf.data());
        d["cushions_before_pot"] = py::array_t<int>(static_cast<py::ssize_t>(outcome.cushionsBeforePot.size()), outcome.cushionsBeforePot.data());
        d["final_positions"] = pointArray(outcome.finalPositions);
        d["pot_order"] = outcome.potOrder;
        d["scratch"] = outcome.scratch;
        d["first_contact"] = outcome.firstContact;
        d["contact_velocity"] = py::make_tuple(outcome.contactVelocity.x, outcome.contactVelocity.y);
        d["events"] = outcome.events;
        d["complete"] = outcome.complete;
        return d;
    }

    py::dict ghostDict(const Rl::GhostShot& ghost)
    {
        py::dict d;
        d["ghost"] = py::make_tuple(ghost.ghost.x, ghost.ghost.y);
        d["phi"] = ghost.phiDegrees;
        d["cut"] = ghost.cutDegrees;
        d["cue_distance"] = ghost.cueDistance;
        d["object_distance"] = ghost.objectDistance;
        return d;
    }

    py::dict layoutDict(const Rl::SingleBallLayout& layout)
    {
        py::dict d;
        d["positions"] = pointArray(layout.positions);
        d["difficulty"] = std::string(Rl::difficultyName(layout.difficulty));
        d["pocket"] = layout.pocket;
        d["ghost"] = ghostDict(layout.ghost);
        return d;
    }
}

void bindShotLab(py::module_& parent)
{
    py::module_ m = parent.def_submodule("rl", "Shot-level simulation, geometry and layouts for reinforcement learning");

    m.def("table_info", []()
    {
        const Sim::Table& table = standardTable();
        py::dict d;
        d["width"] = table.width;
        d["length"] = table.length;
        d["ball_radius"] = PARAMS.R;
        std::vector<glm::dvec2> pockets;
        std::vector<double> radii;
        for (const Sim::Pocket& p : table.pockets)
        {
            pockets.emplace_back(p.center.x, p.center.y);
            radii.push_back(p.radius);
        }
        std::vector<glm::dvec2> mouths;
        std::vector<glm::dvec2> inward;
        std::vector<double> halfWidths;
        for (const Rl::PocketMouth& mouth : standardMouths())
        {
            mouths.push_back(mouth.centre);
            inward.push_back(mouth.inward);
            halfWidths.push_back(mouth.halfWidth);
        }
        d["pockets"] = pointArray(pockets);
        d["pocket_radius"] = radii;
        d["mouths"] = pointArray(mouths);
        d["mouth_inward"] = pointArray(inward);
        d["mouth_half_width"] = halfWidths;
        return d;
    }, "The standard 9 ft table: size, ball radius, pocket drop circles and mouths.");

    m.def("simulate",
        [](const py::array_t<double, py::array::c_style | py::array::forcecast>& positions,
           const py::array_t<double, py::array::c_style | py::array::forcecast>& strike)
        {
            if (strike.size() != 5)
            {
                throw std::invalid_argument("strike must be (speed, phi, theta, a, b)");
            }
            const std::vector<glm::dvec2> layout = toPositions(positions);
            const Sim::CueStrike s = toStrike(strike.data());
            Rl::ShotOutcome outcome;
            {
                py::gil_scoped_release release;
                outcome = Rl::simulateOutcome(standardTable(), layout, s, PARAMS);
            }
            return outcomeDict(outcome);
        },
        py::arg("positions"), py::arg("strike"),
        "Plays one shot on ball 0 from a layout at rest; returns what happened.");

    m.def("simulate_batch",
        [](const py::array_t<double, py::array::c_style | py::array::forcecast>& positions,
           const py::array_t<double, py::array::c_style | py::array::forcecast>& strikes, unsigned threads)
        {
            if ((positions.ndim() != 3) || (positions.shape(2) != 2))
            {
                throw std::invalid_argument("positions must have shape (batch, n, 2)");
            }
            if ((strikes.ndim() != 2) || (strikes.shape(1) != 5) || (strikes.shape(0) != positions.shape(0)))
            {
                throw std::invalid_argument("strikes must have shape (batch, 5)");
            }
            const py::ssize_t batch = positions.shape(0);
            const py::ssize_t balls = positions.shape(1);
            std::vector<std::vector<glm::dvec2>> layouts(static_cast<std::size_t>(batch));
            std::vector<Sim::CueStrike> shots(static_cast<std::size_t>(batch));
            const auto p = positions.unchecked<3>();
            for (py::ssize_t i = 0; i < batch; ++i)
            {
                auto& layout = layouts[static_cast<std::size_t>(i)];
                layout.resize(static_cast<std::size_t>(balls));
                for (py::ssize_t j = 0; j < balls; ++j)
                {
                    layout[static_cast<std::size_t>(j)] = glm::dvec2(p(i, j, 0), p(i, j, 1));
                }
                shots[static_cast<std::size_t>(i)] = toStrike(strikes.data() + i * 5);
            }

            std::vector<Rl::ShotOutcome> outcomes;
            {
                py::gil_scoped_release release;
                outcomes = Rl::simulateBatch(standardTable(), layouts, shots, PARAMS, threads);
            }

            py::array_t<int> pocketOf({batch, balls});
            py::array_t<int> cushions({batch, balls});
            py::array_t<double> finals({batch, balls, py::ssize_t{2}});
            py::array_t<bool> scratch(batch);
            py::array_t<int> firstContact(batch);
            py::array_t<double> contactVelocity({batch, py::ssize_t{2}});
            py::array_t<int> events(batch);
            py::array_t<bool> complete(batch);
            auto po = pocketOf.mutable_unchecked<2>();
            auto cu = cushions.mutable_unchecked<2>();
            auto fi = finals.mutable_unchecked<3>();
            auto sc = scratch.mutable_unchecked<1>();
            auto fc = firstContact.mutable_unchecked<1>();
            auto cv = contactVelocity.mutable_unchecked<2>();
            auto ev = events.mutable_unchecked<1>();
            auto co = complete.mutable_unchecked<1>();
            for (py::ssize_t i = 0; i < batch; ++i)
            {
                const Rl::ShotOutcome& o = outcomes[static_cast<std::size_t>(i)];
                for (py::ssize_t j = 0; j < balls; ++j)
                {
                    po(i, j) = o.pocketOf[static_cast<std::size_t>(j)];
                    cu(i, j) = o.cushionsBeforePot[static_cast<std::size_t>(j)];
                    fi(i, j, 0) = o.finalPositions[static_cast<std::size_t>(j)].x;
                    fi(i, j, 1) = o.finalPositions[static_cast<std::size_t>(j)].y;
                }
                sc(i) = o.scratch;
                fc(i) = o.firstContact;
                cv(i, 0) = o.contactVelocity.x;
                cv(i, 1) = o.contactVelocity.y;
                ev(i) = o.events;
                co(i) = o.complete;
            }
            py::dict d;
            d["pocket_of"] = pocketOf;
            d["cushions_before_pot"] = cushions;
            d["final_positions"] = finals;
            d["scratch"] = scratch;
            d["first_contact"] = firstContact;
            d["contact_velocity"] = contactVelocity;
            d["events"] = events;
            d["complete"] = complete;
            return d;
        },
        py::arg("positions"), py::arg("strikes"), py::arg("threads") = 0U,
        "Plays many independent shots across threads (0: all cores), the GIL released.");

    m.def("ghost_aims",
        [](const py::array_t<double, py::array::c_style | py::array::forcecast>& positions, int objectIndex)
        {
            const std::vector<glm::dvec2> layout = toPositions(positions);
            if ((objectIndex < 1) || (objectIndex >= static_cast<int>(layout.size())))
            {
                throw std::invalid_argument("object_index must name an object ball");
            }
            const glm::dvec2 cue = layout[0];
            const glm::dvec2 object = layout[static_cast<std::size_t>(objectIndex)];
            const std::size_t count = standardMouths().size();
            py::array_t<double> phi(static_cast<py::ssize_t>(count));
            py::array_t<double> cut(static_cast<py::ssize_t>(count));
            py::array_t<double> cueDistance(static_cast<py::ssize_t>(count));
            py::array_t<double> objectDistance(static_cast<py::ssize_t>(count));
            py::array_t<double> ease(static_cast<py::ssize_t>(count));
            py::array_t<bool> blocked(static_cast<py::ssize_t>(count));
            for (std::size_t k = 0; k < count; ++k)
            {
                const Rl::PocketMouth& mouth = standardMouths()[k];
                const Rl::GhostShot g = Rl::ghostShot(cue, object, mouth.centre, PARAMS.R);
                const auto i = static_cast<py::ssize_t>(k);
                phi.mutable_at(i) = g.phiDegrees;
                cut.mutable_at(i) = g.cutDegrees;
                cueDistance.mutable_at(i) = g.cueDistance;
                objectDistance.mutable_at(i) = g.objectDistance;
                ease.mutable_at(i) = Rl::shotEase(g);
                blocked.mutable_at(i) = Rl::pathBlocked(layout, cue, g.ghost, PARAMS.R, 0, objectIndex) ||
                                        Rl::pathBlocked(layout, object, mouth.centre, PARAMS.R, 0, objectIndex);
            }
            py::dict d;
            d["phi"] = phi;
            d["cut"] = cut;
            d["cue_distance"] = cueDistance;
            d["object_distance"] = objectDistance;
            d["ease"] = ease;
            d["blocked"] = blocked;
            return d;
        },
        py::arg("positions"), py::arg("object_index") = 1,
        "Ghost-ball lines from the cue ball through one object ball to every pocket mouth.");

    m.def("geometry_shot",
        [](const py::array_t<double, py::array::c_style | py::array::forcecast>& positions, int objectIndex, int pocket,
           double maxCut) -> py::object
        {
            const std::vector<glm::dvec2> layout = toPositions(positions);
            const std::optional<Rl::GeometryShot> shot =
                Rl::geometryShot(standardTable(), standardMouths(), layout, objectIndex, PARAMS, maxCut, pocket);
            if (!shot)
            {
                return py::none();
            }
            py::dict d;
            d["pocket"] = shot->pocket;
            d["strike"] = py::make_tuple(shot->strike.speed, shot->strike.phiDegrees, shot->strike.thetaDegrees,
                                         shot->strike.a, shot->strike.b);
            d["ease"] = shot->ease;
            d["ghost"] = ghostDict(shot->ghost);
            return d;
        },
        py::arg("positions"), py::arg("object_index") = 1, py::arg("pocket") = -1, py::arg("max_cut") = 75.0,
        "The geometry baseline: aim at the easiest clear pocket mouth, speed from the friction model. None if no line.");

    py::class_<Rl::LayoutGenerator>(m, "LayoutGenerator")
        .def(py::init([](std::uint64_t seed) { return Rl::LayoutGenerator(standardTable(), seed, PARAMS); }), py::arg("seed"))
        .def("single_ball", [](Rl::LayoutGenerator& g, const std::string& band) { return layoutDict(g.singleBall(Rl::difficultyFromName(band))); },
             py::arg("band") = "any", "A legal cue + object ball layout in a difficulty band (easy, medium, hard, none, any).")
        .def("single_ball_within", [](Rl::LayoutGenerator& g, double maxCut, double maxCue, double maxObject, double minObject)
             {
                 return layoutDict(g.singleBallWithin(Rl::ShotLimits{maxCut, maxCue, maxObject, minObject}));
             },
             py::arg("max_cut"), py::arg("max_cue_distance"), py::arg("max_object_distance"), py::arg("min_object_distance") = 0.08,
             "A layout built around a shot within these limits (degrees, metres), for curricula.")
        .def("analyse", [](const Rl::LayoutGenerator& g, const py::array_t<double, py::array::c_style | py::array::forcecast>& positions)
             { return layoutDict(g.analyse(toPositions(positions))); },
             py::arg("positions"), "The difficulty band and easiest pocket of a given two-ball layout.")
        .def("legal_spot", [](const Rl::LayoutGenerator& g, double x, double y,
                              const py::array_t<double, py::array::c_style | py::array::forcecast>& others)
             { return g.legalSpot({x, y}, others.size() == 0 ? std::vector<glm::dvec2>{} : toPositions(others)); },
             py::arg("x"), py::arg("y"), py::arg("others"));
}
