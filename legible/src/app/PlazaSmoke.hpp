#pragma once
#include "content/ContentRegistry.hpp"
#include "camera/Camera.hpp"
#include <functional>

namespace ecosphere {
// dibuja la escena: luces, si incluye contenido y qué instancia aislar
using PlazaRender = std::function<void(const LightingSettings&, bool, const std::optional<std::string>&)>;

void checkPlazaFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present);
}
