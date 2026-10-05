#include "app/overlay_screens.h"

#include <doctest/doctest.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace BilliardsSaloon;

namespace
{
    // A camera at the origin looking down -Z, like the renderer builds it.
    FrameView makeView(float aspectRatio)
    {
        FrameView view;
        view.position = glm::vec3(0.0f);
        view.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        view.up = glm::vec3(0.0f, 1.0f, 0.0f);
        view.right = glm::vec3(1.0f, 0.0f, 0.0f);
        view.viewProjection =
            glm::perspective(glm::radians(52.0f), aspectRatio, 0.05f, 60.0f) *
            glm::lookAt(view.position, view.position + view.forward, view.up);
        return view;
    }

    MenuScreenModel threeEntryMenu()
    {
        MenuScreenModel model;
        model.entries = {"A", "B", "C"};
        model.firstEntryOffset = 0.18f;
        model.entrySpacing = 0.15f;
        return model;
    }
}

TEST_CASE("the cursor over a menu card selects that card")
{
    const FrameView view = makeView(16.0f / 9.0f);
    const MenuScreenModel model = threeEntryMenu();

    for (std::size_t i = 0; i < model.entries.size(); ++i)
    {
        glm::vec2 centerNdc;
        REQUIRE(projectToNdc(view, menuCardCenter(view, model, i), centerNdc));

        const std::optional<std::size_t> hit = menuEntryAt(view, model, centerNdc);
        REQUIRE(hit.has_value());
        CHECK(*hit == i);
    }
}

TEST_CASE("the cursor between, beside or above the cards selects nothing")
{
    const FrameView view = makeView(16.0f / 9.0f);
    const MenuScreenModel model = threeEntryMenu();

    glm::vec2 first;
    glm::vec2 second;
    REQUIRE(projectToNdc(view, menuCardCenter(view, model, 0), first));
    REQUIRE(projectToNdc(view, menuCardCenter(view, model, 1), second));

    CHECK_FALSE(menuEntryAt(view, model, 0.5f * (first + second)).has_value());
    CHECK_FALSE(menuEntryAt(view, model, glm::vec2(0.95f, first.y)).has_value());
    CHECK_FALSE(menuEntryAt(view, model, glm::vec2(0.0f, 0.99f)).has_value());
}

TEST_CASE("card edges stay hit-testable on a narrow window")
{
    const FrameView view = makeView(4.0f / 3.0f);
    const MenuScreenModel model = threeEntryMenu();

    // Just inside the right edge of the middle card.
    const glm::vec3 nearEdge =
        menuCardCenter(view, model, 1) + view.right * (0.5f * MENU_CARD_SCALE.x - 0.01f);
    glm::vec2 ndc;
    REQUIRE(projectToNdc(view, nearEdge, ndc));

    const std::optional<std::size_t> hit = menuEntryAt(view, model, ndc);
    REQUIRE(hit.has_value());
    CHECK(*hit == 1);
}
