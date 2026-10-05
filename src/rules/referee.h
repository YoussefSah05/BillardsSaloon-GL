#pragma once

#include "gameplay/game_variant.h"

#include <array>
#include <optional>
#include <vector>

// The referee: WPA World Standardized Rules for 8-ball, 9-ball and 10-ball as
// pure functions over a small frame state, so they can be tested clause by
// clause and reused headless by the AI. Balls are identified by number; the
// cue ball is 0. Where the game simplifies a rule, the comment says so.
namespace BilliardsSaloon::Rules
{
    inline constexpr int CUE_BALL = 0;

    enum class Group
    {
        None,
        Solids,     // 1-7
        Stripes     // 9-15
    };

    enum class Phase
    {
        Break,      // the next shot is the break
        Open,       // 8-ball: groups not yet decided
        Groups,     // 8-ball: groups decided
        Play,       // 9-ball and 10-ball after the break
        FrameOver
    };

    enum class BallInHand
    {
        None,
        BehindHeadString,   // the break, and 8-ball after a scratch on the break
        Anywhere
    };

    enum class Foul
    {
        None,
        CueBallPocketed,
        NoBallHit,
        WrongBallFirst,
        NoRail,             // after contact, no ball pocketed and no ball reached a cushion
        IllegalBreak        // 9-ball and 10-ball: fewer than four balls to a cushion and none pocketed
    };

    enum class FrameEnd
    {
        None,
        GameBallPotted,         // the 8, 9 or 10 legally: shooter wins
        EightBallEarly,         // 8 pocketed before the group was cleared: shooter loses
        EightBallOnFoul,        // 8 pocketed on a foul: shooter loses
        EightBallWrongPocket,   // 8 pocketed in a pocket that was not called: shooter loses
        ThreeFouls              // third foul in a row (9-ball, 10-ball): shooter loses
    };

    // A decision the rules hand to a player before play continues.
    enum class Choice
    {
        None,
        IllegalBreak,       // 8-ball: accept the table or re-rack
        EightOnBreak,       // 8-ball: spot the 8 and play, or re-rack
        AfterPushOut,       // 9/10-ball: play from here or hand the shot back
        AfterUncalledPot    // 10-ball: play from here or hand the shot back
    };

    enum class Option
    {
        Play,               // the chooser shoots from the current position
        PassBack,           // the other player shoots
        Rerack,             // re-rack; the chooser breaks
        RerackOpponent      // re-rack; the other player breaks
    };

    [[nodiscard]] std::vector<Option> optionsFor(Choice choice);

    struct Call
    {
        int ball {0};
        int pocket {0};     // simulator pocket index
    };

    struct Pot
    {
        int ball {0};
        int pocket {0};
    };

    // What happened on one shot, as the referee needs it (see shot_record.h).
    struct ShotRecord
    {
        int firstContact {-1};          // first object ball the cue ball touched, -1 if none
        std::vector<Pot> pots;          // in the order they dropped, cue ball included
        bool railAfterContact {false};  // any ball touched a cushion after the first contact
        int objectBallsToRail {0};      // distinct object balls that touched a cushion
        bool pushOut {false};           // declared before the shot
        std::optional<Call> call;

        [[nodiscard]] bool potted(int ball) const;
        [[nodiscard]] std::optional<int> pocketOf(int ball) const;
        [[nodiscard]] std::vector<int> objectPots() const;
    };

    struct FrameState
    {
        GameDiscipline discipline {GameDiscipline::EightBall};
        Phase phase {Phase::Break};
        int shooter {0};
        int breaker {0};
        std::array<Group, 2> groups {Group::None, Group::None};
        std::array<int, 2> consecutiveFouls {0, 0};
        BallInHand ballInHand {BallInHand::BehindHeadString};
        bool pushOutAvailable {false};
        Choice choice {Choice::None};
        int chooser {-1};
        int winner {-1};
        FrameEnd end {FrameEnd::None};
    };

    // What the referee said about a shot, for the table, the HUD and stats.
    struct Verdict
    {
        int shooter {0};
        Foul foul {Foul::None};
        bool breakShot {false};
        bool illegalBreak {false};      // 8-ball: not a foul, but the opponent may re-rack
        bool pushOut {false};
        bool groupsAssigned {false};
        bool turnPassed {false};
        std::vector<int> objectPots;    // object balls pocketed, in order
        std::vector<int> spot;          // balls the table must put back on the foot spot
        int foulsInRow {0};             // the shooter's consecutive fouls after this shot
        bool frameOver {false};
        int winner {-1};
        FrameEnd end {FrameEnd::None};
        Choice choice {Choice::None};
        int chooser {-1};
    };

    [[nodiscard]] int otherPlayer(int player);

    // A fresh rack with breaker to break from behind the head string.
    [[nodiscard]] FrameState startFrame(GameDiscipline discipline, int breaker);

    [[nodiscard]] Group groupOf(int ball);

    // The game ball: 8, 9 or 10.
    [[nodiscard]] int gameBall(GameDiscipline discipline);

    // Balls the shooter may legally contact first. onTable lists object balls
    // still on the table. 8-ball, open table: any ball but the 8 (WPA allows
    // either group; the 8 first is treated as a foul).
    [[nodiscard]] std::vector<int> legalFirstContacts(const FrameState& state, const std::vector<int>& onTable);

    // True when the shot needs a called ball and pocket: the 8 in 8-ball;
    // every shot after the break in 10-ball except a push-out. Other 8-ball
    // shots count any pocketed ball of the shooter's group (no calling of
    // obvious balls).
    [[nodiscard]] bool callRequired(const FrameState& state, const std::vector<int>& onTable);

    // Judges a shot and advances state. onTable lists the object balls on the
    // table before the shot. Must not be called while a choice is pending or
    // after the frame is over.
    [[nodiscard]] Verdict judgeShot(FrameState& state, const std::vector<int>& onTable, const ShotRecord& shot);

    struct ChoiceResult
    {
        bool accepted {false};
        bool rerack {false};    // re-rack; state is a fresh frame with its breaker set
    };

    // Applies the chooser's answer to the pending choice.
    ChoiceResult applyChoice(FrameState& state, Option option);
}
