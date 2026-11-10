#pragma once
#include "scene/Transform.hpp"

namespace ecosphere::demo {
// +Y arriba, +X derecha, la cámara mira hacia -Z
// Transform = {posición en metros, rotación en grados (x, y, z), escala}, colores RGB de 0 a 1

// ---- Suelo ----
inline const Transform ground{{0, -0.15f, -2}, {0, 0, 0}, {14, 0.3f, 14}};

// brillo de cada par de caras del cubo
inline constexpr float frontBackTint = 1.0f;
inline constexpr float leftRightTint = 0.65f;
inline constexpr float topBottomTint = 0.85f;
inline const glm::vec3 groundColor{0.24f, 0.30f, 0.32f};

// ---- Cubos ----
inline const Transform redCube{{-2, 0.6f, 0}, {0, 20, 0}, {1.2f, 1.2f, 1.2f}};
inline const glm::vec3 redColor{0.92f, 0.22f, 0.16f};
inline const Transform blueCube{{2.2f, 0.75f, -3.5f}, {15, -25, 0}, {1.5f, 1.5f, 1.5f}};
inline const glm::vec3 blueColor{0.18f, 0.48f, 0.95f};

// el hijo se mueve con el padre, su posición es relativa a él
inline const Transform parent{{0, 0.65f, -2}, {0, 30, 0}, {1.3f, 1.3f, 1.3f}};
inline const glm::vec3 parentColor{0.96f, 0.67f, 0.12f};
inline const Transform child{{1.25f, 1.0f, 0}, {0, 0, 25}, {0.45f, 0.45f, 0.45f}};
inline const glm::vec3 childColor{0.25f, 0.90f, 0.48f};

// ---- Modelo importado ----
inline constexpr const char* importedModel = "models/blender_probe_unlit.glb";
inline const Transform importedPlacement{{-3.3f, 1.8f, -4}, {0, 15, 0}, {0.85f, 0.85f, 0.85f}};
inline constexpr bool importedVisible = true;

// ---- Cámara y ventana ----
inline const glm::vec3 cameraStart{0, 1.7f, 7};
inline constexpr float observerHeight = 1.7f;       // metros sobre el suelo
inline constexpr float cameraYawDegrees = -90.0f;   // -90 mira hacia -Z
inline constexpr float cameraPitchDegrees = -8.0f;
inline constexpr float cameraSpeed = 3.0f;          // metros por segundo
inline constexpr float lookSpeed = 70.0f;           // grados por segundo con flechas
inline constexpr float pitchLimitDegrees = 89.0f;
inline constexpr float fovDegrees = 60.0f;          // campo de visión vertical
inline constexpr float nearPlane = 0.1f;
inline constexpr float farPlane = 100.0f;
inline constexpr double maxDeltaSeconds = 0.1;
inline constexpr int windowWidth = 800;
inline constexpr int windowHeight = 600;
}
