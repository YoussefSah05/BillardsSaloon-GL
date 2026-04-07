#include "render/mesh.h"

#include <glad/gl.h>

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
            static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), // How much data we are sending to the GPU
            vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
            indices.data(), // indices is the pointer to the data we want to send to the GPU
            GL_STATIC_DRAW
        );

        constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex)); // stride is the distance between consecutive vertex attributes in the buffer

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, normal)));

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
        const std::vector<Vertex> vertices = { // 6 Faces * 4 Vertices per face = 24 Vertices
            {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}}, // normal is the same for all vertices of a face.
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

        const std::vector<std::uint32_t> indices = { // 3 indices per triangle * 2 triangles per face * 6 faces = 36 indices
             0,  1,  2,   2,  3,  0,
             4,  5,  6,   6,  7,  4,
             8,  9, 10,  10, 11,  8,
            12, 13, 14,  14, 15, 12,
            16, 17, 18,  18, 19, 16,
            20, 21, 22,  22, 23, 20
        };

        return std::make_unique<Mesh>(vertices, indices);
    }
}