#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>

namespace ecosphere::plaza {
enum class Model {
    pavement = 1, perimeter, path, pedestal, globe, basin, jets, arch,
    platform, pergola, solarCanopy, signpost, kiosk, panel, bench, lamp, bicycleRack,
    wasteStation, drinkingFountain, planter, broadTree, smallTree, flowerBush, grasses, rocks
};

struct Settings {
    // ---- Medidas de la plaza (metros) ----
    float radius = 10.5f;
    float pathWidth = 3.6f;
    float pathLength = 8;
    glm::vec3 sculpturePosition{0, .93f, 0};

    // ---- Escala por modelo (i = modelo PLAZA número i+1, aplica a todas sus copias) ----
    std::array<glm::vec3, 25> modelScale{{
        {1, 1, 1},   // 1 pavimento
        {1, 1, 1},   // 2 borde perimetral
        {1, 1, 1},   // 3 sendero
        {1, 1, 1},   // 4 pedestal
        {1, 1, 1},   // 5 globo escultura
        {1, 1, 1},   // 6 fuente
        {1, 1, 1},   // 7 chorros de agua
        {1, 1, 1},   // 8 arco de acceso
        {1, 1, 1},   // 9 plataforma de acceso
        {1, 1, 1},   // 10 pérgola
        {1, 1, 1},   // 11 techo solar
        {1, 1, 1},   // 12 letrero
        {1, 1, 1},   // 13 kiosco
        {1, 1, 1},   // 14 panel informativo
        {1, 1, 1},   // 15 banca
        {1, 1, 1},   // 16 lámpara
        {1, 1, 1},   // 17 bicicletero
        {1, 1, 1},   // 18 estación de residuos
        {1, 1, 1},   // 19 bebedero
        {1, 1, 1},   // 20 jardinera
        {1, 1, 1},   // 21 árbol grande
        {1, 1, 1},   // 22 árbol pequeño
        {1, 1, 1},   // 23 arbusto con flores
        {1, 1, 1},   // 24 pastos
        {1, 1, 1}    // 25 rocas
    }};
    // giro extra sobre Y en grados
    std::array<float, 25> modelYawDegrees{{
        0,   // 1 pavimento
        0,   // 2 borde perimetral
        0,   // 3 sendero
        0,   // 4 pedestal
        0,   // 5 globo escultura
        0,   // 6 fuente
        0,   // 7 chorros de agua
        0,   // 8 arco de acceso
        0,   // 9 plataforma de acceso
        0,   // 10 pérgola
        0,   // 11 techo solar
        0,   // 12 letrero
        0,   // 13 kiosco
        0,   // 14 panel informativo
        0,   // 15 banca
        0,   // 16 lámpara
        0,   // 17 bicicletero
        0,   // 18 estación de residuos
        0,   // 19 bebedero
        0,   // 20 jardinera
        0,   // 21 árbol grande
        0,   // 22 árbol pequeño
        0,   // 23 arbusto con flores
        0,   // 24 pastos
        0    // 25 rocas
    }};

    // ---- Cámara y ventana ----
    glm::vec3 cameraStart{0, 1.7f, 19};
    float cameraYaw = -90;
    float cameraPitch = 1;
    int windowWidth = 1280;
    int windowHeight = 720;

    // ---- Cuántas copias de cada cosa ----
    unsigned benchCount = 6;
    unsigned broadTreeCount = 8;
    unsigned smallTreeCount = 4;
    unsigned lampCount = 8;
    unsigned rockCount = 8;
    unsigned grassCount = 16;
    float treeRingOffset = 7;   // distancia de los árboles grandes al borde

    // ---- Posiciones (metros, +Y arriba) ----
    std::array<glm::vec3, 6> benches{{
        {-6, 0, 5.4f},
        {6, 0, 5.4f},
        {-6, 0, -5.4f},
        {6, 0, -5.4f},
        {-8, 0, 0},
        {8, 0, 0}
    }};
    std::array<glm::vec3, 4> planters{{
        {-8.2f, 0, 7.8f},
        {8.2f, 0, 7.8f},
        {-8.2f, 0, -7.8f},
        {8.2f, 0, -7.8f}
    }};
    std::array<glm::vec3, 4> smallTrees{{
        {-10, 0, -13},
        {10, 0, -13},
        {-18, 0, -10},
        {18, 0, -10}
    }};
    glm::vec3 pergolaPosition{12, 0, 9.5f};
    glm::vec3 solarPosition{-12, 0, 9.5f};
    glm::vec3 signPosition{-2.8f, 0, 11.2f};
    glm::vec3 kioskPosition{3.8f, 0, 9.5f};
    glm::vec3 bicyclePosition{-5, 0, 10.5f};
    std::array<glm::vec3, 2> panels{{
        {-5, 0, 6.5f},     // water
        {6.3f, 0, -6.3f}   // cycle
    }};
    std::array<glm::vec3, 3> wasteStations{{
        {-4.5f, 0, 10},
        {4.7f, 0, 10},
        {5, 0, -10}
    }};
    std::array<glm::vec3, 2> drinkingFountains{{
        {-5.7f, 0, 6.7f},   // west
        {5.7f, 0, 6.7f}     // east
    }};

    // false oculta todas las copias del modelo
    std::array<bool, 25> visible{{
        true,   // 1 pavimento
        true,   // 2 borde perimetral
        true,   // 3 sendero
        true,   // 4 pedestal
        true,   // 5 globo escultura
        true,   // 6 fuente
        true,   // 7 chorros de agua
        true,   // 8 arco de acceso
        true,   // 9 plataforma de acceso
        true,   // 10 pérgola
        true,   // 11 techo solar
        true,   // 12 letrero
        true,   // 13 kiosco
        true,   // 14 panel informativo
        true,   // 15 banca
        true,   // 16 lámpara
        true,   // 17 bicicletero
        true,   // 18 estación de residuos
        true,   // 19 bebedero
        true,   // 20 jardinera
        true,   // 21 árbol grande
        true,   // 22 árbol pequeño
        true,   // 23 arbusto con flores
        true,   // 24 pastos
        true    // 25 rocas
    }};

    // ---- Iluminación ----
    LightingSettings lighting = [] {
        LightingSettings light;
        light.legacyUnlit = false;
        light.ambient = .20f;
        light.directional = {true, {-.4f, -1, -.3f}, {1, .95f, .82f}, .85f};
        light.point = {true, {0, 4.8f, 3}, {1, .82f, .55f}, 1.1f};
        light.environment.intensity = .28f;
        return light;
    }();
};

inline const Settings settings;
}
