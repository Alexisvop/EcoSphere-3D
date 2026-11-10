#pragma once
#include "PlazaSmoke.hpp"

namespace ecosphere {
void checkEcoEnergyFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present);
}
