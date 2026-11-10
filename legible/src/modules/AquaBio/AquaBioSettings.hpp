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

    // ---- Ajustes por modelo (posición i = modelo AQUA número i+1, en metros) ----
    std::array<glm::vec3, 28> positions{{
        {0, 0, -1},              // 1 plataforma
        {0, .01f, 17},           // 2 pórtico de entrada
        {0, .06f, 11},           // 3 andador principal
        {-12, .01f, 9.5f},       // 4 techo de captación
        {-12, 3.0f, 7.48f},      // 5 canaleta
        {-12, .01f, 1.5f},       // 6 cisterna
        {-11.9f, .04f, 7.48f},   // 7 rejilla
        {-12, .01f, -5.5f},      // 8 bomba
        {-12, .91f, -5.34f},     // 9 impulsor de la bomba
        {-13.5f, .01f, -13},     // 10 sedimentador
        {-13.5f, 1.1f, -13},     // 11 rascador
        {-8.5f, .01f, -19},      // 12 filtro de arena
        {-3.5f, .01f, -13},      // 13 filtro de carbón
        {-8.5f, .01f, -16.5f},   // 14 cartucho
        {3, .01f, -19},          // 15 cámara UV
        {12.5f, .01f, -13},      // 16 tanque de agua tratada
        {-12, .01f, -1.5f},      // 17 tuberías
        {-9.5f, .01f, -1.8f},    // 18 válvula
        {-12, 1.10f, -1.35f},    // 19 indicador de flujo
        {6, .01f, -4},           // 20 toma de muestras
        {-13, 1.81f, 16.5f},     // 21 ciclo del agua
        {-7, .01f, 16.5f},       // 22 nube
        {14, .01f, 5},           // 23 humedal
        {12.3f, .36f, 3.2f},     // 24 planta flotante
        {10.9f, .30f, 2.7f},     // 25 juncos
        {20.5f, .01f, 1},        // 26 árbol ribereño
        {18.8f, .01f, 14.5f},    // 27 refugio de polinizadores
        {12.3f, -.05f, 9.9f}     // 28 peces
    }};
    std::array<glm::vec3, 28> scale{{
        {1, 1, 1},   // 1 plataforma
        {1, 1, 1},   // 2 pórtico de entrada
        {1, 1, 1},   // 3 andador principal
        {1, 1, 1},   // 4 techo de captación
        {1, 1, 1},   // 5 canaleta
        {1, 1, 1},   // 6 cisterna
        {1, 1, 1},   // 7 rejilla
        {1, 1, 1},   // 8 bomba
        {1, 1, 1},   // 9 impulsor de la bomba
        {1, 1, 1},   // 10 sedimentador
        {1, 1, 1},   // 11 rascador
        {1, 1, 1},   // 12 filtro de arena
        {1, 1, 1},   // 13 filtro de carbón
        {1, 1, 1},   // 14 cartucho
        {1, 1, 1},   // 15 cámara UV
        {1, 1, 1},   // 16 tanque de agua tratada
        {1, 1, 1},   // 17 tuberías
        {1, 1, 1},   // 18 válvula
        {1, 1, 1},   // 19 indicador de flujo
        {1, 1, 1},   // 20 toma de muestras
        {1, 1, 1},   // 21 ciclo del agua
        {1, 1, 1},   // 22 nube
        {1, 1, 1},   // 23 humedal
        {1, 1, 1},   // 24 planta flotante
        {1, 1, 1},   // 25 juncos
        {1, 1, 1},   // 26 árbol ribereño
        {1, 1, 1},   // 27 refugio de polinizadores
        {1, 1, 1}    // 28 peces
    }};
    // giro sobre Y en grados
    std::array<float, 28> yawDegrees{{
        0,   // 1 plataforma
        0,   // 2 pórtico de entrada
        0,   // 3 andador principal
        0,   // 4 techo de captación
        0,   // 5 canaleta
        0,   // 6 cisterna
        0,   // 7 rejilla
        0,   // 8 bomba
        0,   // 9 impulsor de la bomba
        0,   // 10 sedimentador
        0,   // 11 rascador
        0,   // 12 filtro de arena
        0,   // 13 filtro de carbón
        0,   // 14 cartucho
        0,   // 15 cámara UV
        0,   // 16 tanque de agua tratada
        0,   // 17 tuberías
        0,   // 18 válvula
        0,   // 19 indicador de flujo
        0,   // 20 toma de muestras
        0,   // 21 ciclo del agua
        0,   // 22 nube
        0,   // 23 humedal
        0,   // 24 planta flotante
        0,   // 25 juncos
        0,   // 26 árbol ribereño
        0,   // 27 refugio de polinizadores
        0    // 28 peces
    }};
    // false oculta todas las copias del modelo
    std::array<bool, 28> visible{{
        true,   // 1 plataforma
        true,   // 2 pórtico de entrada
        true,   // 3 andador principal
        true,   // 4 techo de captación
        true,   // 5 canaleta
        true,   // 6 cisterna
        true,   // 7 rejilla
        true,   // 8 bomba
        true,   // 9 impulsor de la bomba
        true,   // 10 sedimentador
        true,   // 11 rascador
        true,   // 12 filtro de arena
        true,   // 13 filtro de carbón
        true,   // 14 cartucho
        true,   // 15 cámara UV
        true,   // 16 tanque de agua tratada
        true,   // 17 tuberías
        true,   // 18 válvula
        true,   // 19 indicador de flujo
        true,   // 20 toma de muestras
        true,   // 21 ciclo del agua
        true,   // 22 nube
        true,   // 23 humedal
        true,   // 24 planta flotante
        true,   // 25 juncos
        true,   // 26 árbol ribereño
        true,   // 27 refugio de polinizadores
        true    // 28 peces
    }};

    // ---- Andadores extra (path-0 a path-8) ----
    glm::vec3 mainWalkwayScale{3.8f / 2.8f, 1, 1.5f};
    std::array<glm::vec3, 9> extraWalkways{{
        {0, .06f, 1},         // path-0
        {0, .06f, -9},        // path-1
        {4, .06f, -9},        // path-2 acceso al laboratorio
        {-5.5f, .06f, 4.5f},  // path-3
        {-7, .06f, -2.5f},    // path-4 entre captación y tratamiento
        {-5.5f, .06f, -9},    // path-5
        {5.3f, .06f, 5.5f},   // path-6 ramal corto al humedal
        {8.3f, .06f, 9.5f},   // path-7
        {12.3f, .06f, 12}     // path-8
    }};
    std::array<float, 9> extraWalkwayYaws{{
        0,    // path-0
        0,    // path-1
        90,   // path-2
        90,   // path-3
        0,    // path-4
        90,   // path-5
        90,   // path-6
        0,    // path-7
        90    // path-8
    }};
    std::array<glm::vec3, 9> extraWalkwayScales{{
        {3.8f / 2.8f, 1, 1.25f},   // path-0
        {3.8f / 2.8f, 1, 1.25f},   // path-1
        {3.8f / 2.8f, 1, 1},       // path-2
        {1, 1, 1.5f},              // path-3
        {1, 1, 1.75f},             // path-4
        {1, 1, 1.5f},              // path-5
        {1, 1, .85f},              // path-6
        {1, 1, 1},                 // path-7
        {1, 1, 1}                  // path-8
    }};

    // ---- Copias extra de modelos ----
    std::array<glm::vec3, 5> extraFloatingPlants{{
        {15.5f, .36f, 3},
        {13.7f, .36f, 4.6f},
        {16.2f, .36f, 5.4f},
        {11.8f, .36f, 6.1f},
        {14.5f, .36f, 7.1f}
    }};
    std::array<glm::vec3, 5> extraReeds{{
        {17.3f, .3f, 2.7f},
        {17, .3f, 4.0f},
        {16.6f, .3f, 7.3f},
        {12, .3f, 7.5f},
        {10.9f, .3f, 4.9f}
    }};
    std::array<glm::vec3, 2> extraTrees{{
        {19.5f, .01f, 10},
        {15.5f, .01f, 16.5f}
    }};
    glm::vec3 impellerDemoPosition{-9.5f, .44f, -5.5f};    // impulsor que se puede tomar
    glm::vec3 cartridgeDemoPosition{-3.5f, .01f, -11.2f};  // cartucho que se puede tomar
    glm::vec3 extraPipesPosition{13, .01f, -10};

    // ---- Animaciones ----
    float pumpSpeedDeg = 110;          // impulsor, grados por segundo
    float scraperSpeedDeg = 12;        // rascador
    float flowSpeedDeg = 55;           // indicador de flujo
    float cycleSpeedDeg = 16;          // ciclo del agua
    double gutterDurationSeconds = 8;
    double cartridgeDurationSeconds = 10;
    float gutterSwingDegrees = 6;
    float cartridgeLiftMeters = .85f;
    float cartridgeTravelMeters = .60f;
    float floatingAmplitudeMeters = .055f;
    float reedAmplitudeDegrees = 5;
    double floatingFrequencyHz = .23;
    double reedFrequencyHz = .31;

    // ---- Iluminación ----
    LightingSettings lighting = [] {
        LightingSettings light;
        light.legacyUnlit = false;
        light.ambient = .22f;
        light.directional = {true, {-.45f, -1, -.25f}, {.82f, .94f, 1}, .95f};
        light.point = {true, {1, 5.5f, -4}, {.28f, .82f, .90f}, 1.15f};
        light.attenuation = {1, .07f, .018f};
        light.environment.intensity = .26f;
        light.environment.fresnelPower = 5;
        return light;
    }();
};

inline const Settings settings;
}
