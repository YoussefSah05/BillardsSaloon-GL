#pragma once

#include <vector>

namespace BilliardsSaloon
{
    // The HDR scene buffer: drawn multisampled, resolved to a float texture
    // that the post pass reads.
    class SceneTarget
    {
    public:
        SceneTarget() = default;
        ~SceneTarget();
        SceneTarget(const SceneTarget&) = delete;
        SceneTarget& operator=(const SceneTarget&) = delete;

        // Recreates the buffers when the size or sample count changes.
        void ensure(int width, int height, int samples);
        void bindForDrawing() const;
        void resolve() const;

        [[nodiscard]] unsigned int colorTexture() const { return m_resolveColor; }
        [[nodiscard]] int width() const { return m_width; }
        [[nodiscard]] int height() const { return m_height; }

    private:
        void release();

        int m_width {0};
        int m_height {0};
        int m_samples {0};
        unsigned int m_drawFramebuffer {0};
        unsigned int m_drawColor {0};
        unsigned int m_drawDepth {0};
        unsigned int m_resolveFramebuffer {0};
        unsigned int m_resolveColor {0};
    };

    // Half, quarter, ... resolution float textures for bloom.
    class BloomChain
    {
    public:
        struct Level
        {
            unsigned int framebuffer {0};
            unsigned int texture {0};
            int width {0};
            int height {0};
        };

        BloomChain() = default;
        ~BloomChain();
        BloomChain(const BloomChain&) = delete;
        BloomChain& operator=(const BloomChain&) = delete;

        void ensure(int width, int height, int levels);
        [[nodiscard]] const std::vector<Level>& levels() const { return m_levels; }

    private:
        void release();

        int m_width {0};
        int m_height {0};
        std::vector<Level> m_levels;
    };

    // One depth layer per lamp, in a 2D texture array, compared in the shader.
    class ShadowMaps
    {
    public:
        ShadowMaps() = default;
        ~ShadowMaps();
        ShadowMaps(const ShadowMaps&) = delete;
        ShadowMaps& operator=(const ShadowMaps&) = delete;

        void ensure(int size, int layers);
        void bindLayerForDrawing(int layer) const;

        [[nodiscard]] unsigned int texture() const { return m_texture; }
        [[nodiscard]] int size() const { return m_size; }

    private:
        void release();

        int m_size {0};
        int m_layers {0};
        unsigned int m_framebuffer {0};
        unsigned int m_texture {0};
    };
}
