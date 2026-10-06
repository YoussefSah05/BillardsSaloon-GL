// billiards_sim: the game's simulator, referee and classical AI for Python
// (self-play, data generation, evaluation). Built with -DBS_BUILD_PYTHON=ON.

#include "ai/ai_profile.h"
#include "ai/environment.h"
#include "ai/features.h"
#include "ai/planner.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace BilliardsSaloon;

void bindShotLab(py::module_& parent);   // rl_bindings.cpp

namespace
{
    py::dict resultDict(const Ai::StepResult& result)
    {
        py::dict d;
        d["shooter"] = result.shooter;
        d["to_act"] = result.toAct;
        d["frame_over"] = result.frameOver;
        d["winner"] = result.winner;
        d["foul"] = static_cast<int>(result.verdict.foul);
        d["turn_passed"] = result.verdict.turnPassed;
        d["potted"] = result.verdict.objectPots;
        return d;
    }

    py::array_t<float> toArray(const std::vector<float>& values)
    {
        py::array_t<float> array(static_cast<py::ssize_t>(values.size()));
        std::copy(values.begin(), values.end(), array.mutable_data());
        return array;
    }
}

PYBIND11_MODULE(billiards_sim, m)
{
    m.doc() = "Billiards Saloon simulator, WPA referee and classical AI";
    bindShotLab(m);

    py::enum_<GameDiscipline>(m, "Discipline")
        .value("EIGHT_BALL", GameDiscipline::EightBall)
        .value("NINE_BALL", GameDiscipline::NineBall)
        .value("TEN_BALL", GameDiscipline::TenBall);

    py::enum_<Ai::Decision>(m, "Decision")
        .value("BREAK", Ai::Decision::Break)
        .value("PLACE", Ai::Decision::Place)
        .value("SHOT", Ai::Decision::Shot)
        .value("CHOICE", Ai::Decision::Choice)
        .value("FRAME_OVER", Ai::Decision::FrameOver);

    py::enum_<Rules::Option>(m, "Option")
        .value("PLAY", Rules::Option::Play)
        .value("PASS_BACK", Rules::Option::PassBack)
        .value("RERACK", Rules::Option::Rerack)
        .value("RERACK_OPPONENT", Rules::Option::RerackOpponent);

    py::class_<ShotInput>(m, "ShotInput")
        .def(py::init<>())
        .def_readwrite("aim", &ShotInput::aimRadians)
        .def_readwrite("power", &ShotInput::power01)
        .def_readwrite("right", &ShotInput::strikeRight01)
        .def_readwrite("forward", &ShotInput::strikeForward01)
        .def_readwrite("elevation", &ShotInput::elevationDegrees)
        .def_readwrite("push_out", &ShotInput::pushOut)
        .def_property("call",
            [](const ShotInput& s) -> py::object
            {
                return s.call ? py::make_tuple(s.call->ball, s.call->pocket) : py::object(py::none());
            },
            [](ShotInput& s, py::object value)
            {
                if (value.is_none())
                {
                    s.call.reset();
                }
                else
                {
                    const auto t = value.cast<std::pair<int, int>>();
                    s.call = Rules::Call{t.first, t.second};
                }
            })
        .def("__repr__", [](const ShotInput& s)
        {
            return "ShotInput(aim=" + std::to_string(s.aimRadians) + ", power=" + std::to_string(s.power01) + ")";
        });

    m.attr("FEATURE_COUNT") = static_cast<int>(Ai::FEATURE_COUNT);
    m.def("profiles", []()
    {
        std::vector<std::string> ids;
        for (const Ai::AiProfile& p : Ai::aiProfiles())
        {
            ids.push_back(p.id);
        }
        return ids;
    }, "AI player ids, club to champion");

    py::class_<Ai::Environment>(m, "Environment")
        .def(py::init<GameDiscipline, std::uint32_t>(), py::arg("discipline"), py::arg("seed") = 1)
        .def("reset", &Ai::Environment::reset, py::arg("seed"), py::arg("breaker") = 0)
        .def("decision", &Ai::Environment::decision)
        .def("to_act", &Ai::Environment::toAct)
        .def("shots_played", &Ai::Environment::shotsPlayed)
        .def("winner", [](const Ai::Environment& e) { return e.frame().winner; })
        .def("features", [](const Ai::Environment& e, int player) { return toArray(Ai::encodeTable(e.observe(), player)); },
             py::arg("player"), "Table features from a player's point of view (FEATURE_COUNT floats)")
        .def("balls", [](const Ai::Environment& e)
        {
            const Ai::AiTable t = e.observe();
            py::list balls;
            for (std::size_t i = 0; i < t.numbers.size(); ++i)
            {
                balls.append(py::make_tuple(t.numbers[i], t.positions[i].x, t.positions[i].y, static_cast<bool>(t.pocketed[i])));
            }
            return balls;
        }, "(number, x, z, pocketed) for every ball, cue ball first")
        .def("shoot", [](Ai::Environment& e, const ShotInput& input) { return resultDict(e.shoot(input)); })
        .def("place", [](Ai::Environment& e, float x, float z) { return e.place(glm::vec2(x, z)); })
        .def("choose", &Ai::Environment::choose)
        .def("play_classical", [](Ai::Environment& e, const std::string& profile, std::uint32_t seed, bool withError)
        {
            py::gil_scoped_release release;
            const Ai::StepResult result = e.playClassical(Ai::findAiProfile(profile), seed, withError);
            py::gil_scoped_acquire acquire;
            return resultDict(result);
        }, py::arg("profile"), py::arg("seed"), py::arg("with_error") = true)
        .def("plan_shot", [](const Ai::Environment& e, const std::string& profile, std::uint32_t seed)
        {
            py::gil_scoped_release release;
            return Ai::planShot(e.observe(), Ai::findAiProfile(profile), seed).input;
        }, py::arg("profile"), py::arg("seed"), "The classical AI's planned shot for the player to act");
}
