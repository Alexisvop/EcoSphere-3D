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

    // Índice = número ENERGY menos uno; permite escalar, girar u ocultar por modelo en clase.
    std::array<glm::vec3, 28> modelScale = [] {
        std::array<glm::vec3, 28> v{};
        v.fill(glm::vec3(1.0f));
        return v;
    }();
    std::array<float, 28> modelYawDegrees{};
    std::array<bool, 28> visible = [] {
        std::array<bool, 28> v{};
        v.fill(true);
        return v;
    }();

    // Zona 1: Generación solar (NO + estación demostrativa frontal).
    glm::vec3 solarPanelScale{1.0f, 1.0f, 1.0f};
    float solarTiltDegrees = 26.5f;
    glm::vec3 solarDemoPosition{-2.4f, 0.01f, 1.4f};
    float solarDemoYaw = 25.0f;
    std::array<glm::vec3, 3> solarArrayPositions{{{ -11.8f, 0.01f, -9.2f }, { -9.2f, 0.01f, -9.2f }, { -6.6f, 0.01f, -9.2f }}};
    float solarArrayYaw = 15.0f;
    glm::vec3 trackerPosition{-4.2f, 0.01f, -8.2f};
    glm::vec3 inverterPosition{-13.2f, 0.01f, -6.2f};
    std::array<glm::vec3, 2> combinerPositions{{{ -11.6f, 0.01f, -6.8f }, { -5.6f, 0.01f, -6.8f }}};

    // Zona 2: Generación eólica y electromagnetismo (N / NE).
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

    // Zona 3: Transformación, medición y distribución eléctrica.
    glm::vec3 transformerPosition{12.2f, 0.01f, -2.0f};
    std::array<glm::vec3, 2> polePositions{{{ 14.2f, 0.01f, -7.5f }, { 14.2f, 0.01f, 2.2f }}};
    std::array<glm::vec3, 2> smartMeterPositions{{{ -2.4f, 0.01f, -1.6f }, { 2.4f, 0.01f, -1.6f }}};

    // Zona 4: Almacenamiento en baterías BESS (SE).
    glm::vec3 mainBessPosition{8.2f, 0.01f, 1.2f};
    float mainBessYaw = 0.0f;
    glm::vec3 secondaryBessPosition{10.4f, 0.01f, 1.2f};
    glm::vec3 interactiveBatteryPosition{5.6f, 0.01f, 1.8f};
    glm::vec3 spareBatteryPosition{6.7f, 0.01f, 1.4f};

    // Zona 5: Movilidad eléctrica y consumo responsable (SO).
    std::array<glm::vec3, 2> chargerPositions{{{ -11.2f, 0.01f, -3.2f }, { -8.4f, 0.01f, 0.8f }}};
    glm::vec3 shuttleEvPosition{-11.2f, 0.01f, 4.2f};
    float shuttleEvYaw = 180.0f;
    glm::vec3 interactiveEvPosition{-8.4f, 0.01f, 6.8f};
    float interactiveEvYaw = 165.0f;
    glm::vec3 chargeConnectorPosition{-5.6f, 0.01f, 2.0f};

    // Paneles educativos por estación (Solar, Eólica, Almacenamiento, Movilidad).
    std::array<glm::vec3, 4> infoPanelPositions{{{ -4.2f, 0.01f, -1.8f }, { 2.4f, 0.01f, -7.8f }, { 3.6f, 0.01f, 1.6f }, { -5.6f, 0.01f, 0.6f }}};
    std::array<float, 4> infoPanelYaws{{ 25.0f, 18.0f, -18.0f, 15.0f }};

    // Animaciones educativas (4 simples, 2 keyframes, 2 procedurales; reloj en segundos reales).
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

    // Vacío por defecto para compartir los 28 recursos CPU/GPU sin duplicar mallas; editable en clase.
    std::optional<VisualProperties> solarPanelVisualOverride;

    LightingSettings lighting = [] {
        LightingSettings s;
        s.legacyUnlit = false;
        s.ambient = 0.18f;
        s.directional = {true, {-0.45f, -1.0f, -0.35f}, {1.0f, 0.93f, 0.76f}, 0.92f};
        s.point = {true, {0.0f, 5.2f, -3.0f}, {1.0f, 0.78f, 0.35f}, 1.45f};
        s.attenuation = {1.0f, 0.07f, 0.018f};
        s.environment.intensity = 0.32f;
        s.environment.fresnelPower = 5.0f;
        return s;
    }();
};

inline const Settings settings;
}
