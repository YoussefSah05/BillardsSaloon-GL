#include "render/render_targets.h"

#include <glad/gl.h>

#include <algorithm>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        void checkComplete(const char* what)
        {
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw std::runtime_error(std::string("Incomplete framebuffer: ") + what);
            }
        }

        unsigned int createFloatTexture(int width, int height)
        {
            unsigned int texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            return texture;
        }
    }

    // ---- SceneTarget ----------------------------------------------------------

    SceneTarget::~SceneTarget()
    {
        release();
    }

    void SceneTarget::release()
    {
        glDeleteFramebuffers(1, &m_drawFramebuffer);
        glDeleteRenderbuffers(1, &m_drawColor);
        glDeleteRenderbuffers(1, &m_drawDepth);
        glDeleteFramebuffers(1, &m_resolveFramebuffer);
        glDeleteTextures(1, &m_resolveColor);
        m_drawFramebuffer = m_drawColor = m_drawDepth = m_resolveFramebuffer = m_resolveColor = 0;
    }

    void SceneTarget::ensure(int width, int height, int samples)
    {
        width = std::max(width, 1);
        height = std::max(height, 1);
        int maxSamples = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        samples = std::clamp(samples, 1, maxSamples);
        if ((width == m_width) && (height == m_height) && (samples == m_samples) && (m_drawFramebuffer != 0))
        {
            return;
        }

        release();
        m_width = width;
        m_height = height;
        m_samples = samples;

        glGenRenderbuffers(1, &m_drawColor);
        glBindRenderbuffer(GL_RENDERBUFFER, m_drawColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
        glGenRenderbuffers(1, &m_drawDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_drawDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);

        glGenFramebuffers(1, &m_drawFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_drawFramebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_drawColor);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_drawDepth);
        checkComplete("scene");

        m_resolveColor = createFloatTexture(width, height);
        glGenFramebuffers(1, &m_resolveFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_resolveFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_resolveColor, 0);
        checkComplete("scene resolve");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void SceneTarget::bindForDrawing() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_drawFramebuffer);
        glViewport(0, 0, m_width, m_height);
    }

    void SceneTarget::resolve() const
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_drawFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolveFramebuffer);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // ---- BloomChain -----------------------------------------------------------

    BloomChain::~BloomChain()
    {
        release();
    }

    void BloomChain::release()
    {
        for (Level& level : m_levels)
        {
            glDeleteFramebuffers(1, &level.framebuffer);
            glDeleteTextures(1, &level.texture);
        }
        m_levels.clear();
    }

    void BloomChain::ensure(int width, int height, int levels)
    {
        if ((width == m_width) && (height == m_height) && (static_cast<int>(m_levels.size()) == levels))
        {
            return;
        }
        release();
        m_width = width;
        m_height = height;

        int w = width;
        int h = height;
        for (int i = 0; i < levels; ++i)
        {
            w = std::max(w / 2, 1);
            h = std::max(h / 2, 1);
            Level level;
            level.width = w;
            level.height = h;
            level.texture = createFloatTexture(w, h);
            glGenFramebuffers(1, &level.framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, level.framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, level.texture, 0);
            checkComplete("bloom");
            m_levels.push_back(level);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // ---- ShadowMaps -----------------------------------------------------------

    ShadowMaps::~ShadowMaps()
    {
        release();
    }

    void ShadowMaps::release()
    {
        glDeleteFramebuffers(1, &m_framebuffer);
        glDeleteTextures(1, &m_texture);
        m_framebuffer = m_texture = 0;
    }

    void ShadowMaps::ensure(int size, int layers)
    {
        if ((size == m_size) && (layers == m_layers) && (m_texture != 0))
        {
            return;
        }
        release();
        m_size = size;
        m_layers = layers;

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, size, size, layers, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // outside the map: lit
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, white);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texture, 0, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        checkComplete("shadow");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ShadowMaps::bindLayerForDrawing(int layer) const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texture, 0, layer);
        glViewport(0, 0, m_size, m_size);
    }
}
