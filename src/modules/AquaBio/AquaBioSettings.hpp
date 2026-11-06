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
    // Frente: precipitación/captación; oeste: conducción; fondo: tratamiento;
    // este: medición y humedal. Los montajes se trasladan con sus mecanismos.
    std::array<glm::vec3, 28> positions{{
        {0,0,-1}, {0,.01f,17}, {0,.06f,11}, {-12,.01f,9.5f}, {-12,3.0f,7.48f},
        {-12,.01f,1.5f}, {-11.9f,.04f,7.48f}, {-12,.01f,-5.5f}, {-12,.91f,-5.34f},
        {-13.5f,.01f,-13}, {-13.5f,1.1f,-13}, {-8.5f,.01f,-19}, {-3.5f,.01f,-13},
        {-8.5f,.01f,-16.5f}, {3,.01f,-19}, {12.5f,.01f,-13}, {-12,.01f,-1.5f},
        {-9.5f,.01f,-1.8f}, {-12,1.10f,-1.35f}, {6,.01f,-4}, {-13,1.81f,16.5f},
        {-7,.01f,16.5f}, {14,.01f,5}, {12.3f,.36f,3.2f}, {10.9f,.30f,2.7f},
        {20.5f,.01f,1}, {18.8f,.01f,14.5f}, {12.3f,-.05f,9.9f}
    }};
    std::array<glm::vec3, 28> scale = [] {
        std::array<glm::vec3,28> v; v.fill(glm::vec3(1)); return v;
    }();
    std::array<float,28> yawDegrees{};
    std::array<bool,28> visible = [] {
        std::array<bool,28> v; v.fill(true); return v;
    }();
    // Eje central de 3.8 m; circuito de observación con ramales de 2.8 m.
    glm::vec3 mainWalkwayScale{3.8f/2.8f,1,1.5f};
    std::array<glm::vec3,9> extraWalkways{{
        {0,.06f,1}, {0,.06f,-9}, {4,.06f,-9}, {-5.5f,.06f,4.5f},
        {-7,.06f,-2.5f}, {-5.5f,.06f,-9}, {5.3f,.06f,5.5f}, {8.3f,.06f,9.5f}, {12.3f,.06f,12}
    }};
    std::array<float,9> extraWalkwayYaws{{0,0,90,90,0,90,90,0,90}};
    std::array<glm::vec3,9> extraWalkwayScales = [] {
        std::array<glm::vec3,9> v; v.fill(glm::vec3(1));
        for(std::size_t i=0;i<3;++i)v[i]={3.8f/2.8f,1,1.25f};
        v[2].z=1; // El antiguo fondo de saco pasa a ser el acceso al laboratorio.
        for(std::size_t i=3;i<6;++i)v[i].z=1.5f;
        v[4].z=1.75f; // Conecta captación y tratamiento sin cruzar la conducción.
        v[6].z=.85f; // Ramal corto: conserva el acceso lateral al humedal.
        return v;
    }();
    std::array<glm::vec3,5> extraFloatingPlants{{
        {15.5f,.36f,3}, {13.7f,.36f,4.6f}, {16.2f,.36f,5.4f}, {11.8f,.36f,6.1f}, {14.5f,.36f,7.1f}
    }};
    std::array<glm::vec3,5> extraReeds{{
        {17.3f,.3f,2.7f}, {17,.3f,4.0f}, {16.6f,.3f,7.3f}, {12,.3f,7.5f}, {10.9f,.3f,4.9f}
    }};
    std::array<glm::vec3,2> extraTrees{{{19.5f,.01f,10}, {15.5f,.01f,16.5f}}};
    glm::vec3 impellerDemoPosition{-9.5f,.44f,-5.5f};
    glm::vec3 cartridgeDemoPosition{-3.5f,.01f,-11.2f};
    glm::vec3 extraPipesPosition{13,.01f,-10};

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
