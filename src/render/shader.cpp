#include "render/shader.h"

#include <glad/gl.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace BilliardsSaloon
{
    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
    {
        const std::string vertexSource = readTextFile(vertexPath);
        const std::string fragmentSource = readTextFile(fragmentPath);

        const unsigned int vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource);
        const unsigned int fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource);

        try
        {
            m_program = linkProgram(vertexShader, fragmentShader);
        }
        catch (...)
        {
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            throw;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    Shader::~Shader()
    {
        if (m_program != 0)
        {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

    void Shader::bind() const
    {
        glUseProgram(m_program);
    }

    void Shader::setMat4(const std::string& name, const glm::mat4& value) const
    {
        glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, &value[0][0]);
    }

    void Shader::setVec3(const std::string& name, const glm::vec3& value) const
    {
        glUniform3fv(uniformLocation(name), 1, &value[0]);
    }
    
    void Shader::setFloat(const std::string& name, float value) const
    {
        glUniform1f(uniformLocation(name), value);
    }

    void Shader::setInt(const std::string& name, int value) const
    {
        glUniform1i(uniformLocation(name), value);
    }
    
    std::string Shader::readTextFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::in);
        if (!file)
        {
            throw std::runtime_error("Failed to open shader file: " + path);
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    unsigned int Shader::compileStage(unsigned int stageType, const std::string& source)
    {
        const unsigned int shader = glCreateShader(stageType);

        const char* sourcePtr = source.c_str();
        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (success == GL_FALSE)
        {
            int logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

            std::vector<char> log(static_cast<std::size_t>(logLength));
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());

            std::string message(log.begin(), log.end());
            glDeleteShader(shader);
            throw std::runtime_error("Shader compilation failed: " + message);
        }

        return shader;
    }

    unsigned int Shader::linkProgram(unsigned int vertexShader, unsigned int fragmentShader)
    {
        const unsigned int program = glCreateProgram();

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        int success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (success == GL_FALSE)
        {
            int logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

            std::vector<char> log(static_cast<std::size_t>(logLength));
            glGetProgramInfoLog(program, logLength, nullptr, log.data());

            std::string message(log.begin(), log.end());
            glDeleteProgram(program);
            throw std::runtime_error("Program link failed: " + message);
        }

        return program;
    }

    void Shader::setVec2(const std::string& name, const glm::vec2& value) const
    {
        glUniform2f(uniformLocation(name), value.x, value.y);
    }

    int Shader::uniformLocation(const std::string& name) const
    {
        const auto cached = m_uniformLocations.find(name);
        if (cached != m_uniformLocations.end())
        {
            return cached->second;
        }

        // A uniform the compiler optimised away has no location; setting it
        // is then a no-op, as in OpenGL itself (location -1).
        const int location = glGetUniformLocation(m_program, name.c_str());
        m_uniformLocations.emplace(name, location);
        return location;
    }
}
