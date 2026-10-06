#include "ai/planner.h"

#include "rules/shot_record.h"
#include "sim/simulate.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace BilliardsSaloon::Ai
{
    // ---- AiTable ------------------------------------------------------------------

    std::vector<int> AiTable::objectBallsOnTable() const
    {
        std::vector<int> balls;
        for (std::size_t i = 1; i < numbers.size(); ++i)
        {
            if (!pocketed[i])
            {
                balls.push_back(numbers[i]);
            }
        }
        std::sort(balls.begin(), balls.end());
        return balls;
    }

    int AiTable::indexOf(int number) const
    {
        for (std::size_t i = 0; i < numbers.size(); ++i)
        {
            if (numbers[i] == number)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        // ---- Geometry ---------------------------------------------------------------

        float segmentDistance(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
        {
            const glm::vec2 ab = b - a;
            const float t = std::clamp(glm::dot(p - a, ab) / std::max(glm::dot(ab, ab), 1.0e-9f), 0.0f, 1.0f);
            return glm::length(p - (a + t * ab));
        }

        // True if a ball can roll from a to b without touching any other ball
        // (indices in `ignore` are not obstacles).
        bool pathClear(const AiTable& table, const glm::vec2& a, const glm::vec2& b, int ignoreA, int ignoreB)
        {
            const float clearance = 2.0f * table.radius * 0.98f;
            for (std::size_t i = 0; i < table.positions.size(); ++i)
            {
                if (table.pocketed[i] || (static_cast<int>(i) == ignoreA) || (static_cast<int>(i) == ignoreB))
                {
                    continue;
                }
                if (segmentDistance(table.positions[i], a, b) < clearance)
                {
                    return false;
                }
            }
            return true;
        }

        struct PotLine
        {
            int ball {-1};          // simulator index
            int pocket {-1};
            glm::vec2 ghost {0.0f};
            float cutRadians {0.0f};
            float cueDistance {0.0f};
            float ballDistance {0.0f};
            float ease {0.0f};      // 0..1
        };

        // The ghost-ball line for potting ball into pocket from the cue ball.
        PotLine potLine(const AiTable& table, const glm::vec2& cue, int ball, int pocket)
        {
            PotLine line;
            line.ball = ball;
            line.pocket = pocket;
            const glm::vec2 b = table.positions[static_cast<std::size_t>(ball)];
            const glm::vec2 p = table.pockets[static_cast<std::size_t>(pocket)];
            const glm::vec2 toPocket = p - b;
            line.ballDistance = glm::length(toPocket);
            if (line.ballDistance < 1.0e-4f)
            {
                return line;
            }
            const glm::vec2 dir = toPocket / line.ballDistance;
            line.ghost = b - dir * (2.0f * table.radius);
            const glm::vec2 aim = line.ghost - cue;
            line.cueDistance = glm::length(aim);
            if (line.cueDistance < 1.0e-4f)
            {
                return line;
            }
            line.cutRadians = std::acos(std::clamp(glm::dot(aim / line.cueDistance, dir), -1.0f, 1.0f));
            if (line.cutRadians > glm::radians(78.0f))
            {
                return line;
            }

            // Side pockets take balls well only from in front.
            float mouth = 1.0f;
            if (std::abs(p.x) < 0.3f)
            {
                mouth = std::pow(std::abs(dir.y), 1.5f);
            }

            if (!pathClear(table, cue, line.ghost, 0, ball) || !pathClear(table, b, p, 0, ball))
            {
                return line;
            }

            const float c = std::cos(line.cutRadians);
            line.ease = c * c * mouth * std::exp(-(line.cueDistance + line.ballDistance) / 2.2f);
            return line;
        }

        std::vector<int> legalTargets(const AiTable& table)
        {
            std::vector<int> indices;
            for (const int number : Rules::legalFirstContacts(table.frame, table.objectBallsOnTable()))
            {
                const int i = table.indexOf(number);
                if (i > 0)
                {
                    indices.push_back(i);
                }
            }
            return indices;
        }

        // ---- Simulation and judging ------------------------------------------------------

        struct Outcome
        {
            Rules::Verdict verdict;
            AiTable after;
        };

        Outcome play(const AiTable& table, const ShotInput& input)
        {
            std::vector<Sim::BallState> balls;
            for (std::size_t i = 0; i < table.positions.size(); ++i)
            {
                Sim::BallState state;
                state.r = SimBridge::toSimPosition(glm::vec3(table.positions[i].x, table.radius, table.positions[i].y), table.length, table.width);
                state.r.z = table.ball.R;
                state.s = table.pocketed[i] ? Sim::MotionState::Pocketed : Sim::MotionState::Stationary;
                balls.push_back(state);
            }
            Sim::SimulationLimits limits;
            limits.maxEvents = 3000;
            const Sim::ShotTrajectory trajectory =
                Sim::simulateShot(table.table, std::move(balls), 0, toCueStrike(table.tuning, input), table.ball, {}, limits);

            Rules::ShotRecord record = Rules::recordShot(trajectory, table.numbers);
            record.call = input.call;
            record.pushOut = input.pushOut;

            Outcome outcome;
            outcome.after = table;
            outcome.verdict = Rules::judgeShot(outcome.after.frame, table.objectBallsOnTable(), record);

            const std::vector<Sim::BallState>& final = trajectory.finalState();
            for (std::size_t i = 0; i < final.size(); ++i)
            {
                outcome.after.pocketed[i] = final[i].s == Sim::MotionState::Pocketed;
                const glm::vec3 p = SimBridge::toGamePosition(final[i].r, table.length, table.width);
                outcome.after.positions[i] = glm::vec2(p.x, p.z);
            }
            // Spotted balls come back to the foot spot (near enough for planning).
            for (const int number : outcome.verdict.spot)
            {
                const int i = outcome.after.indexOf(number);
                if (i >= 0)
                {
                    outcome.after.pocketed[static_cast<std::size_t>(i)] = false;
                    outcome.after.positions[static_cast<std::size_t>(i)] = glm::vec2(0.25f * table.length, 0.0f);
                }
            }
            return outcome;
        }

        // How good a shot's result is for the player who took it.
        double score(const AiTable& before, const Outcome& outcome)
        {
            const Rules::Verdict& v = outcome.verdict;
            const int me = before.frame.shooter;

            if (v.frameOver)
            {
                return (v.winner == me) ? 100.0 : -100.0;
            }
            if (v.foul != Rules::Foul::None)
            {
                const bool threeFoulRisk = (before.discipline != GameDiscipline::EightBall) && (v.foulsInRow >= 2);
                return threeFoulRisk ? -70.0 : -40.0;
            }

            double value = 0.0;
            // A little credit for clearing balls of my own (8-ball) or any (rotation).
            value += 2.0 * static_cast<double>(v.objectPots.size());

            if (outcome.after.frame.shooter == me && (outcome.after.frame.choice == Rules::Choice::None))
            {
                value += 20.0 + 12.0 * positionValue(outcome.after);
            }
            else
            {
                // The opponent's turn: how hard is their next shot?
                AiTable theirs = outcome.after;
                theirs.frame.shooter = Rules::otherPlayer(me);
                value -= 18.0 * positionValue(theirs);
            }
            return value;
        }

        // ---- Candidates ---------------------------------------------------------------------

        struct Candidate
        {
            ShotInput input;
            bool safety {false};
            std::string description;
            double quick {-1.0e9};
            double expected {-1.0e9};
        };

        std::string pocketLabel(const AiTable& table, int pocket)
        {
            const glm::vec2 p = table.pockets[static_cast<std::size_t>(pocket)];
            const char* end = (p.x > 0.3f) ? "FOOT" : (p.x < -0.3f) ? "HEAD" : "SIDE";
            return std::string(end) + ((p.y < 0.0f) ? " RIGHT" : " LEFT");
        }

        void addPots(const AiTable& table, const AiProfile& profile, std::vector<Candidate>& out)
        {
            const glm::vec2 cue = table.cueBall();
            const bool callAll = table.discipline == GameDiscipline::TenBall;
            for (const int ball : legalTargets(table))
            {
                for (int pocket = 0; pocket < static_cast<int>(table.pockets.size()); ++pocket)
                {
                    const PotLine line = potLine(table, cue, ball, pocket);
                    if (line.ease < 0.02f)
                    {
                        continue;
                    }
                    const float travel = line.cueDistance + line.ballDistance;
                    const float base = std::clamp(0.14f + 0.16f * travel + 0.12f * (line.cutRadians / 1.4f) + 0.06f * profile.powerStyle, 0.08f, 0.85f);
                    const struct { float power; float forward; const char* spin; } variants[] = {
                        {base, 0.0f, "stun"}, {base, 0.45f, "follow"}, {base + 0.06f, -0.45f, "draw"},
                        {std::min(base + 0.18f, 0.95f), 0.0f, "firm"}};
                    for (const auto& variant : variants)
                    {
                        Candidate c;
                        c.input.aimRadians = angleFromDirection(line.ghost - cue);
                        c.input.power01 = variant.power;
                        c.input.strikeForward01 = variant.forward;
                        const int number = table.numbers[static_cast<std::size_t>(ball)];
                        if (callAll || (number == 8))
                        {
                            c.input.call = Rules::Call{number, pocket};
                        }
                        c.description = std::to_string(number) + " to " + pocketLabel(table, pocket) + ", " + variant.spin;
                        out.push_back(c);
                    }
                }
            }
        }

        void addSafeties(const AiTable& table, std::vector<Candidate>& out)
        {
            const glm::vec2 cue = table.cueBall();
            for (const int ball : legalTargets(table))
            {
                const glm::vec2 b = table.positions[static_cast<std::size_t>(ball)];
                const float distance = glm::length(b - cue);
                if ((distance < 1.0e-3f) || !pathClear(table, cue, b, 0, ball))
                {
                    continue;
                }
                const float centre = angleFromDirection(b - cue);
                for (const float fullness : {0.0f, 0.5f, -0.5f, 0.85f, -0.85f})
                {
                    const float offset = std::asin(std::clamp(fullness * 2.0f * table.radius / distance, -1.0f, 1.0f));
                    for (const float power : {0.12f, 0.22f})
                    {
                        Candidate c;
                        c.safety = true;
                        c.input.aimRadians = centre + offset;
                        c.input.power01 = power;
                        const int number = table.numbers[static_cast<std::size_t>(ball)];
                        if (table.discipline == GameDiscipline::TenBall)
                        {
                            c.input.call = Rules::Call{number, 0};   // nothing is meant to drop
                        }
                        c.description = "safety on the " + std::to_string(number);
                        out.push_back(c);
                    }
                }
            }
        }

        // Snookered: try the cue ball off the cushions in all directions.
        void addKicks(const AiTable& table, std::vector<Candidate>& out)
        {
            for (int i = 0; i < 48; ++i)
            {
                for (const float power : {0.3f, 0.5f})
                {
                    Candidate c;
                    c.safety = true;
                    c.input.aimRadians = 2.0f * PI * static_cast<float>(i) / 48.0f;
                    c.input.power01 = power;
                    c.description = "kick";
                    out.push_back(c);
                }
            }
            (void)table;
        }
    }

    // ---- Public ----------------------------------------------------------------------------

    namespace
    {
        // A planned pot wins ties with a safety that happens to pot (it is the
        // shot the player means); the player's style shifts safeties up or down.
        double intent(const Candidate& candidate, const AiProfile& profile)
        {
            return candidate.safety ? profile.safetyBias : 1.0;
        }
    }

    double positionValue(const AiTable& table)
    {
        if (table.pocketed.front())
        {
            return 0.5;   // ball in hand: a fair position
        }
        float best = 0.0f;
        int good = 0;
        for (const int ball : legalTargets(table))
        {
            for (int pocket = 0; pocket < static_cast<int>(table.pockets.size()); ++pocket)
            {
                const float ease = potLine(table, table.cueBall(), ball, pocket).ease;
                best = std::max(best, ease);
                good += (ease > 0.25f) ? 1 : 0;
            }
        }
        return 0.75 * std::min(best / 0.6f, 1.0f) + 0.25 * std::min(good, 3) / 3.0;
    }

    ShotInput withExecutionError(ShotInput input, const AiProfile& profile, std::mt19937& random)
    {
        std::normal_distribution<float> unit(0.0f, 1.0f);
        input.aimRadians += glm::radians(profile.aimNoiseDegrees) * unit(random);
        input.power01 = std::clamp(input.power01 * (1.0f + profile.powerNoise * unit(random)), 0.02f, 1.0f);
        input.strikeRight01 = std::clamp(input.strikeRight01 + profile.spinNoise * unit(random), -0.75f, 0.75f);
        input.strikeForward01 = std::clamp(input.strikeForward01 + profile.spinNoise * unit(random), -0.75f, 0.75f);
        return input;
    }

    AiShot planShot(const AiTable& table, const AiProfile& profile, std::uint32_t seed)
    {
        std::vector<Candidate> candidates;
        addPots(table, profile, candidates);
        addSafeties(table, candidates);
        if (candidates.empty())
        {
            addKicks(table, candidates);
        }

        // Stage 1: every candidate once, as planned.
        for (Candidate& c : candidates)
        {
            const Outcome outcome = play(table, c.input);
            // A "safety" that pots and keeps the table (or wins) is really a pot.
            const bool keeps = outcome.verdict.frameOver ? (outcome.verdict.winner == table.frame.shooter)
                                                         : (outcome.after.frame.shooter == table.frame.shooter);
            if (c.safety && keeps && (outcome.verdict.foul == Rules::Foul::None))
            {
                c.safety = false;
                c.description += " (pots)";
            }
            c.quick = score(table, outcome) + intent(c, profile);
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.quick > b.quick; });

        // Stage 2: the finalists with this player's error.
        std::mt19937 random(seed);
        const std::size_t finalists = std::min<std::size_t>(candidates.size(), 10);
        for (std::size_t i = 0; i < finalists; ++i)
        {
            double sum = 0.0;
            for (int s = 0; s < profile.samples; ++s)
            {
                sum += score(table, play(table, withExecutionError(candidates[i].input, profile, random)));
            }
            candidates[i].expected = sum / std::max(profile.samples, 1) + intent(candidates[i], profile);
        }

        AiShot shot;
        if (candidates.empty())
        {
            // Nothing found at all (should not happen): roll towards the table's centre.
            shot.input.aimRadians = angleFromDirection(-table.cueBall());
            shot.input.power01 = 0.3f;
            shot.description = "no shot found";
            return shot;
        }
        const auto best = std::max_element(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(std::max<std::size_t>(finalists, 1)),
            [](const Candidate& a, const Candidate& b) { return a.expected < b.expected; });
        shot.input = best->input;
        shot.score = best->expected;
        shot.safety = best->safety;
        shot.description = best->description;
        return shot;
    }

    AiShot planBreak(const AiTable& table, const AiProfile& profile)
    {
        // Into the ball nearest the cue ball at the head of the rack (the 1 in rotation games).
        const glm::vec2 cue = table.cueBall();
        int target = -1;
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t i = 1; i < table.positions.size(); ++i)
        {
            if (table.pocketed[i])
            {
                continue;
            }
            const float d = glm::length(table.positions[i] - cue);
            if (d < nearest)
            {
                nearest = d;
                target = static_cast<int>(i);
            }
        }
        AiShot shot;
        shot.description = "break";
        if (target < 0)
        {
            return shot;
        }
        shot.input.aimRadians = angleFromDirection(table.positions[static_cast<std::size_t>(target)] - cue);
        shot.input.power01 = profile.breakPower;
        shot.input.strikeForward01 = -0.1f;   // a touch of draw keeps the cue ball central
        return shot;
    }

    glm::vec2 planCueBallPlacement(const AiTable& table, const AiProfile& profile, std::uint32_t seed)
    {
        const float r = table.radius;
        const float maxX = (table.frame.ballInHand == Rules::BallInHand::BehindHeadString) ? table.headStringX : 0.5f * table.length - r;
        const auto legal = [&](const glm::vec2& p)
        {
            if ((p.x < -0.5f * table.length + r) || (p.x > maxX) || (std::abs(p.y) > 0.5f * table.width - r))
            {
                return false;
            }
            for (std::size_t i = 1; i < table.positions.size(); ++i)
            {
                if (!table.pocketed[i] && (glm::length(table.positions[i] - p) < 2.0f * r + 0.002f))
                {
                    return false;
                }
            }
            return true;
        };

        // Spots behind each legal ball on its line to each pocket, near and far,
        // a little off line for position; ranked by how good the position is.
        std::vector<std::pair<double, glm::vec2>> spots;
        for (const int ball : legalTargets(table))
        {
            const glm::vec2 b = table.positions[static_cast<std::size_t>(ball)];
            for (const glm::vec2& pocket : table.pockets)
            {
                const glm::vec2 dir = glm::normalize(pocket - b);
                const glm::vec2 side(-dir.y, dir.x);
                for (const float back : {0.2f, 0.4f, 0.7f})
                {
                    for (const float off : {0.0f, 0.06f, -0.06f})
                    {
                        const glm::vec2 p = b - dir * (2.0f * r + back) + side * off;
                        if (!legal(p))
                        {
                            continue;
                        }
                        AiTable trial = table;
                        trial.positions.front() = p;
                        trial.pocketed.front() = false;
                        spots.emplace_back(positionValue(trial), p);
                    }
                }
            }
        }
        if (spots.empty())
        {
            // Nothing lines up: the head spot, or the nearest free place to it.
            for (float d = 0.0f; d < 0.6f; d += 0.03f)
            {
                for (const float sign : {1.0f, -1.0f})
                {
                    const glm::vec2 p(table.headStringX, sign * d);
                    if (legal(p))
                    {
                        return p;
                    }
                }
            }
            return glm::vec2(table.headStringX, 0.0f);
        }

        // Plan the best few for real and keep the one with the best shot.
        std::sort(spots.begin(), spots.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        AiProfile quick = profile;
        quick.samples = 2;
        double bestScore = -1.0e9;
        glm::vec2 best = spots.front().second;
        for (std::size_t i = 0; i < std::min<std::size_t>(spots.size(), 4); ++i)
        {
            AiTable trial = table;
            trial.positions.front() = spots[i].second;
            trial.pocketed.front() = false;
            trial.frame.ballInHand = Rules::BallInHand::None;
            const double s = planShot(trial, quick, seed + static_cast<std::uint32_t>(i)).score;
            if (s > bestScore)
            {
                bestScore = s;
                best = spots[i].second;
            }
        }
        return best;
    }

    Rules::Option planChoice(const AiTable& table, const AiProfile& profile, std::uint32_t seed)
    {
        AiTable mine = table;
        mine.frame.shooter = table.frame.chooser;
        mine.frame.choice = Rules::Choice::None;
        AiProfile quick = profile;
        quick.samples = 3;

        switch (table.frame.choice)
        {
            case Rules::Choice::EightOnBreak:
                return Rules::Option::Play;
            case Rules::Choice::IllegalBreak:
                return (planShot(mine, quick, seed).score > 5.0) ? Rules::Option::Play : Rules::Option::Rerack;
            case Rules::Choice::AfterPushOut:
            case Rules::Choice::AfterUncalledPot:
                return (planShot(mine, quick, seed).score > 5.0) ? Rules::Option::Play : Rules::Option::PassBack;
            case Rules::Choice::None:
                break;
        }
        return Rules::Option::Play;
    }
}
