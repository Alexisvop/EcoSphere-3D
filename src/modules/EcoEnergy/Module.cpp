#include "EcoEnergy.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
void validate(const ecoenergy::Settings& s) {
    if (!finiteVector(s.clearColor) || glm::any(glm::lessThan(s.clearColor, glm::vec3(0.0f))) ||
        glm::any(glm::greaterThan(s.clearColor, glm::vec3(1.0f))))
        throw std::invalid_argument("Invalid EcoEnergy clear color");
    if (!finiteVector(s.solarPanelScale) || glm::any(glm::lessThanEqual(s.solarPanelScale, glm::vec3(0.01f))) ||
        !std::isfinite(s.solarTiltDegrees) || std::abs(s.solarTiltDegrees) > 85.0f ||
        !std::isfinite(s.secondaryTurbineScale) || s.secondaryTurbineScale <= 0.1f || s.secondaryTurbineScale > 4.0f)
        throw std::invalid_argument("Invalid EcoEnergy scale or tilt parameter");
    for (float speed : {s.mainRotorSpeedDeg, s.secondaryRotorSpeedDeg, s.anemometerSpeedDeg, s.demoFanSpeedDeg})
        if (!std::isfinite(speed) || std::abs(speed) > 1440.0f)
            throw std::invalid_argument("Invalid EcoEnergy simple animation speed");
    if (!std::isfinite(s.trackerDurationSeconds) || s.trackerDurationSeconds <= 0.1 ||
        !std::isfinite(s.evShuttleDurationSeconds) || s.evShuttleDurationSeconds <= 0.1 ||
        !std::isfinite(s.trackerSwingDegrees) || std::abs(s.trackerSwingDegrees) > 80.0f ||
        !std::isfinite(s.evShuttleTravelMeters) || s.evShuttleTravelMeters < 0.0f || s.evShuttleTravelMeters > 12.0f)
        throw std::invalid_argument("Invalid EcoEnergy keyframe animation parameter");
    if (!std::isfinite(s.weathervaneAmplitudeDeg) || std::abs(s.weathervaneAmplitudeDeg) > 180.0f ||
        !std::isfinite(s.weathervaneFrequencyHz) || s.weathervaneFrequencyHz < 0.0 || s.weathervaneFrequencyHz > 20.0 ||
        !std::isfinite(s.chargeIndicatorAmplitudeMeters) || std::abs(s.chargeIndicatorAmplitudeMeters) > 0.8f ||
        !std::isfinite(s.chargeIndicatorFrequencyHz) || s.chargeIndicatorFrequencyHz < 0.0 || s.chargeIndicatorFrequencyHz > 20.0)
        throw std::invalid_argument("Invalid EcoEnergy procedural animation parameter");
    validateLighting(s.lighting, glm::vec3{0.0f, 1.7f, 6.0f});
}

glm::quat yawQuat(float degrees) {
    return glm::angleAxis(glm::radians(degrees), glm::vec3{0.0f, 1.0f, 0.0f});
}

glm::quat tiltedYaw(float yawDegrees, float tiltDegrees) {
    return glm::normalize(yawQuat(yawDegrees) * glm::angleAxis(glm::radians(tiltDegrees), glm::vec3{1.0f, 0.0f, 0.0f}));
}

AnimationDescriptor makeSolarTrackerAnimation(double duration, float swingDegrees) {
    const auto roll = [](float deg) {
        return glm::angleAxis(glm::radians(deg), glm::vec3{0.0f, 0.0f, 1.0f});
    };
    KeyframeTrack track({
        {0.00 * duration, {{0.0f, 0.0f, 0.0f}, roll(-swingDegrees), {1.0f, 1.0f, 1.0f}}},
        {0.25 * duration, {{0.0f, 0.0f, 0.0f}, roll(0.0f),          {1.0f, 1.0f, 1.0f}}},
        {0.50 * duration, {{0.0f, 0.0f, 0.0f}, roll(swingDegrees),  {1.0f, 1.0f, 1.0f}}},
        {0.75 * duration, {{0.0f, 0.0f, 0.0f}, roll(0.0f),          {1.0f, 1.0f, 1.0f}}},
        {1.00 * duration, {{0.0f, 0.0f, 0.0f}, roll(-swingDegrees), {1.0f, 1.0f, 1.0f}}},
    });
    return {std::move(track), duration, true, 1.0};
}

AnimationDescriptor makeEvShuttleAnimation(double duration, float travelMeters) {
    KeyframeTrack track({
        {0.00 * duration, {{0.0f, 0.0f, 0.0f},                 yawQuat(0.0f),  {1.0f, 1.0f, 1.0f}}},
        {0.22 * duration, {{-0.35f, 0.0f, 0.45f * travelMeters}, yawQuat(-8.0f), {1.0f, 1.0f, 1.0f}}},
        {0.42 * duration, {{0.0f, 0.0f, travelMeters},         yawQuat(0.0f),  {1.0f, 1.0f, 1.0f}}},
        {0.65 * duration, {{0.0f, 0.0f, travelMeters},         yawQuat(0.0f),  {1.0f, 1.0f, 1.0f}}},
        {0.85 * duration, {{0.25f, 0.0f, 0.40f * travelMeters}, yawQuat(6.0f),  {1.0f, 1.0f, 1.0f}}},
        {1.00 * duration, {{0.0f, 0.0f, 0.0f},                 yawQuat(0.0f),  {1.0f, 1.0f, 1.0f}}},
    });
    return {std::move(track), duration, true, 1.0};
}

AnimationDescriptor makeWeathervaneAnimation(float amplitudeDeg, double frequencyHz) {
    ProceduralAnimation fn = [amplitudeDeg, frequencyHz](double seconds) {
        Pose p;
        const double phase = frequencyHz == 0.0 ? 0.0 : std::fmod(seconds * frequencyHz, 1.0) * glm::two_pi<double>();
        const float wave = static_cast<float>(0.72 * std::sin(phase) + 0.28 * std::sin(2.0 * phase + 0.6));
        p.orientation = yawQuat(amplitudeDeg * wave);
        return p;
    };
    return {std::move(fn), 8.0, true, 1.0};
}

AnimationDescriptor makeChargeIndicatorAnimation(float amplitudeMeters, double frequencyHz) {
    ProceduralAnimation fn = [amplitudeMeters, frequencyHz](double seconds) {
        Pose p;
        const double phase = frequencyHz == 0.0 ? 0.0 : std::fmod(seconds * frequencyHz, 1.0) * glm::two_pi<double>();
        p.position.y = amplitudeMeters * static_cast<float>(0.85 * std::sin(phase) + 0.15 * std::sin(3.0 * phase));
        return p;
    };
    return {std::move(fn), 8.0, true, 1.0};
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
        if (auto* content = services_->content()) {
            const auto entities = makeEcoEnergyInstances(settings_);
            content->configureLighting(settings_.lighting);
            for (const auto& entity : entities) content->add(entity);
            for (const auto& entity : shared::instances("ecoenergy")) content->add(entity);
        }
    }
    void update(double) override {}
    void shutdown() noexcept override {
        if (services_) {
            services_->log("ecoenergy");
            services_->log(services_->content() ? "shutdown content" : "shutdown stub");
            services_ = nullptr;
        }
    }
private:
    ecoenergy::Settings settings_;
    ModuleServices* services_ = nullptr;
};
}

std::string ecoEnergyModelId(ecoenergy::Model model) {
    const int n = static_cast<int>(model);
    if (n < 1 || n > 28) throw std::invalid_argument("Invalid EcoEnergy model number");
    return std::string("ENERGY-") + (n < 10 ? "00" : "0") + std::to_string(n);
}

std::vector<EntityDescriptor> makeEcoEnergyInstances(const ecoenergy::Settings& s) {
    validate(s);
    using ecoenergy::Model;
    std::vector<EntityDescriptor> result;
    result.reserve(60);

    const auto addPose = [&](Model model, const std::string& suffix, glm::vec3 position, glm::quat orientation,
                             glm::vec3 scale = glm::vec3(1.0f),
                             std::optional<VisualProperties> visual = std::nullopt,
                             bool interactive = false,
                             std::optional<AnimationDescriptor> animation = std::nullopt) {
        const auto index = static_cast<std::size_t>(model) - 1;
        if (!s.visible.at(index)) return;
        const float extraYaw = s.modelYawDegrees.at(index);
        if (!std::isfinite(extraYaw)) throw std::invalid_argument("Invalid EcoEnergy model yaw");
        EntityDescriptor e;
        e.id = ecoEnergyModelId(model) + "-" + suffix;
        e.model = "ecoenergy/models/" + ecoEnergyModelId(model) + ".glb";
        e.transform = {
            position,
            glm::normalize(yawQuat(extraYaw) * orientation),
            scale * s.modelScale.at(index)
        };
        validatePose(e.transform);
        e.visual = std::move(visual);
        if (interactive) e.interaction = InteractionDescriptor{};
        e.animation = std::move(animation);
        result.push_back(std::move(e));
    };
    const auto add = [&](Model model, const std::string& suffix, glm::vec3 position, float yaw = 0.0f,
                         glm::vec3 scale = glm::vec3(1.0f),
                         std::optional<VisualProperties> visual = std::nullopt,
                         bool interactive = false,
                         std::optional<AnimationDescriptor> animation = std::nullopt) {
        if (!std::isfinite(yaw)) throw std::invalid_argument("Invalid EcoEnergy instance yaw");
        addPose(model, suffix, position, yawQuat(yaw), scale, std::move(visual), interactive, std::move(animation));
    };

    // INT-1: Panel solar ajustable (registrado primero para prioridad de ray-pick frontal).
    addPose(Model::solarPanel, "panel-adjustable", s.solarDemoPosition + glm::vec3{0.0f, 0.99f, 0.0f},
            tiltedYaw(s.solarDemoYaw, s.solarTiltDegrees), s.solarPanelScale, s.solarPanelVisualOverride, true);
    add(Model::solarMount, "mount-demo", s.solarDemoPosition, s.solarDemoYaw);

    // Plataforma, pórtico de acceso y red de senderos educativos.
    add(Model::platform, "main", s.platformPosition);
    add(Model::gateway, "entry", s.gatewayPosition);
    add(Model::walkway, "entry", {0.0f, 0.015f, 9.5f});
    add(Model::walkway, "south", {0.0f, 0.015f, 2.6f});
    add(Model::walkway, "north", {0.0f, 0.015f, -10.8f});
    add(Model::walkway, "solar", {-6.8f, 0.015f, -4.0f}, 90.0f);
    add(Model::walkway, "grid", {6.8f, 0.015f, -4.0f}, 90.0f);
    add(Model::walkway, "mobility", {-6.2f, 0.015f, 4.2f}, 90.0f);
    add(Model::walkway, "storage", {6.2f, 0.015f, 4.2f}, 90.0f);

    // Zona 1: Generación solar fotovoltaica y seguidor keyframe (KF-1).
    for (std::size_t i = 0; i < s.solarArrayPositions.size(); ++i) {
        const auto pos = s.solarArrayPositions[i];
        add(Model::solarMount, "mount-" + std::to_string(i), pos, s.solarArrayYaw);
        addPose(Model::solarPanel, "panel-" + std::to_string(i), pos + glm::vec3{0.0f, 0.99f, 0.0f},
                tiltedYaw(s.solarArrayYaw, s.solarTiltDegrees), s.solarPanelScale, s.solarPanelVisualOverride);
    }
    add(Model::solarTracker, "tracker-pedestal", s.trackerPosition, 90.0f);
    addPose(Model::solarPanel, "tracker-wing", s.trackerPosition + glm::vec3{0.0f, 1.80f, 0.0f},
            yawQuat(0.0f), s.solarPanelScale, s.solarPanelVisualOverride, false,
            makeSolarTrackerAnimation(s.trackerDurationSeconds, s.trackerSwingDegrees));
    add(Model::inverter, "main", s.inverterPosition, 75.0f);
    add(Model::combinerBox, "west", s.combinerPositions[0], 45.0f);
    add(Model::combinerBox, "east", s.combinerPositions[1], 20.0f);

    // Zona 2: Aerogeneración (SIM-1..4, PROC-1) y electromagnetismo (INT-2, INT-4).
    const auto mainYawQ = yawQuat(s.mainTurbineYaw);
    add(Model::windTower, "main", s.mainTurbinePosition, s.mainTurbineYaw);
    add(Model::turbineNacelle, "main", s.mainTurbinePosition + glm::vec3{0.0f, 9.0f, 0.0f}, s.mainTurbineYaw);
    add(Model::windRotor, "rotor-main",
        s.mainTurbinePosition + mainYawQ * glm::vec3{0.0f, 9.65f, 1.38f}, s.mainTurbineYaw, glm::vec3(1.0f),
        std::nullopt, false, AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation, {0.0f, 0.0f, 1.0f}, s.mainRotorSpeedDeg, 0.0}});

    const float secScale = s.secondaryTurbineScale;
    const auto secYawQ = yawQuat(s.secondaryTurbineYaw);
    const glm::vec3 secScaleVec(secScale);
    add(Model::windTower, "secondary", s.secondaryTurbinePosition, s.secondaryTurbineYaw, secScaleVec);
    add(Model::turbineNacelle, "secondary",
        s.secondaryTurbinePosition + glm::vec3{0.0f, 9.0f * secScale, 0.0f}, s.secondaryTurbineYaw, secScaleVec);
    add(Model::windRotor, "rotor-secondary",
        s.secondaryTurbinePosition + secYawQ * (glm::vec3{0.0f, 9.65f, 1.38f} * secScale),
        s.secondaryTurbineYaw, secScaleVec, std::nullopt, false,
        AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation, {0.0f, 0.0f, 1.0f}, s.secondaryRotorSpeedDeg, 0.0}});

    add(Model::bladeSection, "blade-demo", s.bladeDemoPosition, 15.0f, glm::vec3(1.0f), std::nullopt, true);
    add(Model::solarTracker, "fan-pedestal", s.demoFanPosition, 0.0f, {0.55f, 0.36f, 0.55f});
    add(Model::demoFan, "demo-fan", s.demoFanPosition + glm::vec3{0.0f, 1.18f, 0.0f}, -15.0f, glm::vec3(1.0f),
        std::nullopt, false, AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation, {0.0f, 0.0f, 1.0f}, s.demoFanSpeedDeg, 0.0}});
    add(Model::solarTracker, "weathervane-mast", s.weathervaneMastPosition, 0.0f, {0.50f, 0.88f, 0.50f});
    add(Model::weathervane, "weathervane", s.weathervaneMastPosition + glm::vec3{0.0f, 1.51f, 0.0f}, 0.0f, glm::vec3(1.0f),
        std::nullopt, false, makeWeathervaneAnimation(s.weathervaneAmplitudeDeg, s.weathervaneFrequencyHz));
    add(Model::solarTracker, "anemometer-mast", s.anemometerMastPosition, 0.0f, {0.50f, 0.88f, 0.50f});
    add(Model::anemometer, "anemometer-main", s.anemometerMastPosition + glm::vec3{0.0f, 1.51f, 0.0f}, 0.0f, glm::vec3(1.0f),
        std::nullopt, false, AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation, {0.0f, 1.0f, 0.0f}, s.anemometerSpeedDeg, 0.0}});
    add(Model::cutawayGenerator, "cutaway", s.generatorPosition, -25.0f);
    add(Model::generatorCoil, "generator-coil", s.generatorCoilPosition, -25.0f, glm::vec3(1.0f), std::nullopt, true);

    // Zona 3: Transformación, medición inteligente y red de distribución.
    add(Model::transformer, "main", s.transformerPosition, -75.0f);
    add(Model::distributionPole, "north", s.polePositions[0], -90.0f);
    add(Model::distributionPole, "south", s.polePositions[1], -90.0f);
    add(Model::cableTray, "solar", {-12.8f, 0.01f, -1.8f});
    add(Model::cableTray, "wind", {12.2f, 0.01f, -9.2f});
    add(Model::cableTray, "storage", {11.8f, 0.01f, 1.4f});
    add(Model::cableTray, "cross", {0.0f, 0.005f, -6.2f}, 90.0f);
    add(Model::smartMeter, "generation", s.smartMeterPositions[0], 30.0f);
    add(Model::smartMeter, "consumption", s.smartMeterPositions[1], -30.0f);

    // Zona 4: Almacenamiento estacionario en baterías (PROC-2, INT-3).
    const auto bessYawQ = yawQuat(s.mainBessYaw);
    add(Model::batteryCabinet, "bess-main", s.mainBessPosition, s.mainBessYaw);
    add(Model::chargeIndicator, "charge-indicator",
        s.mainBessPosition + bessYawQ * glm::vec3{0.54f, 1.12f, 0.31f}, s.mainBessYaw, glm::vec3(1.0f),
        std::nullopt, false, makeChargeIndicatorAnimation(s.chargeIndicatorAmplitudeMeters, s.chargeIndicatorFrequencyHz));
    add(Model::batteryCabinet, "bess-secondary", s.secondaryBessPosition, -15.0f);
    add(Model::batteryModule, "battery-module", s.interactiveBatteryPosition, 15.0f, glm::vec3(1.0f), std::nullopt, true);
    add(Model::batteryModule, "battery-spare", s.spareBatteryPosition, -8.0f);

    // Zona 5: Movilidad eléctrica y recarga inteligente (KF-2, INT-5, INT-6).
    add(Model::chargingStation, "charger-lane", s.chargerPositions[0], 0.0f);
    add(Model::chargingStation, "charger-visitor", s.chargerPositions[1], 0.0f);
    add(Model::electricVehicle, "ev-shuttle", s.shuttleEvPosition, s.shuttleEvYaw, glm::vec3(1.0f),
        std::nullopt, false, makeEvShuttleAnimation(s.evShuttleDurationSeconds, s.evShuttleTravelMeters));
    add(Model::electricVehicle, "ev-interactive", s.interactiveEvPosition, s.interactiveEvYaw, glm::vec3(1.0f),
        std::nullopt, true);
    add(Model::chargeConnector, "charge-connector", s.chargeConnectorPosition, 20.0f, glm::vec3(1.0f), std::nullopt, true);

    // Paneles informativos por estación temática.
    const std::array<const char*, 4> panelNames{{"solar", "wind", "storage", "mobility"}};
    for (std::size_t i = 0; i < panelNames.size(); ++i)
        add(Model::infoPanel, panelNames[i], s.infoPanelPositions[i], s.infoPanelYaws[i]);

    return result;
}

std::unique_ptr<IModule> makeEcoEnergy(const ecoenergy::Settings& settings) {
    return std::make_unique<EcoEnergyModule>(settings);
}

std::unique_ptr<IModule> makeEcoEnergy() {
    return makeEcoEnergy(ecoenergy::settings);
}
}
