#include "Environment.hpp"
#include "Shader.hpp"
#include "RenderState.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <algorithm>
#include <cmath>

namespace ecosphere {
Environment::Environment(const std::filesystem::path& shaders) {
    try {
        const auto v = readShaderFile(shaders / "skybox.vert"), f = readShaderFile(shaders / "skybox.frag");
        Shader vertex(GL_VERTEX_SHADER, v.c_str()), fragment(GL_FRAGMENT_SHADER, f.c_str());
        program_ = linkProgram(vertex.handle, fragment.handle);
        inverseSky_ = glGetUniformLocation(program_, "inverseSky");
        sampler_ = glGetUniformLocation(program_, "environmentMap");
        if (inverseSky_ < 0 || sampler_ < 0) throw std::runtime_error("Skybox missing required uniforms");
        RenderState state;
        glActiveTexture(GL_TEXTURE1);
        glGenTextures(1, &texture_); glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
        constexpr int size = 32;
        std::array<unsigned char, size * size * 3> pixels{};
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                const float u = 2.0f * (x + 0.5f) / size - 1, vCoord = 2.0f * (y + 0.5f) / size - 1;
                // Orden OpenGL +X,-X,+Y,-Y,+Z,-Z; v crece desde la fila de memoria 0.
                const std::array<glm::vec3, 6> directions{{{1,-vCoord,-u}, {-1,-vCoord,u},
                    {u,1,vCoord}, {u,-1,-vCoord}, {u,-vCoord,1}, {-u,-vCoord,-1}}};
                const auto d = glm::normalize(directions[face]);
                const float horizon = std::clamp(d.y * 0.5f + 0.5f, 0.0f, 1.0f);
                auto color = glm::mix(glm::vec3(0.09f,0.065f,0.035f), glm::vec3(0.18f,0.46f,0.8f), horizon);
                const float sun = std::pow(std::max(glm::dot(d, glm::normalize(glm::vec3(-1,1,1))), 0.0f), 48.0f);
                color += glm::vec3(0.8f,0.65f,0.4f) * sun;
                // Franja angular clara: hace legible la reflexión sobre superficies curvas.
                color += glm::vec3(0.22f) * std::pow(std::max(1 - std::abs(d.y - 0.12f) * 18, 0.0f), 2.0f);
                for (int c = 0; c < 3; ++c)
                    pixels[(y*size+x)*3+c] = static_cast<unsigned char>(std::lround(std::clamp(color[c], 0.0f, 1.0f) * 255));
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot upload cubemap face " + std::to_string(face));
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        for (auto axis : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
            glTexParameteri(GL_TEXTURE_CUBE_MAP, axis, GL_CLAMP_TO_EDGE);
        glGenVertexArrays(1, &vao_);
        if (!texture_ || !vao_ || glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot create cubemap/skybox");
    } catch (...) { release(); throw; }
}
void Environment::release() noexcept {
    glDeleteTextures(1, &texture_); glDeleteVertexArrays(1, &vao_); glDeleteProgram(program_);
}
Environment::~Environment() { release(); }
void Environment::draw(const glm::mat4& view, const glm::mat4& projection) const {
    RenderState state;
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
    glUseProgram(program_); glBindVertexArray(vao_);
    const auto inverse = glm::inverse(projection * glm::mat4(glm::mat3(view))); // Elimina traslación de cámara.
    glUniformMatrix4fv(inverseSky_, 1, GL_FALSE, glm::value_ptr(inverse));
    glUniform1i(sampler_, 1); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
}
