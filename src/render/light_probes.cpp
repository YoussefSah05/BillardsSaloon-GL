#include "render/light_probes.h"

#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

namespace Septhuit
{
    LightProbes::~LightProbes()
    {
        releaseAtlas();
        if (m_textures[0] != 0)
        {
            glDeleteTextures(SH_TEXTURES, m_textures.data());
        }
    }

    void LightProbes::releaseAtlas()
    {
        if (m_framebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_framebuffer);
            glDeleteTextures(1, &m_color);
            glDeleteRenderbuffers(1, &m_depth);
            m_framebuffer = m_color = m_depth = 0;
        }
    }

    void LightProbes::beginCapture(int probeCount)
    {
        const int tiles = probeCount * 6;
        const int width = m_tilesPerRow * FACE_SIZE;
        const int height = ((tiles + m_tilesPerRow - 1) / m_tilesPerRow) * FACE_SIZE;
        if ((width != m_atlasWidth) || (height != m_atlasHeight))
        {
            releaseAtlas();
            m_atlasWidth = width;
            m_atlasHeight = height;

            glGenTextures(1, &m_color);
            glBindTexture(GL_TEXTURE_2D, m_color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glGenRenderbuffers(1, &m_depth);
            glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

            glGenFramebuffers(1, &m_framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                throw std::runtime_error("Light probe capture framebuffer is incomplete.");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, m_atlasWidth, m_atlasHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
    }

    void LightProbes::bindTile(int probe, int face) const
    {
        const int tile = probe * 6 + face;
        const int x = (tile % m_tilesPerRow) * FACE_SIZE;
        const int y = (tile / m_tilesPerRow) * FACE_SIZE;
        glViewport(x, y, FACE_SIZE, FACE_SIZE);
        glScissor(x, y, FACE_SIZE, FACE_SIZE);
    }

    std::vector<ShRgb9> LightProbes::finishCapture(int probeCount) const
    {
        glDisable(GL_SCISSOR_TEST);
        std::vector<glm::vec4> pixels(static_cast<std::size_t>(m_atlasWidth) * static_cast<std::size_t>(m_atlasHeight));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, m_atlasWidth, m_atlasHeight, GL_RGBA, GL_FLOAT, pixels.data());

        std::vector<ShRgb9> probes;
        probes.reserve(static_cast<std::size_t>(probeCount));
        for (int probe = 0; probe < probeCount; ++probe)
        {
            std::array<CubeFace, 6> faces;
            for (int face = 0; face < 6; ++face)
            {
                const int tile = probe * 6 + face;
                const int x0 = (tile % m_tilesPerRow) * FACE_SIZE;
                const int y0 = (tile / m_tilesPerRow) * FACE_SIZE;
                std::vector<glm::vec3>& texels = faces[static_cast<std::size_t>(face)].texels;
                texels.reserve(FACE_SIZE * FACE_SIZE);
                for (int y = 0; y < FACE_SIZE; ++y)
                {
                    for (int x = 0; x < FACE_SIZE; ++x)
                    {
                        const glm::vec4& p = pixels[static_cast<std::size_t>((y0 + y) * m_atlasWidth + x0 + x)];
                        texels.emplace_back(p);
                    }
                }
            }
            probes.push_back(projectCube(faces, FACE_SIZE));
        }
        return probes;
    }

    void LightProbes::upload(const std::vector<ShRgb9>& probes, const glm::ivec3& counts)
    {
        if (m_textures[0] == 0)
        {
            glGenTextures(SH_TEXTURES, m_textures.data());
        }
        // 27 floats per probe, packed four to a texel across the textures.
        const std::size_t count = probes.size();
        std::array<std::vector<glm::vec4>, SH_TEXTURES> packed;
        for (std::vector<glm::vec4>& texture : packed)
        {
            texture.resize(count);
        }
        for (std::size_t p = 0; p < count; ++p)
        {
            std::array<float, SH_TEXTURES * 4> flat {};
            for (std::size_t c = 0; c < 9; ++c)
            {
                flat[c * 3 + 0] = probes[p].coefficients[c].r;
                flat[c * 3 + 1] = probes[p].coefficients[c].g;
                flat[c * 3 + 2] = probes[p].coefficients[c].b;
            }
            for (std::size_t t = 0; t < SH_TEXTURES; ++t)
            {
                packed[t][p] = glm::vec4(flat[t * 4], flat[t * 4 + 1], flat[t * 4 + 2], flat[t * 4 + 3]);
            }
        }
        for (std::size_t t = 0; t < SH_TEXTURES; ++t)
        {
            glBindTexture(GL_TEXTURE_3D, m_textures[t]);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, counts.x, counts.y, counts.z, 0, GL_RGBA, GL_FLOAT, packed[t].data());
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        glBindTexture(GL_TEXTURE_3D, 0);
        m_ready = true;
    }

    void LightProbes::bindTextures(int firstUnit) const
    {
        for (int t = 0; t < SH_TEXTURES; ++t)
        {
            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + firstUnit + t));
            glBindTexture(GL_TEXTURE_3D, m_textures[static_cast<std::size_t>(t)]);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    glm::mat4 LightProbes::faceView(const glm::vec3& eye, int face)
    {
        // The standard cube map capture basis: these up vectors make the
        // rendered image match the cube map's texel layout.
        static const glm::vec3 forward[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        static const glm::vec3 up[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
        return glm::lookAt(eye, eye + forward[face], up[face]);
    }
}
