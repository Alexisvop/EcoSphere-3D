#pragma once
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace ecosphere {
enum class AlphaMode { opaque, mask, blend };
struct MaterialSettings {
    bool unlit = true;
    bool doubleSided = false;
    AlphaMode alphaMode = AlphaMode::opaque;
    float alphaCutoff = 0.5f;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float specular = 0.5f;
    float reflection = 1.0f;
};
inline void validateMaterial(const MaterialSettings& m) {
    for (float v : {m.alphaCutoff, m.metallic, m.roughness, m.specular, m.reflection})
        if (!std::isfinite(v) || v < 0 || v > 1) throw std::invalid_argument("Invalid material factor: require finite [0,1]");
    if (m.alphaMode != AlphaMode::opaque && m.alphaMode != AlphaMode::mask && m.alphaMode != AlphaMode::blend)
        throw std::invalid_argument("Invalid alphaMode");
}
// Aproximación didáctica, no PBR: rugosidad alta produce un lóbulo ancho.
inline float blinnExponent(float roughness) { return 2.0f + 254.0f * std::pow(1.0f - roughness, 4.0f); }
}
