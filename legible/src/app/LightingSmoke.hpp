#pragma once
#include "scene/Lighting.hpp"
#include "assets/Model.hpp"
#include <filesystem>
#include <functional>

namespace ecosphere {
// captura la escena de materiales con varias luces y compara regiones de pantalla
void checkLightingFramebuffer(const std::filesystem::path& directory, int width, int height,
    const glm::mat4& viewProjection, const LightingSettings& settings, Model& showcase,
    const std::function<void(const LightingSettings&)>& render);
}
