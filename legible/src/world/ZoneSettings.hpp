#pragma once
#include "PlazaSettings.hpp"
#include "modules/EcoCycle/EcoCycleSettings.hpp"
#include "camera/Camera.hpp"
#include <array>
#include <string_view>

namespace ecosphere {
enum class Zone { hub, ecoenergy, aquabio, ecocycle };
std::string_view zoneId(Zone zone);
std::string_view zoneTitle(Zone zone);

struct ZoneCamera {
    glm::vec3 position;
    float yaw, pitch, height = 1.7f, speed = 3;
    CameraMode mode = CameraMode::firstPerson;
    Camera make() const;
};

struct Portal {
    Zone destination;
    glm::vec3 center, halfExtent;
    ZoneCamera returnCamera;
    bool contains(glm::vec3 position) const;
};

struct ZoneSettings {
    // cámara al entrar a cada zona {posición, yaw, pitch}
    std::array<ZoneCamera, 4> entries{{
        {plaza::settings.cameraStart, plaza::settings.cameraYaw, plaza::settings.cameraPitch},   // plaza
        {{0, 1.7f, 6}, -90, -4},                                                                 // ecoenergy
        {{0, 1.7f, 13}, -90, -4},                                                                // aquabio
        {ecocycle::settings.cameraPosition, ecocycle::settings.cameraYaw,
         ecocycle::settings.cameraPitch}                                                         // ecocycle
    }};
    // distancia del centro a cada arco
    float access = plaza::settings.radius + plaza::settings.pathLength - .5f;
    // portal = {zona, centro, medio tamaño de la caja, cámara al volver a la plaza}
    std::array<Portal, 3> portals{{
        {Zone::ecoenergy, {-access, 1.7f, 0}, {1.5f, 1.4f, 2}, {{-access + 3.8f, 1.7f, 0}, 0, 0}},
        {Zone::aquabio,   {0, 1.7f, -access}, {2, 1.4f, 1.5f}, {{0, 1.7f, -access + 3.8f}, 90, 0}},
        {Zone::ecocycle,  {access, 1.7f, 0},  {1.5f, 1.4f, 2}, {{access - 3.8f, 1.7f, 0}, 180, 0}}
    }};
};
}
