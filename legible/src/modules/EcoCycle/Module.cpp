#include "EcoCycle.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
namespace {
constexpr bool selectable = true;
constexpr bool notSelectable = false;

glm::quat rotate(float degrees, glm::vec3 axis = {0, 1, 0}) {
    return glm::angleAxis(glm::radians(degrees), axis);
}

void validate(const ecocycle::Settings& settings) {
    validatePose({settings.cameraPosition, rotate(settings.cameraYaw), glm::vec3(1)});
    const glm::vec3 color = settings.clearColor;
    if (!finiteVector(color) || glm::any(glm::lessThan(color, glm::vec3(0))) ||
        glm::any(glm::greaterThan(color, glm::vec3(1))))
        throw std::invalid_argument("EcoCycle clear color");

    for (float speed : {settings.rollerSpeedDeg, settings.magnetSpeedDeg, settings.rotorSpeedDeg,
                        settings.compostSpeedDeg}) {
        if (!std::isfinite(speed) || std::abs(speed) > 1440)
            throw std::invalid_argument("EcoCycle rotation speed");
    }
    for (double duration : {settings.gateDurationSeconds, settings.pressDurationSeconds}) {
        if (!std::isfinite(duration) || duration <= .1) throw std::invalid_argument("EcoCycle duration");
    }
    for (float angle : {settings.gateOpeningDeg, settings.sensorAmplitudeDegrees}) {
        if (!std::isfinite(angle) || angle < 0 || angle > 80) throw std::invalid_argument("EcoCycle angle");
    }
    for (float travel : {settings.pressTravelMeters, settings.feederAmplitudeMeters}) {
        if (!std::isfinite(travel) || travel < 0 || travel > 1)
            throw std::invalid_argument("EcoCycle travel");
    }
    for (double frequency : {settings.feederFrequencyHz, settings.sensorFrequencyHz}) {
        if (!std::isfinite(frequency) || frequency < 0 || frequency > 20)
            throw std::invalid_argument("EcoCycle frequency");
    }
    for (float width : {settings.mainPathWidth, settings.secondaryPathWidth}) {
        if (!std::isfinite(width) || width < 2 || width > 5)
            throw std::invalid_argument("EcoCycle path width");
    }
    if (!std::isfinite(settings.cameraPitch) || std::abs(settings.cameraPitch) > 89)
        throw std::invalid_argument("EcoCycle camera pitch");
    for (std::size_t i = 0; i < settings.positions.size(); ++i) {
        if (!std::isfinite(settings.yawDegrees[i])) throw std::invalid_argument("EcoCycle yaw");
        validatePose({settings.positions[i], rotate(settings.yawDegrees[i]), settings.scale[i]});
    }
    validateLighting(settings.lighting, settings.cameraPosition);
}

// ---- Animaciones ----

AnimationDescriptor simpleRotation(float speed, glm::vec3 axis) {
    return {SimpleAnimation{SimpleMotion::rotation, axis, speed, 0}};
}

// compuerta que se abre y se cierra
AnimationDescriptor gateAnimation(const ecocycle::Settings& settings) {
    const auto duration = settings.gateDurationSeconds;
    Pose open;
    open.orientation = rotate(settings.gateOpeningDeg);
    KeyframeTrack track({
        {0, Pose{}},
        {duration * .25, open},
        {duration * .55, open},
        {duration * .85, Pose{}},
        {duration, Pose{}}
    });
    return {std::move(track), duration};
}

// pistón de la prensa que baja y sube
AnimationDescriptor pressAnimation(const ecocycle::Settings& settings) {
    const auto duration = settings.pressDurationSeconds;
    Pose down;
    down.position.y = -settings.pressTravelMeters;
    KeyframeTrack track({
        {0, Pose{}},
        {duration * .3, down},
        {duration * .6, down},
        {duration * .9, Pose{}},
        {duration, Pose{}}
    });
    return {std::move(track), duration};
}

// alimentador que vibra hacia arriba y abajo
AnimationDescriptor feederAnimation(const ecocycle::Settings& settings) {
    const float amplitude = settings.feederAmplitudeMeters;
    const double frequency = settings.feederFrequencyHz;
    return {ProceduralAnimation{[amplitude, frequency](double seconds) {
        const auto phase = std::fmod(seconds * frequency, 1.0) * glm::two_pi<double>();
        Pose pose;
        pose.position.y = amplitude * static_cast<float>(std::sin(phase));
        return pose;
    }}};
}

// sensor que gira de lado a lado
AnimationDescriptor sensorAnimation(const ecocycle::Settings& settings) {
    const float amplitude = settings.sensorAmplitudeDegrees;
    const double frequency = settings.sensorFrequencyHz;
    return {ProceduralAnimation{[amplitude, frequency](double seconds) {
        const auto phase = std::fmod(seconds * frequency, 1.0) * glm::two_pi<double>();
        Pose pose;
        pose.orientation = rotate(amplitude * static_cast<float>(std::sin(phase)));
        return pose;
    }}};
}

class EcoCycleModule final : public IModule {
public:
    explicit EcoCycleModule(ecocycle::Settings settings) : settings_(std::move(settings)) {}

    std::string_view id() const noexcept override { return "ecocycle"; }

    void initialize(ModuleServices& services) override {
        services_ = &services;
        services.log("ecocycle");
        services.log(services.content() ? "initialize content" : "initialize stub");
        services.setClearColor(settings_.clearColor);
        auto* content = services.content();
        if (!content) return;
        const auto entities = makeEcoCycleInstances(settings_);
        content->configureLighting(settings_.lighting);
        for (const auto& entity : entities) content->add(entity);
        for (const auto& entity : shared::instances("ecocycle")) content->add(entity);
    }

    void update(double) override {}

    void shutdown() noexcept override {
        if (!services_) return;
        services_->log("ecocycle");
        services_->log(services_->content() ? "shutdown content" : "shutdown stub");
        services_ = nullptr;
    }

private:
    ecocycle::Settings settings_;
    ModuleServices* services_ = nullptr;
};
}

std::string ecoCycleModelId(ecocycle::Model model) {
    const auto number = static_cast<int>(model);
    if (number < 1 || number > 24) throw std::invalid_argument("EcoCycle model number");
    return std::string("CYCLE-") + (number < 10 ? "00" : "0") + std::to_string(number);
}

std::vector<EntityDescriptor> makeEcoCycleInstances(const ecocycle::Settings& settings) {
    validate(settings);
    using ecocycle::Model;
    std::vector<EntityDescriptor> result;

    // agrega una copia del modelo con los ajustes de EcoCycleSettings.hpp
    const auto add = [&](Model model, const std::string& suffix, glm::vec3 position,
                         std::optional<AnimationDescriptor> animation = std::nullopt,
                         bool interactive = notSelectable, float yaw = 0, glm::vec3 scale = glm::vec3(1)) {
        const auto index = static_cast<std::size_t>(model) - 1;
        EntityDescriptor entity;
        entity.id = ecoCycleModelId(model) + "-" + suffix;
        entity.model = "ecocycle/models/" + ecoCycleModelId(model) + ".glb";
        entity.transform = {position, rotate(settings.yawDegrees[index] + yaw),
                            settings.scale[index] * scale};
        validatePose(entity.transform);
        entity.animation = std::move(animation);
        if (interactive) entity.interaction = InteractionDescriptor{};
        if (settings.visible[index]) result.push_back(std::move(entity));
    };
    const auto positionOf = [&](Model model) {
        return settings.positions[static_cast<std::size_t>(model) - 1];
    };
    const auto noAnimation = std::nullopt;

    // ---- Los 24 modelos (id, posición, animación, ¿se puede tomar?) ----
    add(Model::platform, "main", positionOf(Model::platform));
    add(Model::gateway, "main", positionOf(Model::gateway));
    add(Model::walkway, "main", positionOf(Model::walkway), noAnimation, notSelectable, 0,
        glm::vec3(settings.mainPathWidth / 3.8f, 1, 1));
    add(Model::kiosk, "main", positionOf(Model::kiosk));
    add(Model::sorting, "main", positionOf(Model::sorting), noAnimation, selectable);
    add(Model::feeder, "main", positionOf(Model::feeder), feederAnimation(settings));
    add(Model::conveyor, "main", positionOf(Model::conveyor));
    add(Model::roller, "main", positionOf(Model::roller), simpleRotation(settings.rollerSpeedDeg, {1, 0, 0}));
    add(Model::sensor, "main", positionOf(Model::sensor), sensorAnimation(settings));
    add(Model::gate, "main", positionOf(Model::gate), gateAnimation(settings));
    add(Model::magnetic, "main", positionOf(Model::magnetic),
        simpleRotation(settings.magnetSpeedDeg, {1, 0, 0}));
    add(Model::press, "main", positionOf(Model::press));
    add(Model::ram, "main", positionOf(Model::ram), pressAnimation(settings));
    add(Model::bale, "main", positionOf(Model::bale), noAnimation, selectable);
    add(Model::shredder, "main", positionOf(Model::shredder));
    add(Model::rotor, "main", positionOf(Model::rotor), simpleRotation(settings.rotorSpeedDeg, {1, 0, 0}));
    add(Model::washer, "main", positionOf(Model::washer), noAnimation, selectable);
    add(Model::extruder, "main", positionOf(Model::extruder));
    add(Model::spool, "main", positionOf(Model::spool), noAnimation, selectable);
    add(Model::glassCrusher, "main", positionOf(Model::glassCrusher));
    add(Model::glassBin, "main", positionOf(Model::glassBin), noAnimation, selectable);
    add(Model::compost, "main", positionOf(Model::compost),
        simpleRotation(settings.compostSpeedDeg, {0, 1, 0}));
    add(Model::workbench, "main", positionOf(Model::workbench), noAnimation, selectable);
    add(Model::chair, "main", positionOf(Model::chair), noAnimation, selectable);

    // ---- Andadores extra: los tres primeros van al centro, los demás son ramales ----
    for (std::size_t i = 0; i < settings.extraPaths.size(); ++i) {
        const bool central = i < 3;
        const float width = central ? settings.mainPathWidth : settings.secondaryPathWidth;
        const float yaw = central ? 0.0f : 90.0f;
        const float length = central ? 1 : 1.8f;
        const std::string name = "path-" + std::to_string(i);
        add(Model::walkway, name, settings.extraPaths[i], noAnimation, notSelectable, yaw,
            {width / 3.8f, 1, length});
    }

    // ---- Copias extra ----
    const auto addCopies = [&](Model model, const auto& positions) {
        for (std::size_t i = 0; i < positions.size(); ++i)
            add(model, "stock-" + std::to_string(i), positions[i]);
    };
    addCopies(Model::kiosk, settings.extraKiosks);
    addCopies(Model::bale, settings.extraBales);
    addCopies(Model::spool, settings.extraSpools);
    addCopies(Model::chair, settings.extraChairs);
    return result;
}

std::unique_ptr<IModule> makeEcoCycle(const ecocycle::Settings& settings) {
    return std::make_unique<EcoCycleModule>(settings);
}

std::unique_ptr<IModule> makeEcoCycle() { return makeEcoCycle(ecocycle::settings); }
}
