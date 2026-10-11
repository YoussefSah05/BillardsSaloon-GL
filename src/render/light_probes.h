#pragma once

#include "scene/spherical_harmonics.h"

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace Septhuit
{
    // Indirect light from a grid of probes. Each probe sees the lit room as
    // a small cube map; its radiance is projected onto nine SH coefficients
    // per colour, stored in seven 3D textures (27 floats packed into RGBA)
    // that the scene shader interpolates trilinearly between probes.
    class LightProbes
    {
    public:
        static constexpr int FACE_SIZE = 32;
        static constexpr int SH_TEXTURES = 7;

        LightProbes() = default;
        ~LightProbes();
        LightProbes(const LightProbes&) = delete;
        LightProbes& operator=(const LightProbes&) = delete;

        // Prepares an atlas with a FACE_SIZE tile per cube face of every probe
        // and binds it for drawing.
        void beginCapture(int probeCount);
        // Viewport and scissor on one face's tile.
        void bindTile(int probe, int face) const;
        // Reads the atlas back and projects every probe onto SH.
        [[nodiscard]] std::vector<ShRgb9> finishCapture(int probeCount) const;

        // Uploads probe SH as the 3D textures (x fastest, then y, then z).
        void upload(const std::vector<ShRgb9>& probes, const glm::ivec3& counts);
        void bindTextures(int firstUnit) const;

        [[nodiscard]] bool ready() const { return m_ready; }

        // The view basis of cube face f (OpenGL order +X, -X, +Y, -Y, +Z, -Z).
        [[nodiscard]] static glm::mat4 faceView(const glm::vec3& eye, int face);

    private:
        void releaseAtlas();

        unsigned int m_framebuffer {0};
        unsigned int m_color {0};
        unsigned int m_depth {0};
        int m_tilesPerRow {64};
        int m_atlasWidth {0};
        int m_atlasHeight {0};
        std::array<unsigned int, SH_TEXTURES> m_textures {};
        bool m_ready {false};
    };
}
