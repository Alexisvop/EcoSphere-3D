#include "Environment.hpp"
#include "Shader.hpp"
#include "RenderState.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ecosphere {
namespace {
constexpr int skySize = 512;

// número pseudoaleatorio entre 0 y 1 a partir de un entero
float hash1(int value) {
    unsigned h = static_cast<unsigned>(value) * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return static_cast<float>(h & 0xFFFFFFu) / 16777215.0f;
}

float smoothRange(float low, float high, float value) {
    const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

float noiseAt(int x, int y) {
    return hash1(x * 7919 + y * 104729);
}

// ruido suave para las nubes
float valueNoise(float x, float y) {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = smoothRange(0, 1, x - ix);
    const float fy = smoothRange(0, 1, y - iy);
    const float top = noiseAt(ix, iy) + (noiseAt(ix + 1, iy) - noiseAt(ix, iy)) * fx;
    const float bottom = noiseAt(ix, iy + 1) + (noiseAt(ix + 1, iy + 1) - noiseAt(ix, iy + 1)) * fx;
    return top + (bottom - top) * fy;
}

// altura de la silueta de montañas alrededor del horizonte
float ridgeHeight(float turn, int segments, int seed) {
    const float position = turn * segments;
    const int index = static_cast<int>(std::floor(position));
    const float current = hash1(seed + index % segments);
    const float next = hash1(seed + (index + 1) % segments);
    return current + (next - current) * (position - index);
}

float facetShade(float turn, int segments, int seed) {
    return hash1(seed + 500 + static_cast<int>(turn * segments) % segments);
}

// color del cielo en una dirección: degradado, sol, nubes, montañas y suelo
glm::vec3 skyColor(const glm::vec3& direction) {
    const float flat = std::max(std::hypot(direction.x, direction.z), 0.0001f);
    const float elevation = direction.y / flat;
    const float turn = std::clamp(std::atan2(direction.z, direction.x) / 6.2831853f + 0.5f, 0.0f, 0.9999f);
    const float edge = 0.004f;
    const auto ground = glm::mix(glm::vec3(0.36f, 0.50f, 0.30f), glm::vec3(0.24f, 0.36f, 0.20f),
                                 smoothRange(0, 0.6f, -direction.y));
    if (elevation <= -edge) return ground;
    auto color = glm::mix(glm::vec3(0.62f, 0.78f, 0.88f), glm::vec3(0.16f, 0.38f, 0.72f),
                          std::pow(std::max(direction.y, 0.0f), 0.6f));
    const float sun = std::max(glm::dot(direction, glm::normalize(glm::vec3(0.45f, 1, 0.3f))), 0.0f);
    color += glm::vec3(1, 0.92f, 0.75f) * (0.25f * std::pow(sun, 64.0f) + 0.8f * std::pow(sun, 900.0f));
    const float scale = 1.1f / (std::max(direction.y, 0.0f) + 0.25f);
    const float u = direction.x * scale + 13;
    const float v = direction.z * scale + 7;
    const float cloud = 0.6f * valueNoise(u, v) + 0.3f * valueNoise(2.1f * u, 2.1f * v) +
                        0.1f * valueNoise(4.3f * u, 4.3f * v);
    const auto cloudColor = glm::mix(glm::vec3(0.70f, 0.75f, 0.82f), glm::vec3(1),
                                     smoothRange(0.58f, 0.74f, cloud));
    const float cloudAmount = smoothRange(0.56f, 0.61f, cloud) * smoothRange(0.03f, 0.14f, direction.y);
    color = glm::mix(color, cloudColor, cloudAmount);
    const float farHeight = 0.035f + 0.085f * ridgeHeight(turn, 22, 11);
    const float nearHeight = 0.018f + 0.040f * ridgeHeight(turn, 34, 57);
    const auto farColor = glm::mix(glm::vec3(0.30f, 0.42f, 0.56f), glm::vec3(0.40f, 0.52f, 0.64f),
                                   facetShade(turn, 22, 11));
    const auto nearColor = glm::mix(glm::vec3(0.19f, 0.36f, 0.23f), glm::vec3(0.27f, 0.45f, 0.29f),
                                    facetShade(turn, 34, 57));
    color = glm::mix(farColor, color, smoothRange(farHeight - edge, farHeight + edge, elevation));
    color = glm::mix(nearColor, color, smoothRange(nearHeight - edge, nearHeight + edge, elevation));
    return glm::mix(ground, color, smoothRange(-edge, edge, elevation));
}

// dirección 3D de un píxel (u, v) en cada cara del cubemap (+X, -X, +Y, -Y, +Z, -Z)
glm::vec3 faceDirection(int face, float u, float v) {
    const std::array<glm::vec3, 6> directions{{
        {1, -v, -u}, {-1, -v, u}, {u, 1, v}, {u, -1, -v}, {u, -v, 1}, {-u, -v, -1}}};
    return directions[face];
}

// genera las 6 caras del cielo en RGB de 8 bits
std::array<std::vector<unsigned char>, 6> buildSkyFaces() {
    std::array<std::vector<unsigned char>, 6> faces;
    for (int face = 0; face < 6; ++face) {
        auto& pixels = faces[face];
        pixels.resize(static_cast<std::size_t>(skySize) * skySize * 3);
        for (int y = 0; y < skySize; ++y) {
            for (int x = 0; x < skySize; ++x) {
                const float u = 2.0f * (x + 0.5f) / skySize - 1;
                const float v = 2.0f * (y + 0.5f) / skySize - 1;
                const auto color = skyColor(glm::normalize(faceDirection(face, u, v)));
                const auto pixel = (static_cast<std::size_t>(y) * skySize + x) * 3;
                for (int c = 0; c < 3; ++c) {
                    const float channel = std::clamp(color[c], 0.0f, 1.0f);
                    pixels[pixel + c] = static_cast<unsigned char>(std::lround(channel * 255));
                }
            }
        }
    }
    return faces;
}
}

Environment::Environment(const std::filesystem::path& shaders) {
    try {
        const auto vertexText = readShaderFile(shaders / "skybox.vert");
        const auto fragmentText = readShaderFile(shaders / "skybox.frag");
        Shader vertex(GL_VERTEX_SHADER, vertexText.c_str());
        Shader fragment(GL_FRAGMENT_SHADER, fragmentText.c_str());
        program_ = linkProgram(vertex.handle, fragment.handle);
        inverseSky_ = glGetUniformLocation(program_, "inverseSky");
        sampler_ = glGetUniformLocation(program_, "environmentMap");
        if (inverseSky_ < 0 || sampler_ < 0) throw std::runtime_error("Skybox missing required uniforms");
        RenderState state;
        // cubemap en la unidad de textura 1
        glActiveTexture(GL_TEXTURE1);
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
        static const auto faces = buildSkyFaces();
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, skySize, skySize, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, faces[face].data());
            if (glGetError() != GL_NO_ERROR) {
                throw std::runtime_error("Cannot upload cubemap face " + std::to_string(face));
            }
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        for (auto axis : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R}) {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, axis, GL_CLAMP_TO_EDGE);
        }
        // el skybox no usa vértices, el shader los genera con gl_VertexID
        glGenVertexArrays(1, &vao_);
        if (!texture_ || !vao_ || glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("Cannot create cubemap/skybox");
        }
    } catch (...) {
        release();
        throw;
    }
}

void Environment::release() noexcept {
    glDeleteTextures(1, &texture_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
}

Environment::~Environment() { release(); }

// triángulo que cubre la pantalla, al fondo de la profundidad
void Environment::draw(const glm::mat4& view, const glm::mat4& projection) const {
    RenderState state;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    // mat3(view) quita la traslación: el cielo no se mueve con la cámara
    const auto inverse = glm::inverse(projection * glm::mat4(glm::mat3(view)));
    glUniformMatrix4fv(inverseSky_, 1, GL_FALSE, glm::value_ptr(inverse));
    glUniform1i(sampler_, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
}
