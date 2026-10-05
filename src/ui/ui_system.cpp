#include "ui/ui_system.h"

#include "core/asset_paths.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi_Platform_GLFW.h>
#include <RmlUi_Renderer_GL3.h>

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <string>

namespace BilliardsSaloon
{
    // Interface sounds: menu items tick on focus and confirm on click.
    class MenuSoundListener final : public Rml::EventListener
    {
    public:
        explicit MenuSoundListener(const std::function<void(bool)>& hook)
            : m_hook(hook)
        {
        }

        void ProcessEvent(Rml::Event& event) override
        {
            Rml::Element* target = event.GetTargetElement();
            if ((target == nullptr) || !m_hook)
            {
                return;
            }
            // Clicks land on inner spans; walk up to the menu item.
            for (Rml::Element* e = target; e != nullptr; e = e->GetParentNode())
            {
                if (e->IsClassSet("menu-item"))
                {
                    m_hook(event.GetId() == Rml::EventId::Click);
                    return;
                }
            }
        }

    private:
        const std::function<void(bool)>& m_hook;
    };

    namespace
    {
        // Every face the stylesheets refer to. RmlUi reads family, weight and
        // style from the font files themselves.
        constexpr const char* FONT_FILES[] = {
            "fonts/Barlow-Regular.ttf",
            "fonts/Barlow-Medium.ttf",
            "fonts/Barlow-SemiBold.ttf",
            "fonts/BarlowCondensed-SemiBold.ttf",
            "fonts/BarlowCondensed-Bold.ttf",
            "fonts/BarlowCondensed-ExtraBold.ttf"
        };

        bool s_instanceAlive = false;
    }

    UiSystem::UiSystem(Window& window)
        : m_window(window)
    {
        if (s_instanceAlive)
        {
            throw std::logic_error("Only one UiSystem may exist at a time.");
        }

        Rml::String message;
        if (!RmlGL3::Initialize(&message))
        {
            throw std::runtime_error("UI renderer failed to start: " + message);
        }

        m_systemInterface = std::make_unique<SystemInterface_GLFW>(window.nativeHandle());
        m_renderInterface = std::make_unique<RenderInterface_GL3>();
        if (!*m_renderInterface)
        {
            throw std::runtime_error("UI renderer could not compile its shaders.");
        }

        Rml::SetSystemInterface(m_systemInterface.get());
        Rml::SetRenderInterface(m_renderInterface.get());

        if (!Rml::Initialise())
        {
            throw std::runtime_error("RmlUi failed to initialise.");
        }
        s_instanceAlive = true;

        const int width = window.width();
        const int height = window.height();
        m_renderInterface->SetViewport(width, height);

        m_context = Rml::CreateContext("main", Rml::Vector2i(width, height));
        if (m_context == nullptr)
        {
            Rml::Shutdown();
            s_instanceAlive = false;
            throw std::runtime_error("RmlUi could not create its context.");
        }

        // Stylesheets use dp units, which scale with the display (2x on Retina)
        // and the player's UI size setting.
        m_contentScale = window.contentScale();
        m_context->SetDensityIndependentPixelRatio(m_contentScale * m_uiScale);

        loadFonts();
        window.setEventSink(this);
    }

    UiSystem::~UiSystem()
    {
        m_window.setEventSink(nullptr);

        // RmlUi must shut down while its interfaces still exist.
        Rml::Shutdown();
        s_instanceAlive = false;

        m_renderInterface.reset();
        m_systemInterface.reset();
        RmlGL3::Shutdown();
    }

    Rml::Context& UiSystem::context()
    {
        return *m_context;
    }

    void UiSystem::loadFonts()
    {
        for (const char* file : FONT_FILES)
        {
            const std::string path = resolveAssetPath(file).string();
            if (!Rml::LoadFontFace(path))
            {
                throw std::runtime_error("Could not load font: " + path);
            }
        }
    }

    Rml::ElementDocument& UiSystem::loadDocument(std::string_view assetPath)
    {
        const std::string path = resolveAssetPath(assetPath).string();
        Rml::ElementDocument* document = m_context->LoadDocument(path);
        if (document == nullptr)
        {
            throw std::runtime_error("Could not load UI document: " + path);
        }
        document->SetClass("reduced-motion", m_reducedMotion);
        if (!m_soundListener)
        {
            m_soundListener = std::make_unique<MenuSoundListener>(m_soundHook);
        }
        document->AddEventListener(Rml::EventId::Focus, m_soundListener.get(), true);
        document->AddEventListener(Rml::EventId::Click, m_soundListener.get(), true);
        return *document;
    }

    void UiSystem::setPointerEnabled(bool enabled)
    {
        if (enabled == m_pointerEnabled)
        {
            return;
        }

        m_pointerEnabled = enabled;
        if (!enabled)
        {
            // Clear hover states so nothing stays highlighted under a hidden cursor.
            m_context->ProcessMouseLeave();
        }
    }

    void UiSystem::setUiScale(float scale)
    {
        m_uiScale = scale;
        m_context->SetDensityIndependentPixelRatio(m_contentScale * m_uiScale);
    }

    void UiSystem::setReducedMotion(bool enabled)
    {
        m_reducedMotion = enabled;
        for (int i = 0; i < m_context->GetNumDocuments(); ++i)
        {
            m_context->GetDocument(i)->SetClass("reduced-motion", enabled);
        }
    }

    void UiSystem::injectKey(int key)
    {
        const auto identifier = static_cast<Rml::Input::KeyIdentifier>(key);
        m_context->ProcessKeyDown(identifier, 0);
        m_context->ProcessKeyUp(identifier, 0);
    }

    void UiSystem::update()
    {
        m_context->Update();
    }

    void UiSystem::render()
    {
        m_renderInterface->SetViewport(m_window.width(), m_window.height());
        m_renderInterface->BeginFrame();
        m_context->Render();
        m_renderInterface->EndFrame();
    }

    void UiSystem::onKey(int key, int action, int mods)
    {
        // RmlGLFW returns true when the event was NOT consumed by the UI.
        m_consumedLastKey = !RmlGLFW::ProcessKeyCallback(m_context, key, action, mods);
    }

    void UiSystem::onChar(unsigned int codepoint)
    {
        RmlGLFW::ProcessCharCallback(m_context, codepoint);
    }

    void UiSystem::onCursorPos(double x, double y, int mods)
    {
        if (m_pointerEnabled)
        {
            RmlGLFW::ProcessCursorPosCallback(m_context, m_window.nativeHandle(), x, y, mods);
        }
    }

    void UiSystem::onCursorEnter(bool entered)
    {
        if (m_pointerEnabled)
        {
            RmlGLFW::ProcessCursorEnterCallback(m_context, entered ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    void UiSystem::onMouseButton(int button, int action, int mods)
    {
        if (m_pointerEnabled)
        {
            RmlGLFW::ProcessMouseButtonCallback(m_context, button, action, mods);
        }
    }

    void UiSystem::onScroll(double yOffset, int mods)
    {
        if (m_pointerEnabled)
        {
            RmlGLFW::ProcessScrollCallback(m_context, yOffset, mods);
        }
    }

    void UiSystem::onFramebufferSize(int width, int height)
    {
        RmlGLFW::ProcessFramebufferSizeCallback(m_context, width, height);
        m_renderInterface->SetViewport(width, height);
    }

    void UiSystem::onContentScale(float scale)
    {
        m_contentScale = scale;
        m_context->SetDensityIndependentPixelRatio(m_contentScale * m_uiScale);
    }
}
