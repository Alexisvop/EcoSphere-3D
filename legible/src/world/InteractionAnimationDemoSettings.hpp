#pragma once
#include "animation/Animation.hpp"
#include "interaction/Interaction.hpp"
#include <array>

namespace ecosphere::interactionDemo {
// ---- Animaciones (metros, segundos y grados) ----
inline constexpr double duration = 8;
inline constexpr double playbackSpeed = 1;
inline constexpr float rotationSpeed = 45;        // grados por segundo
inline constexpr float linearAmplitude = 0.5f;
inline constexpr double linearFrequency = 0.25;
inline constexpr float angularAmplitude = 25;
inline constexpr double angularFrequency = 0.5;
inline constexpr float sineAmplitude = 0.35f;
inline constexpr double sineFrequency = 0.5;
inline constexpr float orbitRadius = 0.4f;
inline constexpr double orbitFrequency = 0.25;
inline constexpr float proceduralAngle = 20;

// ---- Manipulación con teclado ----
inline constexpr float translationSpeed = 1.5f;
inline constexpr float manipulationRotationSpeed = 60;
inline constexpr float scaleSpeed = 0.7f;
inline constexpr float holdDistance = 3;
inline const Aabb selectionBounds{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

// ---- Cubos {posición, rotación (w, x, y, z), escala} ----
inline const std::array<Pose, 7> bases{{
    {{-2, 0.7f, 2},    {1, 0, 0, 0}, {0.6f, 0.6f, 0.6f}},      // rotación simple
    {{-0.8f, 0.7f, 2}, {1, 0, 0, 0}, {0.6f, 0.6f, 0.6f}},      // traslación periódica
    {{0.5f, 0.7f, 2},  {1, 0, 0, 0}, {0.6f, 0.6f, 0.6f}},      // oscilación
    {{1.8f, 0.7f, 2},  {1, 0, 0, 0}, {0.55f, 0.55f, 0.55f}},   // keyframes
    {{-1.4f, 3.8f, 0}, {1, 0, 0, 0}, {0.55f, 0.55f, 0.55f}},   // onda senoidal
    {{1.2f, 3.8f, 0},  {1, 0, 0, 0}, {0.7f, 0.7f, 0.7f}},      // padre en órbita
    {{1.1f, 0, 0},     {1, 0, 0, 0}, {0.4f, 0.4f, 0.4f}}       // hijo, relativo al padre
}};

// ---- Keyframes del cubo 4 {tiempo, {posición, rotación, escala}} ----
inline const std::vector<Keyframe> keyframes{
    {0, {{0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}},
    {duration * 0.5, {{-0.8f, 1.2f, 0},
                      glm::angleAxis(glm::radians(150.0f), glm::vec3(0, 1, 0)),
                      {1.3f, 0.8f, 1}}},
    {duration, {{0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}}}
};
}
