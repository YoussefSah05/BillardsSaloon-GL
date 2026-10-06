#include "render/reflection_probe.h"

#include "render/shader.h"

#include <glad/gl.h>

#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        unsigned int makeCube(bool mipmapped)
        {
            unsigned int texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
            for (int level = 0; level < ReflectionProbe::LEVELS; ++level)
            {
                const int size = ReflectionProbe::SIZE >> level;
                for (int face = 0; face < 6; ++face)
                {
                    glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), level, GL_RGBA16F,
                                 size, size, 0, GL_RGBA, GL_FLOAT, nullptr);
                }
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, ReflectionProbe::LEVELS - 1);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            return texture;
        }
    }

    ReflectionProbe::~ReflectionProbe()
    {
        if (m_framebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_framebuffer);
            glDeleteRenderbuffers(1, &m_depth);
            glDeleteTextures(1, &m_capture);
            glDeleteTextures(1, &m_filtered);
        }
    }

    void ReflectionProbe::create()
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);   // filter across face edges
        m_capture = makeCube(true);
        m_filtered = makeCube(true);

        glGenRenderbuffers(1, &m_depth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SIZE, SIZE);
        glGenFramebuffers(1, &m_framebuffer);
    }

    void ReflectionProbe::bindCaptureFace(int face)
    {
        if (m_framebuffer == 0)
        {
            create();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), m_capture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            throw std::runtime_error("Reflection capture framebuffer is incomplete.");
        }
        glViewport(0, 0, SIZE, SIZE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void ReflectionProbe::prefilter(Shader& prefilter)
    {
        // Mipmaps of the raw capture feed the filtered importance sampling.
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_capture);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        prefilter.bind();
        prefilter.setInt("uSource", 0);
        prefilter.setFloat("uSourceSize", static_cast<float>(SIZE));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_capture);
        for (int level = 0; level < LEVELS; ++level)
        {
            const int size = SIZE >> level;
            glViewport(0, 0, size, size);
            prefilter.setFloat("uRoughness", static_cast<float>(level) / static_cast<float>(LEVELS - 1));
            for (int face = 0; face < 6; ++face)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face), m_filtered, level);
                prefilter.setInt("uFace", face);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        m_ready = true;
    }

    void ReflectionProbe::bind(int unit) const
    {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_filtered);
        glActiveTexture(GL_TEXTURE0);
    }
}
