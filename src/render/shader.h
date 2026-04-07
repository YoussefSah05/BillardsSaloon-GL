#pragma once

#include <glm/glm.hpp>

#include <string>

namespace BilliardsSaloon
{
    class Shader
    {
    public:
        Shader(const std::string& vertexPath, const std::string& fragmentPath);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&) = delete;
        Shader& operator=(Shader&&) = delete;

        void bind() const;

        void setMat4(const std::string& name, const glm::mat4& value) const;
        void setVec3(const std::string& name, const glm::vec3& value) const;
        void setFloat(const std::string& name, float value) const;
        void setInt(const std::string& name, int value) const;

    private:
        static std::string readTextFile(const std::string& path);
        static unsigned int compileStage(unsigned int stageType, const std::string& source);
        static unsigned int linkProgram(unsigned int vertexShader, unsigned int fragmentShader);

        int uniformLocation(const std::string& name) const;

        unsigned int m_program {0};
    };
}