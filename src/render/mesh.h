#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace BilliardsSaloon
{
    struct Vertex
    {
        float position[3];
        float normal[3];
        float texCoord[2];
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
        static std::unique_ptr<Mesh> createPlane(float width, float depth);
        static std::unique_ptr<Mesh> createUVSphere(float radius, std::uint32_t slices, std::uint32_t stacks);

    private:
        unsigned int m_vao {0};
        unsigned int m_vbo {0};
        unsigned int m_ebo {0};
        std::int32_t m_indexCount {0};
    };
}
