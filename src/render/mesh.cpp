#include "render/mesh.h"

#include <glad/gl.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>

namespace BilliardsSaloon
{
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices)
        : m_indexCount(static_cast<std::int32_t>(indices.size()))
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        if ((m_vao == 0) || (m_vbo == 0) || (m_ebo == 0))
        {
            throw std::runtime_error("Failed to allocate OpenGL mesh buffers.");
        }

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
            vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
            indices.data(),
            GL_STATIC_DRAW
        );

        constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(offsetof(Vertex, position))
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(offsetof(Vertex, normal))
        );

        glBindVertexArray(0);
    }

    Mesh::~Mesh()
    {
        if (m_ebo != 0)
        {
            glDeleteBuffers(1, &m_ebo);
        }

        if (m_vbo != 0)
        {
            glDeleteBuffers(1, &m_vbo);
        }

        if (m_vao != 0)
        {
            glDeleteVertexArrays(1, &m_vao);
        }
    }

    void Mesh::draw() const
    {
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    std::unique_ptr<Mesh> Mesh::createCube()
    {
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
            {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},

            {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
            {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},

            {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},

            {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
            {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},

            {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
            {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
        };

        const std::vector<std::uint32_t> indices = {
             0,  1,  2,   2,  3,  0,
             4,  5,  6,   6,  7,  4,
             8,  9, 10,  10, 11,  8,
            12, 13, 14,  14, 15, 12,
            16, 17, 18,  18, 19, 16,
            20, 21, 22,  22, 23, 20
        };

        return std::make_unique<Mesh>(vertices, indices);
    }

    std::unique_ptr<Mesh> Mesh::createPlane(float width, float depth)
    {
        const float halfWidth = 0.5f * width;
        const float halfDepth = 0.5f * depth;

        const std::vector<Vertex> vertices = {
            {{-halfWidth, 0.0f, -halfDepth}, {0.0f, 1.0f, 0.0f}},
            {{ halfWidth, 0.0f, -halfDepth}, {0.0f, 1.0f, 0.0f}},
            {{ halfWidth, 0.0f,  halfDepth}, {0.0f, 1.0f, 0.0f}},
            {{-halfWidth, 0.0f,  halfDepth}, {0.0f, 1.0f, 0.0f}},
        };

        const std::vector<std::uint32_t> indices = {
            0, 2, 1,
            2, 0, 3
        };

        return std::make_unique<Mesh>(vertices, indices);
    }

    std::unique_ptr<Mesh> Mesh::createUVSphere(float radius, std::uint32_t slices, std::uint32_t stacks)
    {
        slices = (slices < 3U) ? 3U : slices;
        stacks = (stacks < 2U) ? 2U : stacks;

        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;

        vertices.reserve(static_cast<std::size_t>((slices + 1U) * (stacks + 1U)));
        indices.reserve(static_cast<std::size_t>(slices * stacks * 6U));

        constexpr float PI = 3.14159265358979323846f; // good precision for single-precision floating-point
        constexpr float TWO_PI = 6.28318530717958647692f;

        for (std::uint32_t stack = 0; stack <= stacks; ++stack)
        {
            const float v = static_cast<float>(stack) / static_cast<float>(stacks);
            const float phi = v * PI;

            const float y = std::cos(phi);
            const float ringRadius = std::sin(phi);

            for (std::uint32_t slice = 0; slice <= slices; ++slice)
            {
                const float u = static_cast<float>(slice) / static_cast<float>(slices);
                const float theta = u * TWO_PI;

                const float x = ringRadius * std::cos(theta);
                const float z = ringRadius * std::sin(theta);

                const float nx = x;
                const float ny = y;
                const float nz = z;

                vertices.push_back(Vertex{
                    {radius * x, radius * y, radius * z},
                    {nx, ny, nz}
                });
            }
        }

        const std::uint32_t stride = slices + 1U;

        for (std::uint32_t stack = 0; stack < stacks; ++stack)
        {
            for (std::uint32_t slice = 0; slice < slices; ++slice)
            {
                const std::uint32_t a = stack * stride + slice;
                const std::uint32_t b = (stack + 1U) * stride + slice;
                const std::uint32_t c = b + 1U;
                const std::uint32_t d = a + 1U;

                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(d);

                indices.push_back(d);
                indices.push_back(b);
                indices.push_back(c);
            }
        }

        return std::make_unique<Mesh>(vertices, indices);
    }
}