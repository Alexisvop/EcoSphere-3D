#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>
#include <optional>

namespace ecosphere::ecoenergy {
enum class Model {
    platform = 1, gateway, walkway, solarMount, solarPanel, solarTracker, inverter, combinerBox,
    smartMeter, batteryCabinet, batteryModule, chargeIndicator, transformer, distributionPole, cableTray,
    windTower, turbineNacelle, windRotor, bladeSection, weathervane, anemometer, cutawayGenerator,
    generatorCoil, demoFan, chargingStation, electricVehicle, chargeConnector, infoPanel
};

struct Settings {
    glm::vec3 clearColor{0.12f, 0.10f, 0.03f};
    glm::vec3 platformPosition{0.0f, 0.0f, -1.0f};
    glm::vec3 gatewayPosition{0.0f, 0.01f, 10.5f};

    // ---- Ajustes por modelo (i = modelo ENERGY número i+1, aplica a todas sus copias) ----
    std::array<glm::vec3, 28> modelScale{{
        {1.0f, 1.0f, 1.0f},   // 1 plataforma
        {1.0f, 1.0f, 1.0f},   // 2 pórtico de entrada
        {1.0f, 1.0f, 1.0f},   // 3 andador
        {1.0f, 1.0f, 1.0f},   // 4 soporte solar
        {1.0f, 1.0f, 1.0f},   // 5 panel solar
        {1.0f, 1.0f, 1.0f},   // 6 seguidor solar
        {1.0f, 1.0f, 1.0f},   // 7 inversor
        {1.0f, 1.0f, 1.0f},   // 8 caja combinadora
        {1.0f, 1.0f, 1.0f},   // 9 medidor inteligente
        {1.0f, 1.0f, 1.0f},   // 10 gabinete de baterías
        {1.0f, 1.0f, 1.0f},   // 11 módulo de batería
        {1.0f, 1.0f, 1.0f},   // 12 indicador de carga
        {1.0f, 1.0f, 1.0f},   // 13 transformador
        {1.0f, 1.0f, 1.0f},   // 14 poste de distribución
        {1.0f, 1.0f, 1.0f},   // 15 charola de cables
        {1.0f, 1.0f, 1.0f},   // 16 torre eólica
        {1.0f, 1.0f, 1.0f},   // 17 góndola
        {1.0f, 1.0f, 1.0f},   // 18 rotor eólico
        {1.0f, 1.0f, 1.0f},   // 19 sección de aspa
        {1.0f, 1.0f, 1.0f},   // 20 veleta
        {1.0f, 1.0f, 1.0f},   // 21 anemómetro
        {1.0f, 1.0f, 1.0f},   // 22 generador en corte
        {1.0f, 1.0f, 1.0f},   // 23 bobina del generador
        {1.0f, 1.0f, 1.0f},   // 24 ventilador demo
        {1.0f, 1.0f, 1.0f},   // 25 cargador
        {1.0f, 1.0f, 1.0f},   // 26 vehículo eléctrico
        {1.0f, 1.0f, 1.0f},   // 27 conector de carga
        {1.0f, 1.0f, 1.0f}    // 28 panel informativo
    }};
    // giro extra sobre Y en grados
    std::array<float, 28> modelYawDegrees{{
        0,   // 1 plataforma
        0,   // 2 pórtico de entrada
        0,   // 3 andador
        0,   // 4 soporte solar
        0,   // 5 panel solar
        0,   // 6 seguidor solar
        0,   // 7 inversor
        0,   // 8 caja combinadora
        0,   // 9 medidor inteligente
        0,   // 10 gabinete de baterías
        0,   // 11 módulo de batería
        0,   // 12 indicador de carga
        0,   // 13 transformador
        0,   // 14 poste de distribución
        0,   // 15 charola de cables
        0,   // 16 torre eólica
        0,   // 17 góndola
        0,   // 18 rotor eólico
        0,   // 19 sección de aspa
        0,   // 20 veleta
        0,   // 21 anemómetro
        0,   // 22 generador en corte
        0,   // 23 bobina del generador
        0,   // 24 ventilador demo
        0,   // 25 cargador
        0,   // 26 vehículo eléctrico
        0,   // 27 conector de carga
        0    // 28 panel informativo
    }};
    // false oculta todas las copias del modelo
    std::array<bool, 28> visible{{
        true,   // 1 plataforma
        true,   // 2 pórtico de entrada
        true,   // 3 andador
        true,   // 4 soporte solar
        true,   // 5 panel solar
        true,   // 6 seguidor solar
        true,   // 7 inversor
        true,   // 8 caja combinadora
        true,   // 9 medidor inteligente
        true,   // 10 gabinete de baterías
        true,   // 11 módulo de batería
        true,   // 12 indicador de carga
        true,   // 13 transformador
        true,   // 14 poste de distribución
        true,   // 15 charola de cables
        true,   // 16 torre eólica
        true,   // 17 góndola
        true,   // 18 rotor eólico
        true,   // 19 sección de aspa
        true,   // 20 veleta
        true,   // 21 anemómetro
        true,   // 22 generador en corte
        true,   // 23 bobina del generador
        true,   // 24 ventilador demo
        true,   // 25 cargador
        true,   // 26 vehículo eléctrico
        true,   // 27 conector de carga
        true    // 28 panel informativo
    }};

    // ---- Zona 1: generación solar ----
    glm::vec3 solarPanelScale{1.0f, 1.0f, 1.0f};
    float solarTiltDegrees = 26.5f;
    glm::vec3 solarDemoPosition{-2.4f, 0.01f, 1.4f};   // panel ajustable del frente
    float solarDemoYaw = 25.0f;
    std::array<glm::vec3, 3> solarArrayPositions{{
        {-11.8f, 0.01f, -9.2f},   // mount-0 / panel-0
        {-9.2f, 0.01f, -9.2f},    // mount-1 / panel-1
        {-6.6f, 0.01f, -9.2f}     // mount-2 / panel-2
    }};
    float solarArrayYaw = 15.0f;
    glm::vec3 trackerPosition{-4.2f, 0.01f, -8.2f};
    glm::vec3 inverterPosition{-13.2f, 0.01f, -6.2f};
    std::array<glm::vec3, 2> combinerPositions{{
        {-11.6f, 0.01f, -6.8f},   // west
        {-5.6f, 0.01f, -6.8f}     // east
    }};

    // ---- Zona 2: generación eólica y electromagnetismo ----
    glm::vec3 mainTurbinePosition{3.4f, 0.01f, -14.2f};
    float mainTurbineYaw = 0.0f;
    glm::vec3 secondaryTurbinePosition{11.4f, 0.01f, -13.4f};
    float secondaryTurbineYaw = -18.0f;
    float secondaryTurbineScale = 0.72f;
    glm::vec3 bladeDemoPosition{4.6f, 0.01f, -6.6f};
    glm::vec3 demoFanPosition{7.0f, 0.01f, -6.8f};
    glm::vec3 weathervaneMastPosition{8.6f, 0.01f, -8.8f};
    glm::vec3 anemometerMastPosition{10.2f, 0.01f, -8.8f};
    glm::vec3 generatorPosition{10.8f, 0.01f, -6.4f};
    glm::vec3 generatorCoilPosition{9.3f, 0.38f, -5.8f};

    // ---- Zona 3: transformación, medición y distribución ----
    glm::vec3 transformerPosition{12.2f, 0.01f, -2.0f};
    std::array<glm::vec3, 2> polePositions{{
        {14.2f, 0.01f, -7.5f},   // north
        {14.2f, 0.01f, 2.2f}     // south
    }};
    std::array<glm::vec3, 2> smartMeterPositions{{
        {-2.4f, 0.01f, -1.6f},   // generation
        {2.4f, 0.01f, -1.6f}     // consumption
    }};

    // ---- Zona 4: almacenamiento en baterías ----
    glm::vec3 mainBessPosition{8.2f, 0.01f, 1.2f};
    float mainBessYaw = 0.0f;
    glm::vec3 secondaryBessPosition{10.4f, 0.01f, 1.2f};
    glm::vec3 interactiveBatteryPosition{5.6f, 0.01f, 1.8f};
    glm::vec3 spareBatteryPosition{6.7f, 0.01f, 1.4f};

    // ---- Zona 5: movilidad eléctrica ----
    std::array<glm::vec3, 2> chargerPositions{{
        {-11.2f, 0.01f, -3.2f},   // charger-lane
        {-8.4f, 0.01f, 0.8f}      // charger-visitor
    }};
    glm::vec3 shuttleEvPosition{-11.2f, 0.01f, 4.2f};
    float shuttleEvYaw = 180.0f;
    glm::vec3 interactiveEvPosition{-8.4f, 0.01f, 6.8f};
    float interactiveEvYaw = 165.0f;
    glm::vec3 chargeConnectorPosition{-5.6f, 0.01f, 2.0f};

    // ---- Paneles informativos ----
    std::array<glm::vec3, 4> infoPanelPositions{{
        {-4.2f, 0.01f, -1.8f},   // solar
        {2.4f, 0.01f, -7.8f},    // wind
        {3.6f, 0.01f, 1.6f},     // storage
        {-5.6f, 0.01f, 0.6f}     // mobility
    }};
    std::array<float, 4> infoPanelYaws{{
        25.0f,    // solar
        18.0f,    // wind
        -18.0f,   // storage
        15.0f     // mobility
    }};

    // ---- Animaciones (velocidades en grados por segundo) ----
    float mainRotorSpeedDeg = 48.0f;
    float secondaryRotorSpeedDeg = 78.0f;
    float anemometerSpeedDeg = 135.0f;
    float demoFanSpeedDeg = 180.0f;

    double trackerDurationSeconds = 10.0;
    float trackerSwingDegrees = 32.0f;
    double evShuttleDurationSeconds = 12.0;
    float evShuttleTravelMeters = 4.6f;

    float weathervaneAmplitudeDeg = 28.0f;
    double weathervaneFrequencyHz = 0.22;
    float chargeIndicatorAmplitudeMeters = 0.54f;
    double chargeIndicatorFrequencyHz = 0.16;

    // color o material propio para los paneles solares (vacío usa el del modelo)
    std::optional<VisualProperties> solarPanelVisualOverride;

    // ---- Iluminación ----
    LightingSettings lighting = [] {
        LightingSettings light;
        light.legacyUnlit = false;
        light.ambient = 0.18f;
        light.directional = {true, {-0.45f, -1.0f, -0.35f}, {1.0f, 0.93f, 0.76f}, 0.92f};
        light.point = {true, {0.0f, 5.2f, -3.0f}, {1.0f, 0.78f, 0.35f}, 1.45f};
        light.attenuation = {1.0f, 0.07f, 0.018f};
        light.environment.intensity = 0.32f;
        light.environment.fresnelPower = 5.0f;
        return light;
    }();
};

inline const Settings settings;
}
