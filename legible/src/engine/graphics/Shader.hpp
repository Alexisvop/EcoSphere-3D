#pragma once
#include <glad/gl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ecosphere {
inline std::string readShaderFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open shader: " + path.string());
    std::ostringstream text;
    text << file.rdbuf();
    if (file.bad()) throw std::runtime_error("Cannot read shader: " + path.string());
    return text.str();
}

// compila un shader, si falla lanza el log de errores
struct Shader {
    GLuint handle;

    Shader(GLenum type, const char* source) : handle(glCreateShader(type)) {
        try {
            glShaderSource(handle, 1, &source, nullptr);
            glCompileShader(handle);
            GLint compileSucceeded = 0;
            glGetShaderiv(handle, GL_COMPILE_STATUS, &compileSucceeded);
            if (!compileSucceeded) {
                GLint logLength = 0;
                glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
                std::vector<char> log(static_cast<std::size_t>(logLength) + 1, '\0');
                glGetShaderInfoLog(handle, logLength, nullptr, log.data());
                throw std::runtime_error(std::string("GLSL compile failed: ") + log.data());
            }
        } catch (...) {
            glDeleteShader(handle);
            throw;
        }
    }

    ~Shader() { glDeleteShader(handle); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
};

// une vertex y fragment shader en un programa
inline GLuint linkProgram(GLuint vertex, GLuint fragment) {
    const GLuint program = glCreateProgram();
    try {
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        GLint succeeded = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &succeeded);
        if (!succeeded) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> log(static_cast<std::size_t>(length) + 1, '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            throw std::runtime_error(std::string("GLSL link failed: ") + log.data());
        }
        return program;
    } catch (...) {
        glDeleteProgram(program);
        throw;
    }
}
}
