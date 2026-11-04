#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
struct LightSettings {
    bool enabled = true;
    glm::vec3 vector{0, -1, 0}; // Dirección de propagación o posición mundial.
    glm::vec3 color{1};
    float intensity = 1;
};
struct EnvironmentSettings {
    bool reflectionEnabled = true, skyboxEnabled = true;
    float intensity = 0.8f;
    float fresnelPower = 5.0f; // Schlick usa 5; se expone para comparación docente.
};
struct LightingSettings {
    bool legacyUnlit = true; // API anterior y smoke RGB mantienen exactamente su salida.
    bool directEnabled = true;
    float ambient = 0.10f;
    LightSettings directional{true, {-0.4f, -1, -0.5f}, {1, 0.95f, 0.85f}, 0.8f};
    LightSettings point{true, {2, 3, 2}, {0.55f, 0.75f, 1}, 2};
    glm::vec3 attenuation{1, 0.14f, 0.07f}; // 1 / (kc + kl*d + kq*d²).
    EnvironmentSettings environment;
};
inline bool finiteVector(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline void validateLighting(const LightingSettings& s, glm::vec3 camera) {
    if (!finiteVector(camera) || !std::isfinite(s.ambient) || s.ambient < 0 || s.ambient > 1)
        throw std::invalid_argument("Invalid camera/ambient");
    for (const auto& l : {s.directional, s.point}) {
        if (!finiteVector(l.vector) || !finiteVector(l.color) || glm::any(glm::lessThan(l.color, glm::vec3(0))) ||
            glm::any(glm::greaterThan(l.color, glm::vec3(1))) || !std::isfinite(l.intensity) || l.intensity < 0 || l.intensity > 16)
            throw std::invalid_argument("Invalid light vector/color/intensity");
    }
    if (!std::isfinite(glm::length(s.directional.vector)) || glm::length(s.directional.vector) < 0.00001f)
        throw std::invalid_argument("Directional light requires nonzero direction");
    if (!finiteVector(s.attenuation) || s.attenuation.x < 0.0001f || s.attenuation.y < 0 || s.attenuation.z < 0 ||
        glm::any(glm::greaterThan(s.attenuation, glm::vec3(100))))
        throw std::invalid_argument("Invalid point attenuation");
    if (!std::isfinite(s.environment.intensity) || s.environment.intensity < 0 || s.environment.intensity > 4 ||
        !std::isfinite(s.environment.fresnelPower) || s.environment.fresnelPower < 1 || s.environment.fresnelPower > 10)
        throw std::invalid_argument("Invalid environment intensity/Fresnel power");
}
inline glm::mat3 normalMatrix(const glm::mat4& model) {
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        if (!std::isfinite(model[c][r])) throw std::invalid_argument("Non-finite model transform");
    if (model[0][3] != 0 || model[1][3] != 0 || model[2][3] != 0 || model[3][3] != 1)
        throw std::invalid_argument("Model transform must be affine");
    const glm::mat3 linear(model);
    const float determinant = glm::determinant(linear);
    if (!std::isfinite(determinant) || std::abs(determinant) < 0.00000001f)
        throw std::invalid_argument("Singular/ill-conditioned normal transform");
    // Una normal debe permanecer perpendicular a tangentes bajo escala no uniforme.
    const auto result = glm::transpose(glm::inverse(linear));
    for (int c = 0; c < 3; ++c) if (!finiteVector(result[c])) throw std::invalid_argument("Non-finite normal matrix");
    return result;
}
inline glm::vec3 fresnelSchlick(float cosine, glm::vec3 f0, float power = 5) {
    if (!std::isfinite(cosine) || !finiteVector(f0) || glm::any(glm::lessThan(f0, glm::vec3(0))) ||
        glm::any(glm::greaterThan(f0, glm::vec3(1))) || !std::isfinite(power) || power < 1 || power > 10)
        throw std::invalid_argument("Invalid Fresnel parameters");
    return f0 + (glm::vec3(1) - f0) * std::pow(1 - std::clamp(cosine, 0.0f, 1.0f), power);
}
}
