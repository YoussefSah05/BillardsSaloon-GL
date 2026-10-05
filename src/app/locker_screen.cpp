#include "app/locker_screen.h"

#include "gameplay/equipment.h"
#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace BilliardsSaloon
{
    namespace
    {
        std::string upper(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return text;
        }

        // Steps id through the options' ids, wrapping around.
        template <typename Option>
        void step(std::string& id, const std::vector<Option>& options, int direction)
        {
            const auto it = std::find_if(options.begin(), options.end(), [&](const Option& o) { return o.id == id; });
            const int count = static_cast<int>(options.size());
            const int index = (it == options.end()) ? 0 : static_cast<int>(it - options.begin());
            id = options[static_cast<std::size_t>(((index + direction) % count + count) % count)].id;
        }

        constexpr const char* ROW_NAMES[] = {"cloth", "rails", "trim", "pockets", "balls", "cue", "hall"};
    }

    LockerScreen::LockerScreen(UiSystem& ui, const EquipmentChoice& initial, LockerActions actions)
        : m_choice(initial)
        , m_actions(std::move(actions))
    {
        Rml::DataModelConstructor model = ui.context().CreateDataModel("locker");
        if (!model)
        {
            throw std::runtime_error("Could not create the locker data model.");
        }

        const EquipmentCatalog& catalog = equipmentCatalog();
        model.BindFunc("cloth", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.cloth, m_choice.cloth).name); });
        model.BindFunc("rails", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.rails, m_choice.rails).name); });
        model.BindFunc("trim", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.trim, m_choice.trim).name); });
        model.BindFunc("pockets", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.pockets, m_choice.pockets).name); });
        model.BindFunc("balls", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.balls, m_choice.balls).name); });
        model.BindFunc("cue", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.cues, m_choice.cue).name); });
        model.BindFunc("hall", [this, &catalog](Rml::Variant& v) { v = upper(findOption(catalog.halls, m_choice.hall).name); });
        model.Bind("gamepad", &m_gamepad);

        model.BindEventCallback("cycle", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 2)
            {
                change(arguments[0].Get<int>(), arguments[1].Get<int>());
            }
        });
        model.BindEventCallback("key", [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments)
        {
            if (arguments.size() != 2)
            {
                return;
            }
            const int key = arguments[1].Get<int>();
            if ((key == Rml::Input::KI_LEFT) || (key == Rml::Input::KI_RIGHT))
            {
                change(arguments[0].Get<int>(), (key == Rml::Input::KI_LEFT) ? -1 : 1);
                event.StopPropagation();
            }
        });
        model.BindEventCallback("back", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (m_actions.back)
            {
                m_actions.back();
            }
        });

        m_model = model.GetModelHandle();
        m_document = &ui.loadDocument("ui/locker.rml");
    }

    void LockerScreen::setVisible(bool visible)
    {
        if (visible == m_document->IsVisible())
        {
            return;
        }
        if (visible)
        {
            m_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
        }
        else
        {
            m_document->Hide();
        }
    }

    bool LockerScreen::isVisible() const
    {
        return m_document->IsVisible();
    }

    void LockerScreen::setGamepadPrompts(bool gamepad)
    {
        m_gamepad = gamepad;
        m_model.DirtyVariable("gamepad");
    }

    void LockerScreen::change(int row, int direction)
    {
        const EquipmentCatalog& catalog = equipmentCatalog();
        switch (row)
        {
            case 0: step(m_choice.cloth, catalog.cloth, direction); break;
            case 1: step(m_choice.rails, catalog.rails, direction); break;
            case 2: step(m_choice.trim, catalog.trim, direction); break;
            case 3: step(m_choice.pockets, catalog.pockets, direction); break;
            case 4: step(m_choice.balls, catalog.balls, direction); break;
            case 5: step(m_choice.cue, catalog.cues, direction); break;
            case 6: step(m_choice.hall, catalog.halls, direction); break;
            default: return;
        }
        m_model.DirtyVariable(ROW_NAMES[row]);
        if (m_actions.apply)
        {
            m_actions.apply(m_choice);
        }
    }
}
