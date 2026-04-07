#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BilliardsSaloon
{
    struct Vertex
    {
        float position[3];
        float normal[3];
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) = delete;
        Mesh& operator=(Mesh&&) = delete;

        void draw() const;

        static std::unique_ptr<Mesh> createCube();

    private:
        unsigned int m_vao {0};
        unsigned int m_vbo {0};
        unsigned int m_ebo {0};
        std::int32_t m_indexCount {0};
    };
}