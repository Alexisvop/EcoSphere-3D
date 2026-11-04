#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>

namespace ecosphere::aquabio {
enum class Model {
    platform = 1, gateway, walkway, rainRoof, gutter, cistern, grate, pump, impeller,
    sedimentation, scraper, sandFilter, carbonFilter, cartridge, uvChamber, treatedTank,
    pipes, valve, flowIndicator, sampling, waterCycle, cloud, wetland, floatingPlant,
    reeds, riparianTree, pollinatorShelter, fish
};
struct Settings {
    glm::vec3 clearColor{.02f, .10f, .18f};
    // Índice = número AQUA menos uno; metros, +Y arriba. La cámara usa ZoneSettings.hpp.
    std::array<glm::vec3, 28> positions{{
        {0,0,-1}, {0,.01f,17}, {0,.06f,11}, {-17,.01f,7}, {-17,3.0f,4.98f},
        {-17,.01f,-.5f}, {-16.9f,.04f,4.98f}, {-16,.01f,-7}, {-16,.91f,-6.84f},
        {-17,.01f,-16}, {-17,1.1f,-16}, {-10,.01f,-22}, {-6,.01f,-15},
        {-10,.01f,-18}, {5,.01f,-13}, {15,.01f,-21}, {-17,.01f,-3.5f},
        {-14.5f,.01f,-4.4f}, {-17,1.10f,-3.35f}, {13,.01f,-7}, {-1,1.81f,-22},
        {5,.01f,-22}, {15,.01f,4}, {13.3f,.36f,2.2f}, {11.9f,.30f,1.7f},
        {21,.01f,-3}, {18.8f,.01f,14.5f}, {11.5f,-.05f,14.5f}
    }};
    std::array<glm::vec3, 28> scale = [] {
        std::array<glm::vec3,28> v; v.fill(glm::vec3(1)); return v;
    }();
    std::array<float,28> yawDegrees{};
    std::array<bool,28> visible = [] {
        std::array<bool,28> v; v.fill(true); return v;
    }();
    // Eje central: 12 + 3x10 m (3.8 m útiles); seis ramales de 2.8 m.
    glm::vec3 mainWalkwayScale{3.8f/2.8f,1,1.5f};
    std::array<glm::vec3,9> extraWalkways{{
        {0,.06f,1}, {0,.06f,-9}, {0,.06f,-19}, {-9.8f,.06f,2.7f},
        {-11.5f,.06f,-3}, {-9.8f,.06f,-10}, {5.8f,.06f,4}, {8.3f,.06f,8}, {12.3f,.06f,11}
    }};
    std::array<float,9> extraWalkwayYaws{{0,0,0,90,0,90,90,0,90}};
    std::array<glm::vec3,9> extraWalkwayScales = [] {
        std::array<glm::vec3,9> v; v.fill(glm::vec3(1));
        for(std::size_t i=0;i<3;++i)v[i]={3.8f/2.8f,1,1.25f};
        for(std::size_t i=3;i<6;++i)v[i].z=2;
        v[4].z=1.75f; // Tramo occidental de 14 m: deja libre la bajante.
        return v;
    }();
    std::array<glm::vec3,5> extraFloatingPlants{{
        {16.5f,.36f,2}, {14.7f,.36f,3.6f}, {17.2f,.36f,4.4f}, {12.8f,.36f,5.1f}, {15.5f,.36f,6.1f}
    }};
    std::array<glm::vec3,5> extraReeds{{
        {17.7f,.3f,1.7f}, {18,.3f,3.4f}, {17.6f,.3f,6.3f}, {13,.3f,6.5f}, {11.9f,.3f,4.3f}
    }};
    std::array<glm::vec3,2> extraTrees{{{21.2f,.01f,9.3f}, {15.5f,.01f,15}}};
    glm::vec3 impellerDemoPosition{-3,.44f,8};
    glm::vec3 cartridgeDemoPosition{-6,.01f,-11.7f};
    glm::vec3 extraPipesPosition{9,.01f,-17};

    // Cinemática ilustrativa: no hay caudal, potabilidad ni fluidodinámica simulados.
    float pumpSpeedDeg = 110, scraperSpeedDeg = 12, flowSpeedDeg = 55, cycleSpeedDeg = 16;
    double gutterDurationSeconds = 8, cartridgeDurationSeconds = 10;
    float gutterSwingDegrees = 6, cartridgeLiftMeters = .85f, cartridgeTravelMeters = .60f;
    float floatingAmplitudeMeters = .055f, reedAmplitudeDegrees = 5;
    double floatingFrequencyHz = .23, reedFrequencyHz = .31;
    LightingSettings lighting = [] {
        LightingSettings s;
        s.legacyUnlit = false; s.ambient = .22f;
        s.directional = {true, {-.45f,-1,-.25f}, {.82f,.94f,1}, .95f};
        s.point = {true, {1,5.5f,-4}, {.28f,.82f,.90f}, 1.15f};
        s.attenuation = {1,.07f,.018f};
        s.environment.intensity = .26f; s.environment.fresnelPower = 5;
        return s;
    }();
};
inline const Settings settings;
}
