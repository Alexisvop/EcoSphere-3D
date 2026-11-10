#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>

namespace ecosphere::ecocycle {
enum class Model {
    platform = 1, gateway, walkway, kiosk, sorting, feeder, conveyor, roller, sensor, gate,
    magnetic, press, ram, bale, shredder, rotor, washer, extruder, spool, glassCrusher, glassBin,
    compost, workbench, chair
};

struct Settings {
    glm::vec3 clearColor{.04f, .14f, .06f};

    // ---- Ajustes por modelo (posición i = modelo CYCLE número i+1, en metros, -Z es norte) ----
    std::array<glm::vec3, 24> positions{{
        {0.000f, 0.000f, -1.000f},     // 1 plataforma
        {0.000f, 0.010f, 18.000f},     // 2 pórtico de entrada
        {0.000f, 0.040f, 12.000f},     // 3 andador principal
        {-7.500f, 0.010f, 14.000f},    // 4 kiosco
        {-16.000f, 0.010f, 9.000f},    // 5 estación de separación
        {-16.000f, 0.010f, 1.000f},    // 6 alimentador
        {-15.000f, 0.010f, -8.000f},   // 7 banda transportadora
        {-15.000f, 1.310f, -6.000f},   // 8 rodillo
        {-16.000f, 2.610f, 1.000f},    // 9 sensor
        {-16.000f, 1.810f, -0.500f},   // 10 compuerta
        {-15.000f, 1.660f, -10.000f},  // 11 separador magnético
        {-16.000f, 0.010f, -18.000f},  // 12 prensa
        {-16.000f, 2.710f, -18.000f},  // 13 pistón de la prensa
        {-7.500f, 0.010f, -18.000f},   // 14 paca
        {-7.000f, 0.010f, -8.000f},    // 15 trituradora
        {-7.000f, 1.510f, -8.000f},    // 16 rotor de la trituradora
        {14.000f, 0.010f, -18.000f},   // 17 lavadora
        {14.000f, 0.010f, -8.000f},    // 18 extrusora
        {7.500f, 0.010f, -18.000f},    // 19 carrete
        {7.500f, 0.010f, -1.000f},     // 20 trituradora de vidrio
        {16.000f, 0.010f, 1.000f},     // 21 contenedor de vidrio
        {16.000f, 0.010f, 20.000f},    // 22 composta
        {16.000f, 0.010f, 12.000f},    // 23 mesa de trabajo
        {7.500f, 0.010f, 12.000f}      // 24 silla
    }};
    std::array<glm::vec3, 24> scale{{
        {1, 1, 1},   // 1 plataforma
        {1, 1, 1},   // 2 pórtico de entrada
        {1, 1, 1},   // 3 andador principal
        {1, 1, 1},   // 4 kiosco
        {1, 1, 1},   // 5 estación de separación
        {1, 1, 1},   // 6 alimentador
        {1, 1, 1},   // 7 banda transportadora
        {1, 1, 1},   // 8 rodillo
        {1, 1, 1},   // 9 sensor
        {1, 1, 1},   // 10 compuerta
        {1, 1, 1},   // 11 separador magnético
        {1, 1, 1},   // 12 prensa
        {1, 1, 1},   // 13 pistón de la prensa
        {1, 1, 1},   // 14 paca
        {1, 1, 1},   // 15 trituradora
        {1, 1, 1},   // 16 rotor de la trituradora
        {1, 1, 1},   // 17 lavadora
        {1, 1, 1},   // 18 extrusora
        {1, 1, 1},   // 19 carrete
        {1, 1, 1},   // 20 trituradora de vidrio
        {1, 1, 1},   // 21 contenedor de vidrio
        {1, 1, 1},   // 22 composta
        {1, 1, 1},   // 23 mesa de trabajo
        {1, 1, 1}    // 24 silla
    }};
    // giro sobre Y en grados
    std::array<float, 24> yawDegrees{{
        0,   // 1 plataforma
        0,   // 2 pórtico de entrada
        0,   // 3 andador principal
        0,   // 4 kiosco
        0,   // 5 estación de separación
        0,   // 6 alimentador
        0,   // 7 banda transportadora
        0,   // 8 rodillo
        0,   // 9 sensor
        0,   // 10 compuerta
        0,   // 11 separador magnético
        0,   // 12 prensa
        0,   // 13 pistón de la prensa
        0,   // 14 paca
        0,   // 15 trituradora
        0,   // 16 rotor de la trituradora
        0,   // 17 lavadora
        0,   // 18 extrusora
        0,   // 19 carrete
        0,   // 20 trituradora de vidrio
        0,   // 21 contenedor de vidrio
        0,   // 22 composta
        0,   // 23 mesa de trabajo
        0    // 24 silla
    }};
    // false oculta todas las copias del modelo
    std::array<bool, 24> visible{{
        true,   // 1 plataforma
        true,   // 2 pórtico de entrada
        true,   // 3 andador principal
        true,   // 4 kiosco
        true,   // 5 estación de separación
        true,   // 6 alimentador
        true,   // 7 banda transportadora
        true,   // 8 rodillo
        true,   // 9 sensor
        true,   // 10 compuerta
        true,   // 11 separador magnético
        true,   // 12 prensa
        true,   // 13 pistón de la prensa
        true,   // 14 paca
        true,   // 15 trituradora
        true,   // 16 rotor de la trituradora
        true,   // 17 lavadora
        true,   // 18 extrusora
        true,   // 19 carrete
        true,   // 20 trituradora de vidrio
        true,   // 21 contenedor de vidrio
        true,   // 22 composta
        true,   // 23 mesa de trabajo
        true    // 24 silla
    }};

    // ---- Andadores extra (path-0 a path-2 centrales, path-3 a path-6 ramales) ----
    float mainPathWidth = 3.8f;
    float secondaryPathWidth = 2.8f;
    std::array<glm::vec3, 7> extraPaths{{
        {0.000f, 0.040f, 2.000f},     // path-0
        {0.000f, 0.040f, -8.000f},    // path-1
        {0.000f, 0.040f, -18.000f},   // path-2
        {-9.000f, 0.040f, 5.000f},    // path-3
        {9.000f, 0.040f, 5.000f},     // path-4
        {-9.000f, 0.040f, -12.000f},  // path-5
        {9.000f, 0.040f, -12.000f}    // path-6
    }};

    // ---- Copias extra de modelos ----
    std::array<glm::vec3, 1> extraKiosks{{
        {6.500f, 0.010f, 18.000f}
    }};
    std::array<glm::vec3, 2> extraBales{{
        {-9.500f, 0.010f, -20.500f},
        {-6.000f, 0.010f, -20.500f}
    }};
    std::array<glm::vec3, 2> extraSpools{{
        {6.000f, 0.010f, -20.500f},
        {9.000f, 0.010f, -20.500f}
    }};
    std::array<glm::vec3, 2> extraChairs{{
        {7.000f, 0.010f, 14.500f},
        {10.000f, 0.010f, 14.500f}
    }};

    // ---- Cámara inicial ----
    glm::vec3 cameraPosition{0, 1.7f, 13};
    float cameraYaw = -90;
    float cameraPitch = -4;

    // ---- Recorrido por zonas (posición de la cámara y punto al que mira) ----
    std::array<glm::vec3, 12> tourPositions{{
        {0, 1.7f, 13},
        {-10, 1.7f, 5},
        {0, 1.7f, 5},
        {0, 1.7f, -12},
        {-10, 1.7f, -12},
        {0, 1.7f, -12},
        {0, 1.7f, -18},
        {10, 1.7f, -12},
        {0, 1.7f, -12},
        {0, 1.7f, 5},
        {10, 1.7f, 5},
        {12, 1.7f, 16}
    }};
    std::array<glm::vec3, 12> tourTargets{{
        {-7, 1.5f, 14},
        {-16, 1.5f, 9},
        {-16, 1.5f, 1},
        {-15, 1.5f, -8},
        {-16, 1.5f, -18},
        {-7, 1.5f, -8},
        {7.5f, 1, -18},
        {14, 1.5f, -18},
        {14, 1.5f, -8},
        {7.5f, 1.5f, -1},
        {16, 1, 1},
        {16, 1.2f, 12}
    }};

    // ---- Animaciones ----
    float rollerSpeedDeg = 60;    // rodillo, grados por segundo
    float magnetSpeedDeg = 22;    // separador magnético
    float rotorSpeedDeg = 75;     // rotor de la trituradora
    float compostSpeedDeg = 18;   // composta
    double gateDurationSeconds = 8;
    double pressDurationSeconds = 10;
    float gateOpeningDeg = 55;
    float pressTravelMeters = .75f;
    float feederAmplitudeMeters = .055f;
    float sensorAmplitudeDegrees = 24;
    double feederFrequencyHz = 2.2;
    double sensorFrequencyHz = .22;

    // ---- Iluminación ----
    LightingSettings lighting = [] {
        LightingSettings light;
        light.legacyUnlit = false;
        light.ambient = .24f;
        light.directional = {true, {-.4f, -1, -.3f}, {1, .95f, .82f}, .9f};
        light.point = {true, {0, 5, -4}, {.68f, 1, .68f}, 1.1f};
        light.attenuation = {1, .07f, .018f};
        light.environment.intensity = .25f;
        light.environment.fresnelPower = 5;
        return light;
    }();
};

inline const Settings settings;
}
