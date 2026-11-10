#include "EcoEnergy.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
constexpr bool selectable = true;
constexpr bool notSelectable = false;

bool outside(double value, double minimum, double maximum) {
    return !std::isfinite(value) || value < minimum || value > maximum;
}

void validate(const ecoenergy::Settings& settings) {
    const glm::vec3 color = settings.clearColor;
    if (!finiteVector(color) || glm::any(glm::lessThan(color, glm::vec3(0.0f))) ||
        glm::any(glm::greaterThan(color, glm::vec3(1.0f))))
        throw std::invalid_argument("Invalid EcoEnergy clear color");

    if (!finiteVector(settings.solarPanelScale) ||
        glm::any(glm::lessThanEqual(settings.solarPanelScale, glm::vec3(0.01f))) ||
        !std::isfinite(settings.solarTiltDegrees) || std::abs(settings.solarTiltDegrees) > 85.0f ||
        !std::isfinite(settings.secondaryTurbineScale) || settings.secondaryTurbineScale <= 0.1f ||
        settings.secondaryTurbineScale > 4.0f)
        throw std::invalid_argument("Invalid EcoEnergy scale or tilt parameter");

    for (float speed : {settings.mainRotorSpeedDeg, settings.secondaryRotorSpeedDeg,
                        settings.anemometerSpeedDeg, settings.demoFanSpeedDeg}) {
        if (!std::isfinite(speed) || std::abs(speed) > 1440.0f)
            throw std::invalid_argument("Invalid EcoEnergy simple animation speed");
    }

    if (!std::isfinite(settings.trackerDurationSeconds) || settings.trackerDurationSeconds <= 0.1 ||
        !std::isfinite(settings.evShuttleDurationSeconds) || settings.evShuttleDurationSeconds <= 0.1 ||
        !std::isfinite(settings.trackerSwingDegrees) || std::abs(settings.trackerSwingDegrees) > 80.0f ||
        !std::isfinite(settings.evShuttleTravelMeters) || settings.evShuttleTravelMeters < 0.0f ||
        settings.evShuttleTravelMeters > 12.0f)
        throw std::invalid_argument("Invalid EcoEnergy keyframe animation parameter");

    if (!std::isfinite(settings.weathervaneAmplitudeDeg) ||
        std::abs(settings.weathervaneAmplitudeDeg) > 180.0f ||
        outside(settings.weathervaneFrequencyHz, 0.0, 20.0) ||
        !std::isfinite(settings.chargeIndicatorAmplitudeMeters) ||
        std::abs(settings.chargeIndicatorAmplitudeMeters) > 0.8f ||
        outside(settings.chargeIndicatorFrequencyHz, 0.0, 20.0))
        throw std::invalid_argument("Invalid EcoEnergy procedural animation parameter");

    validateLighting(settings.lighting, glm::vec3{0.0f, 1.7f, 6.0f});
}

glm::quat yawQuat(float degrees) {
    return glm::angleAxis(glm::radians(degrees), glm::vec3{0.0f, 1.0f, 0.0f});
}

// giro en Y y luego inclinación en X (paneles solares)
glm::quat tiltedYaw(float yawDegrees, float tiltDegrees) {
    const glm::quat tilt = glm::angleAxis(glm::radians(tiltDegrees), glm::vec3{1.0f, 0.0f, 0.0f});
    return glm::normalize(yawQuat(yawDegrees) * tilt);
}

// ---- Animaciones ----

AnimationDescriptor spin(float speedDegrees, glm::vec3 axis) {
    return AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation, axis, speedDegrees, 0.0}};
}

// seguidor solar que se inclina de un lado a otro
AnimationDescriptor makeSolarTrackerAnimation(double duration, float swingDegrees) {
    const auto roll = [](float degrees) {
        return glm::angleAxis(glm::radians(degrees), glm::vec3{0.0f, 0.0f, 1.0f});
    };
    const glm::vec3 origin{0.0f, 0.0f, 0.0f};
    const glm::vec3 unit{1.0f, 1.0f, 1.0f};
    KeyframeTrack track({
        {0.00 * duration, {origin, roll(-swingDegrees), unit}},
        {0.25 * duration, {origin, roll(0.0f),          unit}},
        {0.50 * duration, {origin, roll(swingDegrees),  unit}},
        {0.75 * duration, {origin, roll(0.0f),          unit}},
        {1.00 * duration, {origin, roll(-swingDegrees), unit}},
    });
    return {std::move(track), duration, true, 1.0};
}

// vehículo que avanza, se detiene y regresa
AnimationDescriptor makeEvShuttleAnimation(double duration, float travelMeters) {
    const glm::vec3 unit{1.0f, 1.0f, 1.0f};
    KeyframeTrack track({
        {0.00 * duration, {{0.0f, 0.0f, 0.0f},                    yawQuat(0.0f),  unit}},
        {0.22 * duration, {{-0.35f, 0.0f, 0.45f * travelMeters},  yawQuat(-8.0f), unit}},
        {0.42 * duration, {{0.0f, 0.0f, travelMeters},            yawQuat(0.0f),  unit}},
        {0.65 * duration, {{0.0f, 0.0f, travelMeters},            yawQuat(0.0f),  unit}},
        {0.85 * duration, {{0.25f, 0.0f, 0.40f * travelMeters},   yawQuat(6.0f),  unit}},
        {1.00 * duration, {{0.0f, 0.0f, 0.0f},                    yawQuat(0.0f),  unit}},
    });
    return {std::move(track), duration, true, 1.0};
}

double wavePhase(double seconds, double frequencyHz) {
    if (frequencyHz == 0.0) return 0.0;
    return std::fmod(seconds * frequencyHz, 1.0) * glm::two_pi<double>();
}

// veleta que oscila con el viento
AnimationDescriptor makeWeathervaneAnimation(float amplitudeDeg, double frequencyHz) {
    ProceduralAnimation motion = [amplitudeDeg, frequencyHz](double seconds) {
        const double phase = wavePhase(seconds, frequencyHz);
        const float wave = static_cast<float>(0.72 * std::sin(phase) + 0.28 * std::sin(2.0 * phase + 0.6));
        Pose pose;
        pose.orientation = yawQuat(amplitudeDeg * wave);
        return pose;
    };
    return {std::move(motion), 8.0, true, 1.0};
}

// indicador de carga que sube y baja
AnimationDescriptor makeChargeIndicatorAnimation(float amplitudeMeters, double frequencyHz) {
    ProceduralAnimation motion = [amplitudeMeters, frequencyHz](double seconds) {
        const double phase = wavePhase(seconds, frequencyHz);
        Pose pose;
        const double wave = 0.85 * std::sin(phase) + 0.15 * std::sin(3.0 * phase);
        pose.position.y = amplitudeMeters * static_cast<float>(wave);
        return pose;
    };
    return {std::move(motion), 8.0, true, 1.0};
}

class EcoEnergyModule final : public IModule {
public:
    explicit EcoEnergyModule(ecoenergy::Settings settings) : settings_(std::move(settings)) {}

    std::string_view id() const noexcept override { return "ecoenergy"; }

    void initialize(ModuleServices& services) override {
        services_ = &services;
        services_->log("ecoenergy");
        services_->log(services_->content() ? "initialize content" : "initialize stub");
        services_->setClearColor(settings_.clearColor);
        auto* content = services_->content();
        if (!content) return;
        const auto entities = makeEcoEnergyInstances(settings_);
        content->configureLighting(settings_.lighting);
        for (const auto& entity : entities) content->add(entity);
        for (const auto& entity : shared::instances("ecoenergy")) content->add(entity);
    }

    void update(double) override {}

    void shutdown() noexcept override {
        if (!services_) return;
        services_->log("ecoenergy");
        services_->log(services_->content() ? "shutdown content" : "shutdown stub");
        services_ = nullptr;
    }

private:
    ecoenergy::Settings settings_;
    ModuleServices* services_ = nullptr;
};
}

std::string ecoEnergyModelId(ecoenergy::Model model) {
    const int number = static_cast<int>(model);
    if (number < 1 || number > 28) throw std::invalid_argument("Invalid EcoEnergy model number");
    return std::string("ENERGY-") + (number < 10 ? "00" : "0") + std::to_string(number);
}

std::vector<EntityDescriptor> makeEcoEnergyInstances(const ecoenergy::Settings& settings) {
    validate(settings);
    using ecoenergy::Model;
    std::vector<EntityDescriptor> result;
    result.reserve(60);

    // agrega una copia con orientación completa (cuaternión)
    const auto addPose = [&](Model model, const std::string& suffix, glm::vec3 position,
                             glm::quat orientation,
                             glm::vec3 scale = glm::vec3(1.0f),
                             std::optional<VisualProperties> visual = std::nullopt,
                             bool interactive = notSelectable,
                             std::optional<AnimationDescriptor> animation = std::nullopt) {
        const auto index = static_cast<std::size_t>(model) - 1;
        if (!settings.visible.at(index)) return;
        const float extraYaw = settings.modelYawDegrees.at(index);
        if (!std::isfinite(extraYaw)) throw std::invalid_argument("Invalid EcoEnergy model yaw");
        EntityDescriptor entity;
        entity.id = ecoEnergyModelId(model) + "-" + suffix;
        entity.model = "ecoenergy/models/" + ecoEnergyModelId(model) + ".glb";
        entity.transform = {
            position,
            glm::normalize(yawQuat(extraYaw) * orientation),
            scale * settings.modelScale.at(index)
        };
        validatePose(entity.transform);
        entity.visual = std::move(visual);
        if (interactive) entity.interaction = InteractionDescriptor{};
        entity.animation = std::move(animation);
        result.push_back(std::move(entity));
    };
    // agrega una copia girada solo en Y (grados)
    const auto add = [&](Model model, const std::string& suffix, glm::vec3 position, float yaw = 0.0f,
                         glm::vec3 scale = glm::vec3(1.0f),
                         std::optional<VisualProperties> visual = std::nullopt,
                         bool interactive = notSelectable,
                         std::optional<AnimationDescriptor> animation = std::nullopt) {
        if (!std::isfinite(yaw)) throw std::invalid_argument("Invalid EcoEnergy instance yaw");
        addPose(model, suffix, position, yawQuat(yaw), scale, std::move(visual), interactive,
                std::move(animation));
    };
    const glm::vec3 one(1.0f);
    const auto noVisual = std::nullopt;
    const glm::vec3 panelOnMount{0.0f, 0.99f, 0.0f};
    const glm::quat panelTilt = tiltedYaw(settings.solarDemoYaw, settings.solarTiltDegrees);
    const auto& panelVisual = settings.solarPanelVisualOverride;

    // panel solar ajustable (va primero para que el rayo lo seleccione antes)
    addPose(Model::solarPanel, "panel-adjustable", settings.solarDemoPosition + panelOnMount, panelTilt,
            settings.solarPanelScale, panelVisual, selectable);
    add(Model::solarMount, "mount-demo", settings.solarDemoPosition, settings.solarDemoYaw);

    // ---- Plataforma, pórtico y andadores ----
    add(Model::platform, "main", settings.platformPosition);
    add(Model::gateway, "entry", settings.gatewayPosition);
    add(Model::walkway, "entry",    {0.0f, 0.015f, 9.5f});
    add(Model::walkway, "south",    {0.0f, 0.015f, 2.6f});
    add(Model::walkway, "north",    {0.0f, 0.015f, -10.8f});
    add(Model::walkway, "solar",    {-6.8f, 0.015f, -4.0f}, 90.0f);
    add(Model::walkway, "grid",     {6.8f, 0.015f, -4.0f}, 90.0f);
    add(Model::walkway, "mobility", {-6.2f, 0.015f, 4.2f}, 90.0f);
    add(Model::walkway, "storage",  {6.2f, 0.015f, 4.2f}, 90.0f);

    // ---- Zona 1: generación solar ----
    for (std::size_t i = 0; i < settings.solarArrayPositions.size(); ++i) {
        const auto position = settings.solarArrayPositions[i];
        const glm::quat arrayTilt = tiltedYaw(settings.solarArrayYaw, settings.solarTiltDegrees);
        add(Model::solarMount, "mount-" + std::to_string(i), position, settings.solarArrayYaw);
        addPose(Model::solarPanel, "panel-" + std::to_string(i), position + panelOnMount, arrayTilt,
                settings.solarPanelScale, panelVisual);
    }
    add(Model::solarTracker, "tracker-pedestal", settings.trackerPosition, 90.0f);
    auto trackerMotion =
        makeSolarTrackerAnimation(settings.trackerDurationSeconds, settings.trackerSwingDegrees);
    addPose(Model::solarPanel, "tracker-wing", settings.trackerPosition + glm::vec3{0.0f, 1.80f, 0.0f},
            yawQuat(0.0f), settings.solarPanelScale, panelVisual, notSelectable, std::move(trackerMotion));
    add(Model::inverter, "main", settings.inverterPosition, 75.0f);
    add(Model::combinerBox, "west", settings.combinerPositions[0], 45.0f);
    add(Model::combinerBox, "east", settings.combinerPositions[1], 20.0f);

    // ---- Zona 2: aerogenerador principal ----
    const glm::vec3 nacelleHeight{0.0f, 9.0f, 0.0f};
    const glm::vec3 rotorOffset{0.0f, 9.65f, 1.38f};
    const glm::vec3 mainBase = settings.mainTurbinePosition;
    const float mainYaw = settings.mainTurbineYaw;
    const glm::vec3 mainRotorPosition = mainBase + yawQuat(mainYaw) * rotorOffset;
    add(Model::windTower, "main", mainBase, mainYaw);
    add(Model::turbineNacelle, "main", mainBase + nacelleHeight, mainYaw);
    add(Model::windRotor, "rotor-main", mainRotorPosition, mainYaw, one, noVisual, notSelectable,
        spin(settings.mainRotorSpeedDeg, {0.0f, 0.0f, 1.0f}));

    // ---- Zona 2: aerogenerador secundario (más pequeño) ----
    const float smallScale = settings.secondaryTurbineScale;
    const glm::vec3 smallScaleVector(smallScale);
    const glm::vec3 smallBase = settings.secondaryTurbinePosition;
    const float smallYaw = settings.secondaryTurbineYaw;
    const glm::vec3 smallNacellePosition = smallBase + glm::vec3{0.0f, 9.0f * smallScale, 0.0f};
    const glm::vec3 smallRotorPosition = smallBase + yawQuat(smallYaw) * (rotorOffset * smallScale);
    add(Model::windTower, "secondary", smallBase, smallYaw, smallScaleVector);
    add(Model::turbineNacelle, "secondary", smallNacellePosition, smallYaw, smallScaleVector);
    add(Model::windRotor, "rotor-secondary", smallRotorPosition, smallYaw, smallScaleVector, noVisual,
        notSelectable, spin(settings.secondaryRotorSpeedDeg, {0.0f, 0.0f, 1.0f}));

    // ---- Zona 2: estaciones de demostración ----
    const glm::vec3 onMast{0.0f, 1.51f, 0.0f};
    const glm::vec3 mastScale{0.50f, 0.88f, 0.50f};
    add(Model::bladeSection, "blade-demo", settings.bladeDemoPosition, 15.0f, one, noVisual, selectable);
    add(Model::solarTracker, "fan-pedestal", settings.demoFanPosition, 0.0f, {0.55f, 0.36f, 0.55f});
    add(Model::demoFan, "demo-fan", settings.demoFanPosition + glm::vec3{0.0f, 1.18f, 0.0f}, -15.0f, one,
        noVisual, notSelectable, spin(settings.demoFanSpeedDeg, {0.0f, 0.0f, 1.0f}));
    add(Model::solarTracker, "weathervane-mast", settings.weathervaneMastPosition, 0.0f, mastScale);
    auto vaneMotion =
        makeWeathervaneAnimation(settings.weathervaneAmplitudeDeg, settings.weathervaneFrequencyHz);
    add(Model::weathervane, "weathervane", settings.weathervaneMastPosition + onMast, 0.0f, one, noVisual,
        notSelectable, std::move(vaneMotion));
    add(Model::solarTracker, "anemometer-mast", settings.anemometerMastPosition, 0.0f, mastScale);
    add(Model::anemometer, "anemometer-main", settings.anemometerMastPosition + onMast, 0.0f, one, noVisual,
        notSelectable, spin(settings.anemometerSpeedDeg, {0.0f, 1.0f, 0.0f}));
    add(Model::cutawayGenerator, "cutaway", settings.generatorPosition, -25.0f);
    add(Model::generatorCoil, "generator-coil", settings.generatorCoilPosition, -25.0f, one, noVisual,
        selectable);

    // ---- Zona 3: transformación y distribución ----
    add(Model::transformer, "main", settings.transformerPosition, -75.0f);
    add(Model::distributionPole, "north", settings.polePositions[0], -90.0f);
    add(Model::distributionPole, "south", settings.polePositions[1], -90.0f);
    add(Model::cableTray, "solar",   {-12.8f, 0.01f, -1.8f});
    add(Model::cableTray, "wind",    {12.2f, 0.01f, -9.2f});
    add(Model::cableTray, "storage", {11.8f, 0.01f, 1.4f});
    add(Model::cableTray, "cross",   {0.0f, 0.005f, -6.2f}, 90.0f);
    add(Model::smartMeter, "generation", settings.smartMeterPositions[0], 30.0f);
    add(Model::smartMeter, "consumption", settings.smartMeterPositions[1], -30.0f);

    // ---- Zona 4: baterías ----
    const float bessYaw = settings.mainBessYaw;
    const glm::vec3 indicatorPosition =
        settings.mainBessPosition + yawQuat(bessYaw) * glm::vec3{0.54f, 1.12f, 0.31f};
    add(Model::batteryCabinet, "bess-main", settings.mainBessPosition, bessYaw);
    auto chargeMotion = makeChargeIndicatorAnimation(settings.chargeIndicatorAmplitudeMeters,
                                                     settings.chargeIndicatorFrequencyHz);
    add(Model::chargeIndicator, "charge-indicator", indicatorPosition, bessYaw, one, noVisual, notSelectable,
        std::move(chargeMotion));
    add(Model::batteryCabinet, "bess-secondary", settings.secondaryBessPosition, -15.0f);
    add(Model::batteryModule, "battery-module", settings.interactiveBatteryPosition, 15.0f, one, noVisual,
        selectable);
    add(Model::batteryModule, "battery-spare", settings.spareBatteryPosition, -8.0f);

    // ---- Zona 5: movilidad eléctrica ----
    add(Model::chargingStation, "charger-lane", settings.chargerPositions[0], 0.0f);
    add(Model::chargingStation, "charger-visitor", settings.chargerPositions[1], 0.0f);
    auto shuttleMotion =
        makeEvShuttleAnimation(settings.evShuttleDurationSeconds, settings.evShuttleTravelMeters);
    add(Model::electricVehicle, "ev-shuttle", settings.shuttleEvPosition, settings.shuttleEvYaw, one,
        noVisual, notSelectable, std::move(shuttleMotion));
    add(Model::electricVehicle, "ev-interactive", settings.interactiveEvPosition, settings.interactiveEvYaw,
        one, noVisual, selectable);
    add(Model::chargeConnector, "charge-connector", settings.chargeConnectorPosition, 20.0f, one, noVisual,
        selectable);

    // ---- Paneles informativos ----
    const std::array<const char*, 4> panelNames{{"solar", "wind", "storage", "mobility"}};
    for (std::size_t i = 0; i < panelNames.size(); ++i)
        add(Model::infoPanel, panelNames[i], settings.infoPanelPositions[i], settings.infoPanelYaws[i]);

    return result;
}

std::unique_ptr<IModule> makeEcoEnergy(const ecoenergy::Settings& settings) {
    return std::make_unique<EcoEnergyModule>(settings);
}

std::unique_ptr<IModule> makeEcoEnergy() {
    return makeEcoEnergy(ecoenergy::settings);
}
}
