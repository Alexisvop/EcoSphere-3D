#pragma once
#include "scene/Lighting.hpp"
#include "scene/Transform.hpp"
#include "assets/Material.hpp"
#include <array>

namespace ecosphere::demo {
// ---- Luces de la demo de iluminación ----
inline const LightingSettings lighting = [] {
    LightingSettings light;
    light.legacyUnlit = false;
    light.ambient = 0.08f;
    // {encendida, dirección, color, intensidad}
    light.directional = {true, {-0.4f, -1, -0.5f}, {1, 0.92f, 0.78f}, 0.85f};
    // {encendida, posición, color, intensidad}
    light.point = {true, {2.5f, 3.5f, 2.0f}, {0.45f, 0.7f, 1}, 2.5f};
    light.attenuation = {1, 0.14f, 0.07f};
    light.environment = {true, true, 0.9f, 5.0f};
    return light;
}();

// ---- Materiales ----
inline const MaterialSettings matte{false, false, AlphaMode::opaque, 0.5f, 0, 0.95f, 0.03f, 0};
inline const MaterialSettings plastic{false, false, AlphaMode::opaque, 0.5f, 0, 0.18f, 0.8f, 0.6f};
inline const MaterialSettings metal{false, false, AlphaMode::opaque, 0.5f, 1, 0.12f, 1, 1};
inline const MaterialSettings glass{false, true, AlphaMode::blend, 0.5f, 0, 0.10f, 0.9f, 0.55f};

// ---- Colores {r, g, b, a} ----
inline const glm::vec4 matteColor{0.6f, 0.13f, 0.06f, 1};
inline const glm::vec4 plasticColor{0.04f, 0.22f, 0.62f, 1};
inline const glm::vec4 metalColor{0.83f, 0.65f, 0.32f, 1};
inline const glm::vec4 glassColor{0.28f, 0.8f, 0.65f, 0.32f};

// ---- Posiciones {posición, rotación en grados, escala} ----
inline const std::array<Transform, 3> materialPositions{{
    {{-2, 2.65f, -1.5f}, {0, 0, 0}, {0.72f, 0.72f, 0.72f}},   // esfera mate
    {{0, 2.65f, -1.5f},  {0, 0, 0}, {0.72f, 0.72f, 0.72f}},   // esfera de plástico
    {{2, 2.65f, -1.5f},  {0, 0, 0}, {0.72f, 0.72f, 0.72f}}    // esfera de metal
}};
inline const Transform glassPlacement{{2.9f, 1.35f, 0}, {0, 0, 0}, {1.7f, 2.1f, 1}};
inline const Transform glassBackdrop{{2.9f, 1.35f, -1.0f}, {0, 15, 0}, {0.8f, 1.4f, 0.5f}};
inline const Transform maskPlacement{{-3.5f, 3.25f, -2.5f}, {0, 0, 0}, {1.2f, 1.0f, 1}};
}
