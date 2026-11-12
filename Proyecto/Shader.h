#pragma once
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>
#include "RenderState.h"

class Shader
{
public:
    GLuint Program = 0;
    Shader(const char* vertexPath, const char* fragmentPath)
    {
        const std::string vertexSource = ReadFile(vertexPath);
        const std::string fragmentSource = ReadFile(fragmentPath);
        const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource, vertexPath);
        GLuint fragment = 0;
        try { fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath); }
        catch (...) { glDeleteShader(vertex); throw; }
        Program = glCreateProgram();
        glAttachShader(Program, vertex);
        glAttachShader(Program, fragment);
        glLinkProgram(Program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint success = GL_FALSE;
        glGetProgramiv(Program, GL_LINK_STATUS, &success);
        if (!success)
        {
            GLint length = 0;
            glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> log(static_cast<size_t>(length) + 1, '\0');
            glGetProgramInfoLog(Program, static_cast<GLsizei>(log.size()), nullptr, log.data());
            glDeleteProgram(Program); Program = 0;
            throw std::runtime_error("Error al enlazar shader: " + std::string(log.data()));
        }
    }
    ~Shader() { if (Program) glDeleteProgram(Program); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    void Use() const { RenderState::UseProgram(Program); }
    GLint Uniform(const std::string& name) const
    {
        const auto it=uniforms.find(name);if(it!=uniforms.end())return it->second;
        const GLint location=glGetUniformLocation(Program,name.c_str());uniforms.emplace(name,location);return location;
    }
private:
    mutable std::unordered_map<std::string,GLint> uniforms;
    static std::string ReadFile(const char* path)
    {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("No se pudo abrir el shader: " + std::string(path));
        std::ostringstream source;
        source << file.rdbuf();
        return source.str();
    }
    static GLuint Compile(GLenum type, const std::string& source, const char* path)
    {
        const GLuint shader = glCreateShader(type);
        const char* code = source.c_str();
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> log(static_cast<size_t>(length) + 1, '\0');
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error(std::string(path) + ": " + log.data());
        }
        return shader;
    }
};
