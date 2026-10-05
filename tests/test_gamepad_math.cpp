#include "core/gamepad_math.h"

#include <doctest/doctest.h>

using namespace BilliardsSaloon;

TEST_CASE("the dead zone hides stick drift and still reaches full deflection")
{
    CHECK(applyDeadZone(0.1f, 0.2f) == 0.0f);
    CHECK(applyDeadZone(-0.19f, 0.2f) == 0.0f);
    CHECK(applyDeadZone(1.0f, 0.2f) == doctest::Approx(1.0f));
    CHECK(applyDeadZone(-1.0f, 0.2f) == doctest::Approx(-1.0f));
    CHECK(applyDeadZone(0.6f, 0.2f) == doctest::Approx(0.5f));
}

TEST_CASE("holding a direction steps once, waits, then repeats")
{
    HoldRepeater repeater(0.4f, 0.1f);

    CHECK(repeater.update(true, 0.016f) == 1);    // press
    CHECK(repeater.update(true, 0.3f) == 0);      // still inside the initial delay
    CHECK(repeater.update(true, 0.15f) == 1);     // delay passed
    CHECK(repeater.update(true, 0.22f) == 2);     // repeats due at 0.5 s and 0.6 s, not yet 0.7 s

    CHECK(repeater.update(false, 0.016f) == 0);   // release resets
    CHECK(repeater.update(true, 0.016f) == 1);
}
