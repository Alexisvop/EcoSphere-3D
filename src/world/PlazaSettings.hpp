#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>

namespace ecosphere::plaza {
enum class Model { pavement=1, perimeter, path, pedestal, globe, basin, jets, arch,
    platform, pergola, solarCanopy, signpost, kiosk, panel, bench, lamp, bicycleRack,
    wasteStation, drinkingFountain, planter, broadTree, smallTree, flowerBush, grasses, rocks };

struct Settings {
    float radius = 10.5f;
    float pathWidth = 3.6f, pathLength = 8;
    glm::vec3 sculpturePosition{0,.93f,0};
    // Índice = número PLAZA menos uno; ajustes para todas las instancias del modelo.
    std::array<glm::vec3,25> modelScale = [] { std::array<glm::vec3,25> v{}; v.fill(glm::vec3(1)); return v; }();
    std::array<float,25> modelYawDegrees{};
    glm::vec3 cameraStart{0,1.7f,19};
    float cameraYaw = -90, cameraPitch = 1;
    int windowWidth = 1280, windowHeight = 720;
    unsigned benchCount = 6, broadTreeCount = 8, smallTreeCount = 4;
    unsigned lampCount = 8, rockCount = 8, grassCount = 16;
    float treeRingOffset = 7;
    std::array<glm::vec3,6> benches{{{-6,0,5.4f},{6,0,5.4f},{-6,0,-5.4f},{6,0,-5.4f},{-8,0,0},{8,0,0}}};
    std::array<glm::vec3,4> planters{{{-8.2f,0,7.8f},{8.2f,0,7.8f},{-8.2f,0,-7.8f},{8.2f,0,-7.8f}}};
    std::array<glm::vec3,4> smallTrees{{{-10,0,-13},{10,0,-13},{-18,0,-10},{18,0,-10}}};
    glm::vec3 pergolaPosition{12,0,9.5f}, solarPosition{-12,0,9.5f};
    glm::vec3 signPosition{-2.8f,0,11.2f}, kioskPosition{3.8f,0,9.5f};
    glm::vec3 bicyclePosition{-5,0,10.5f};
    std::array<glm::vec3,2> panels{{{-5,0,6.5f},{6.3f,0,-6.3f}}};
    std::array<glm::vec3,3> wasteStations{{{-4.5f,0,10},{4.7f,0,10},{5,0,-10}}};
    std::array<glm::vec3,2> drinkingFountains{{{-5.7f,0,6.7f},{5.7f,0,6.7f}}};
    std::array<bool,25> visible = [] { std::array<bool,25> v{}; v.fill(true); return v; }();
    LightingSettings lighting = [] {
        LightingSettings s; s.legacyUnlit=false; s.ambient=.20f;
        s.directional={true,{-.4f,-1,-.3f},{1,.95f,.82f},.85f};
        s.point={true,{0,4.8f,3},{1,.82f,.55f},1.1f};
        s.environment.intensity=.28f;
        return s;
    }();
};
inline const Settings settings;
}
