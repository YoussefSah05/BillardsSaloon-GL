#include "rules/referee.h"

#include <algorithm>

namespace BilliardsSaloon::Rules
{
    namespace
    {
        constexpr int FOULS_TO_LOSE = 3;
        constexpr int BREAK_BALLS_TO_RAIL = 4;

        [[nodiscard]] bool contains(const std::vector<int>& values, int value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        [[nodiscard]] int lowestBall(const std::vector<int>& onTable)
        {
            return onTable.empty() ? -1 : *std::min_element(onTable.begin(), onTable.end());
        }

        [[nodiscard]] bool groupCleared(Group group, const std::vector<int>& onTable)
        {
            return std::none_of(onTable.begin(), onTable.end(), [group](int ball) { return groupOf(ball) == group; });
        }

        // The usual fouls of a shot after the break.
        [[nodiscard]] Foul shotFoul(const ShotRecord& shot, const std::vector<int>& legalFirst)
        {
            if (shot.potted(CUE_BALL))
            {
                return Foul::CueBallPocketed;
            }
            if (shot.firstContact < 0)
            {
                return Foul::NoBallHit;
            }
            if (!contains(legalFirst, shot.firstContact))
            {
                return Foul::WrongBallFirst;
            }
            if (shot.objectPots().empty() && !shot.railAfterContact)
            {
                return Foul::NoRail;
            }
            return Foul::None;
        }

        [[nodiscard]] bool breakDroveEnough(const ShotRecord& shot)
        {
            return !shot.objectPots().empty() || (shot.objectBallsToRail >= BREAK_BALLS_TO_RAIL);
        }

        void endFrame(FrameState& state, Verdict& verdict, int winner, FrameEnd end)
        {
            state.phase = Phase::FrameOver;
            state.winner = winner;
            state.end = end;
            state.ballInHand = BallInHand::None;
            verdict.frameOver = true;
            verdict.winner = winner;
            verdict.end = end;
        }

        void passTurn(FrameState& state, Verdict& verdict)
        {
            state.shooter = otherPlayer(state.shooter);
            verdict.turnPassed = true;
        }

        void offerChoice(FrameState& state, Verdict& verdict, Choice choice, int chooser)
        {
            state.choice = choice;
            state.chooser = chooser;
            verdict.choice = choice;
            verdict.chooser = chooser;
        }

        // A foul in 9-ball or 10-ball: ball in hand anywhere, and the third in
        // a row loses the frame.
        void rotationFoul(FrameState& state, Verdict& verdict)
        {
            const int shooter = state.shooter;
            verdict.foulsInRow = ++state.consecutiveFouls[static_cast<std::size_t>(shooter)];
            if (verdict.foulsInRow >= FOULS_TO_LOSE)
            {
                endFrame(state, verdict, otherPlayer(shooter), FrameEnd::ThreeFouls);
                return;
            }
            state.ballInHand = BallInHand::Anywhere;
            passTurn(state, verdict);
        }

        void judgeEightBallBreak(FrameState& state, const ShotRecord& shot, Verdict& verdict)
        {
            const int breaker = state.shooter;
            const int incoming = otherPlayer(breaker);
            const bool scratch = shot.potted(CUE_BALL);

            state.phase = Phase::Open;   // the table is always open after the break
            state.ballInHand = BallInHand::None;
            verdict.foul = scratch ? Foul::CueBallPocketed : Foul::None;

            if (shot.potted(8))
            {
                // WPA 8-ball 3.6: the breaker (or, after a scratch, the incoming
                // player) spots the 8 and plays, or re-racks. Spot it now; a
                // re-rack resets the table anyway.
                verdict.spot.push_back(8);
                const int chooser = scratch ? incoming : breaker;
                if (scratch)
                {
                    state.ballInHand = BallInHand::BehindHeadString;
                }
                state.shooter = chooser;
                verdict.turnPassed = chooser != breaker;
                offerChoice(state, verdict, Choice::EightOnBreak, chooser);
                return;
            }

            if (!breakDroveEnough(shot))
            {
                // WPA 8-ball 3.5: not a foul in itself; the incoming player may
                // accept the table or re-rack.
                verdict.illegalBreak = true;
                if (scratch)
                {
                    state.ballInHand = BallInHand::BehindHeadString;
                }
                passTurn(state, verdict);
                offerChoice(state, verdict, Choice::IllegalBreak, incoming);
                return;
            }

            if (scratch)
            {
                // WPA 8-ball 3.7: ball in hand behind the head string.
                state.ballInHand = BallInHand::BehindHeadString;
                passTurn(state, verdict);
                return;
            }

            if (verdict.objectPots.empty())
            {
                passTurn(state, verdict);
            }
        }

        void judgeEightBallShot(FrameState& state, const std::vector<int>& onTable, const ShotRecord& shot, Verdict& verdict)
        {
            const int shooter = state.shooter;
            const std::size_t index = static_cast<std::size_t>(shooter);
            const Group group = state.groups[index];
            const bool onEight = (state.phase == Phase::Groups) && groupCleared(group, onTable);

            verdict.foul = shotFoul(shot, legalFirstContacts(state, onTable));
            const bool foul = verdict.foul != Foul::None;
            state.ballInHand = BallInHand::None;

            if (shot.potted(8))
            {
                const std::optional<int> pocket = shot.pocketOf(8);
                const bool called = shot.call.has_value() && (shot.call->ball == 8) && (shot.call->pocket == *pocket);
                if (foul)
                {
                    endFrame(state, verdict, otherPlayer(shooter), FrameEnd::EightBallOnFoul);
                }
                else if (!onEight)
                {
                    endFrame(state, verdict, otherPlayer(shooter), FrameEnd::EightBallEarly);
                }
                else if (!called)
                {
                    endFrame(state, verdict, otherPlayer(shooter), FrameEnd::EightBallWrongPocket);
                }
                else
                {
                    endFrame(state, verdict, shooter, FrameEnd::GameBallPotted);
                }
                return;
            }

            if (foul)
            {
                state.ballInHand = BallInHand::Anywhere;
                passTurn(state, verdict);
                return;
            }

            if (state.phase == Phase::Open)
            {
                // The first ball down decides the groups (WPA decides by the
                // called ball; this game does not call ordinary balls).
                if (!verdict.objectPots.empty())
                {
                    const Group first = groupOf(verdict.objectPots.front());
                    state.groups[index] = first;
                    state.groups[static_cast<std::size_t>(otherPlayer(shooter))] =
                        (first == Group::Solids) ? Group::Stripes : Group::Solids;
                    state.phase = Phase::Groups;
                    verdict.groupsAssigned = true;
                    return;
                }
                passTurn(state, verdict);
                return;
            }

            const bool pottedOwn = std::any_of(verdict.objectPots.begin(), verdict.objectPots.end(),
                [group](int ball) { return groupOf(ball) == group; });
            if (!pottedOwn)
            {
                passTurn(state, verdict);
            }
        }

        void judgeRotationBreak(FrameState& state, const std::vector<int>& onTable, const ShotRecord& shot, Verdict& verdict)
        {
            const int breaker = state.shooter;
            const int game = gameBall(state.discipline);

            state.phase = Phase::Play;
            state.ballInHand = BallInHand::None;

            if (shot.potted(CUE_BALL))
            {
                verdict.foul = Foul::CueBallPocketed;
            }
            else if (shot.firstContact < 0)
            {
                verdict.foul = Foul::NoBallHit;
            }
            else if (shot.firstContact != lowestBall(onTable))
            {
                verdict.foul = Foul::WrongBallFirst;
            }
            else if (!breakDroveEnough(shot))
            {
                verdict.foul = Foul::IllegalBreak;
            }

            const bool foul = verdict.foul != Foul::None;

            if (shot.potted(game))
            {
                // 9-ball: a legal break that pockets the 9 wins. 10-ball: the 10
                // on the break is always spotted.
                if (!foul && (state.discipline == GameDiscipline::NineBall))
                {
                    endFrame(state, verdict, breaker, FrameEnd::GameBallPotted);
                    return;
                }
                verdict.spot.push_back(game);
            }

            if (foul)
            {
                rotationFoul(state, verdict);
                return;
            }

            state.consecutiveFouls[static_cast<std::size_t>(breaker)] = 0;
            // The player taking the next shot may push out.
            state.pushOutAvailable = true;
            if (verdict.objectPots.empty())
            {
                passTurn(state, verdict);
            }
        }

        void judgeRotationShot(FrameState& state, const std::vector<int>& onTable, const ShotRecord& shot, Verdict& verdict)
        {
            const int shooter = state.shooter;
            const int game = gameBall(state.discipline);
            const bool pushOut = shot.pushOut && state.pushOutAvailable;

            state.pushOutAvailable = false;
            state.ballInHand = BallInHand::None;

            if (pushOut)
            {
                // WPA 9-ball 5.4: no contact or cushion needed; balls pocketed
                // stay down except the game ball, which is spotted.
                verdict.pushOut = true;
                if (shot.potted(game))
                {
                    verdict.spot.push_back(game);
                }
                if (shot.potted(CUE_BALL))
                {
                    verdict.foul = Foul::CueBallPocketed;
                    rotationFoul(state, verdict);
                    return;
                }
                state.consecutiveFouls[static_cast<std::size_t>(shooter)] = 0;
                passTurn(state, verdict);
                offerChoice(state, verdict, Choice::AfterPushOut, state.shooter);
                return;
            }

            verdict.foul = shotFoul(shot, legalFirstContacts(state, onTable));
            const bool foul = verdict.foul != Foul::None;

            if (state.discipline == GameDiscipline::TenBall)
            {
                const bool calledDown = !foul && shot.call.has_value() &&
                    shot.potted(shot.call->ball) && (*shot.pocketOf(shot.call->ball) == shot.call->pocket);

                if (calledDown && (shot.call->ball == game))
                {
                    endFrame(state, verdict, shooter, FrameEnd::GameBallPotted);
                    return;
                }
                if (shot.potted(game))
                {
                    verdict.spot.push_back(game);
                }
                if (foul)
                {
                    rotationFoul(state, verdict);
                    return;
                }

                state.consecutiveFouls[static_cast<std::size_t>(shooter)] = 0;
                if (calledDown)
                {
                    return;
                }
                passTurn(state, verdict);
                if (!verdict.objectPots.empty())
                {
                    // WPA 10-ball 6.4: a ball down but not the called one in the
                    // called pocket; the incoming player may hand the shot back.
                    offerChoice(state, verdict, Choice::AfterUncalledPot, state.shooter);
                }
                return;
            }

            if (shot.potted(game))
            {
                if (!foul)
                {
                    endFrame(state, verdict, shooter, FrameEnd::GameBallPotted);
                    return;
                }
                verdict.spot.push_back(game);
            }

            if (foul)
            {
                rotationFoul(state, verdict);
                return;
            }

            state.consecutiveFouls[static_cast<std::size_t>(shooter)] = 0;
            if (verdict.objectPots.empty())
            {
                passTurn(state, verdict);
            }
        }
    }

    std::vector<Option> optionsFor(Choice choice)
    {
        switch (choice)
        {
            case Choice::IllegalBreak:
                return {Option::Play, Option::Rerack, Option::RerackOpponent};
            case Choice::EightOnBreak:
                return {Option::Play, Option::Rerack};
            case Choice::AfterPushOut:
            case Choice::AfterUncalledPot:
                return {Option::Play, Option::PassBack};
            case Choice::None:
                break;
        }
        return {};
    }

    bool ShotRecord::potted(int ball) const
    {
        return pocketOf(ball).has_value();
    }

    std::optional<int> ShotRecord::pocketOf(int ball) const
    {
        for (const Pot& pot : pots)
        {
            if (pot.ball == ball)
            {
                return pot.pocket;
            }
        }
        return std::nullopt;
    }

    std::vector<int> ShotRecord::objectPots() const
    {
        std::vector<int> balls;
        for (const Pot& pot : pots)
        {
            if (pot.ball != CUE_BALL)
            {
                balls.push_back(pot.ball);
            }
        }
        return balls;
    }

    int otherPlayer(int player)
    {
        return (player == 0) ? 1 : 0;
    }

    FrameState startFrame(GameDiscipline discipline, int breaker)
    {
        FrameState state;
        state.discipline = discipline;
        state.shooter = breaker;
        state.breaker = breaker;
        return state;
    }

    Group groupOf(int ball)
    {
        if ((ball >= 1) && (ball <= 7))
        {
            return Group::Solids;
        }
        if ((ball >= 9) && (ball <= 15))
        {
            return Group::Stripes;
        }
        return Group::None;
    }

    int gameBall(GameDiscipline discipline)
    {
        switch (discipline)
        {
            case GameDiscipline::EightBall:
                return 8;
            case GameDiscipline::NineBall:
                return 9;
            case GameDiscipline::TenBall:
                return 10;
        }
        return 8;
    }

    std::vector<int> legalFirstContacts(const FrameState& state, const std::vector<int>& onTable)
    {
        if (state.discipline != GameDiscipline::EightBall)
        {
            const int lowest = lowestBall(onTable);
            return (lowest < 0) ? std::vector<int>{} : std::vector<int>{lowest};
        }

        std::vector<int> legal;
        const Group group = state.groups[static_cast<std::size_t>(state.shooter)];

        if ((state.phase == Phase::Groups) && groupCleared(group, onTable))
        {
            if (contains(onTable, 8))
            {
                legal.push_back(8);
            }
            return legal;
        }

        for (const int ball : onTable)
        {
            if ((ball == 8) && (state.phase != Phase::Break))
            {
                continue;
            }
            if ((state.phase != Phase::Groups) || (groupOf(ball) == group))
            {
                legal.push_back(ball);
            }
        }
        return legal;
    }

    bool callRequired(const FrameState& state, const std::vector<int>& onTable)
    {
        if ((state.phase == Phase::Break) || (state.phase == Phase::FrameOver))
        {
            return false;
        }

        switch (state.discipline)
        {
            case GameDiscipline::EightBall:
                return (state.phase == Phase::Groups) &&
                    groupCleared(state.groups[static_cast<std::size_t>(state.shooter)], onTable);
            case GameDiscipline::NineBall:
                return false;
            case GameDiscipline::TenBall:
                return true;   // a declared push-out ignores the call
        }
        return false;
    }

    Verdict judgeShot(FrameState& state, const std::vector<int>& onTable, const ShotRecord& shot)
    {
        Verdict verdict;
        verdict.shooter = state.shooter;
        verdict.breakShot = state.phase == Phase::Break;
        verdict.objectPots = shot.objectPots();
        verdict.foulsInRow = state.consecutiveFouls[static_cast<std::size_t>(state.shooter)];

        if ((state.phase == Phase::FrameOver) || (state.choice != Choice::None))
        {
            return verdict;
        }

        if (state.discipline == GameDiscipline::EightBall)
        {
            if (verdict.breakShot)
            {
                judgeEightBallBreak(state, shot, verdict);
            }
            else
            {
                judgeEightBallShot(state, onTable, shot, verdict);
            }
        }
        else if (verdict.breakShot)
        {
            judgeRotationBreak(state, onTable, shot, verdict);
        }
        else
        {
            judgeRotationShot(state, onTable, shot, verdict);
        }

        if ((verdict.foul == Foul::None) && !verdict.frameOver)
        {
            verdict.foulsInRow = state.consecutiveFouls[static_cast<std::size_t>(verdict.shooter)];
        }
        return verdict;
    }

    Verdict judgeTimeFoul(FrameState& state)
    {
        Verdict verdict;
        verdict.shooter = state.shooter;
        verdict.foulsInRow = state.consecutiveFouls[static_cast<std::size_t>(state.shooter)];
        if ((state.phase == Phase::FrameOver) || (state.choice != Choice::None))
        {
            return verdict;
        }

        verdict.foul = Foul::TimeOut;
        state.pushOutAvailable = false;
        if (state.discipline == GameDiscipline::EightBall)
        {
            state.ballInHand = BallInHand::Anywhere;
            passTurn(state, verdict);
        }
        else
        {
            rotationFoul(state, verdict);
        }
        return verdict;
    }

    ChoiceResult applyChoice(FrameState& state, Option option)
    {
        const std::vector<Option> options = optionsFor(state.choice);
        if (std::find(options.begin(), options.end(), option) == options.end())
        {
            return {};
        }

        const int chooser = state.chooser;
        state.choice = Choice::None;
        state.chooser = -1;

        switch (option)
        {
            case Option::Play:
                state.shooter = chooser;
                return {.accepted = true};

            case Option::PassBack:
                state.shooter = otherPlayer(chooser);
                state.ballInHand = BallInHand::None;
                return {.accepted = true};

            case Option::Rerack:
                state = startFrame(state.discipline, chooser);
                return {.accepted = true, .rerack = true};

            case Option::RerackOpponent:
                state = startFrame(state.discipline, otherPlayer(chooser));
                return {.accepted = true, .rerack = true};
        }
        return {};
    }
}
