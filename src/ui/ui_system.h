#pragma once

#include "platform/window.h"

#include <functional>
#include <memory>
#include <string_view>

namespace Rml
{
    class Context;
    class ElementDocument;
}

class SystemInterface_GLFW;
class RenderInterface_GL3;

namespace BilliardsSaloon
{
    // Owns RmlUi for the lifetime of the game: interfaces, fonts and the one
    // UI context. Draws after the 3D scene each frame. Only one may exist.
    class UiSystem final : public WindowEventSink
    {
    public:
        explicit UiSystem(Window& window);
        ~UiSystem() override;

        UiSystem(const UiSystem&) = delete;
        UiSystem& operator=(const UiSystem&) = delete;
        UiSystem(UiSystem&&) = delete;
        UiSystem& operator=(UiSystem&&) = delete;

        [[nodiscard]] Rml::Context& context();

        // Loads an .rml document from the assets folder (e.g. "ui/main_menu.rml").
        // Throws std::runtime_error if it cannot be loaded.
        [[nodiscard]] Rml::ElementDocument& loadDocument(std::string_view assetPath);

        // While false (cursor captured for aiming), mouse events skip the UI.
        void setPointerEnabled(bool enabled);

        // Player text/UI size on top of the display's own scale (1.0 to 1.5).
        void setUiScale(float scale);

        // Adds the reduced-motion class to every document, which turns off transitions.
        void setReducedMotion(bool enabled);

        // Called when a menu item takes focus (confirm = false) or is clicked
        // (confirm = true), for interface sounds.
        void setSoundHook(std::function<void(bool confirm)> hook) { m_soundHook = std::move(hook); }

        // Sends a key press to the UI as if typed (gamepad menu navigation).
        // key is an Rml::Input::KeyIdentifier.
        void injectKey(int key);

        // True when the last key event was used by the UI (e.g. menu navigation).
        [[nodiscard]] bool consumedLastKey() const { return m_consumedLastKey; }

        void update();
        void render();

        void onKey(int key, int action, int mods) override;
        void onChar(unsigned int codepoint) override;
        void onCursorPos(double x, double y, int mods) override;
        void onCursorEnter(bool entered) override;
        void onMouseButton(int button, int action, int mods) override;
        void onScroll(double yOffset, int mods) override;
        void onFramebufferSize(int width, int height) override;
        void onContentScale(float scale) override;

    private:
        void loadFonts();

        Window& m_window;
        std::unique_ptr<SystemInterface_GLFW> m_systemInterface;
        std::unique_ptr<RenderInterface_GL3> m_renderInterface;
        Rml::Context* m_context {nullptr};
        bool m_pointerEnabled {true};
        float m_uiScale {1.0f};
        float m_contentScale {1.0f};
        bool m_reducedMotion {false};
        bool m_consumedLastKey {false};
        std::function<void(bool)> m_soundHook;
        std::unique_ptr<class MenuSoundListener> m_soundListener;
    };
}
