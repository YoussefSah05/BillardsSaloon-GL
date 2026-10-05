#pragma once

#include "core/settings.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <functional>

namespace Rml
{
    class ElementDocument;
}

namespace BilliardsSaloon
{
    class UiSystem;

    struct LockerActions
    {
        // Called with the full choice after every change.
        std::function<void(const EquipmentChoice&)> apply;
        std::function<void()> back;
    };

    // The Locker (assets/ui/locker.rml): cloth, rails, trim, pockets, ball
    // set, cue and hall lighting, cycled like settings rows.
    class LockerScreen
    {
    public:
        LockerScreen(UiSystem& ui, const EquipmentChoice& initial, LockerActions actions);

        void setVisible(bool visible);
        [[nodiscard]] bool isVisible() const;
        void setGamepadPrompts(bool gamepad);

    private:
        void change(int row, int direction);

        EquipmentChoice m_choice;
        bool m_gamepad {false};
        LockerActions m_actions;
        Rml::DataModelHandle m_model;
        Rml::ElementDocument* m_document {nullptr};
    };
}
