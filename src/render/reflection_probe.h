#pragma once

#include <glm/glm.hpp>

namespace Septhuit
{
    class Shader;

    // One reflection cube map of the room, captured from a point above the
    // table and prefiltered into a mip chain by roughness (mip 0 a mirror,
    // the last level fully rough). The scene shader corrects its parallax
    // against the room's box, so nearby walls reflect where they are.
    class ReflectionProbe
    {
    public:
        static constexpr int SIZE = 256;
        static constexpr int LEVELS = 6;   // 256 .. 8 texels

        ReflectionProbe() = default;
        ~ReflectionProbe();
        ReflectionProbe(const ReflectionProbe&) = delete;
        ReflectionProbe& operator=(const ReflectionProbe&) = delete;

        // Binds face f of the raw capture for drawing (SIZE x SIZE viewport).
        void bindCaptureFace(int face);
        // Builds the prefiltered chain from the raw capture with `prefilter`
        // (prefilter.frag); needs a bound empty vertex array.
        void prefilter(Shader& prefilter);

        void bind(int unit) const;
        [[nodiscard]] bool ready() const { return m_ready; }

    private:
        void create();

        unsigned int m_framebuffer {0};
        unsigned int m_depth {0};
        unsigned int m_capture {0};     // raw radiance, with mipmaps for filtered sampling
        unsigned int m_filtered {0};    // the prefiltered chain the scene samples
        bool m_ready {false};
    };
}
