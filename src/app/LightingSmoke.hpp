#pragma once
#include "scene/Lighting.hpp"
#include "assets/Model.hpp"
#include <filesystem>
#include <functional>

namespace ecosphere {
// Capturas del framebuffer real y comparaciones por regiones proyectadas, sin RGB fijo.
void checkLightingFramebuffer(const std::filesystem::path& directory, int width, int height,
    const glm::mat4& viewProjection, const LightingSettings& settings, Model& showcase,
    const std::function<void(const LightingSettings&)>& render);
}
