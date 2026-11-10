#include "AquaBio.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
namespace {
constexpr bool selectable = true;
constexpr bool notSelectable = false;

glm::quat rotation(float degrees, glm::vec3 axis = {0, 1, 0}) {
    return glm::angleAxis(glm::radians(degrees), axis);
}

void validate(const aquabio::Settings& settings) {
    const glm::vec3 color = settings.clearColor;
    if (!finiteVector(color) || glm::any(glm::lessThan(color, glm::vec3(0))) ||
        glm::any(glm::greaterThan(color, glm::vec3(1))))
        throw std::invalid_argument("Invalid AquaBio clear color");

    for (float speed : {settings.pumpSpeedDeg, settings.scraperSpeedDeg, settings.flowSpeedDeg,
                        settings.cycleSpeedDeg}) {
        if (!std::isfinite(speed) || std::abs(speed) > 1440)
            throw std::invalid_argument("Invalid AquaBio rotation speed");
    }
    for (double duration : {settings.gutterDurationSeconds, settings.cartridgeDurationSeconds}) {
        if (!std::isfinite(duration) || duration <= .1)
            throw std::invalid_argument("Invalid AquaBio keyframe duration");
    }
    for (float swing : {settings.gutterSwingDegrees, settings.reedAmplitudeDegrees}) {
        if (!std::isfinite(swing) || std::abs(swing) > 30)
            throw std::invalid_argument("Invalid AquaBio swing");
    }
    for (float travel : {settings.cartridgeLiftMeters, settings.cartridgeTravelMeters,
                         settings.floatingAmplitudeMeters}) {
        if (!std::isfinite(travel) || travel < 0 || travel > 2)
            throw std::invalid_argument("Invalid AquaBio travel");
    }
    for (double frequency : {settings.floatingFrequencyHz, settings.reedFrequencyHz}) {
        if (!std::isfinite(frequency) || frequency < 0 || frequency > 20)
            throw std::invalid_argument("Invalid AquaBio frequency");
    }
    for (std::size_t i = 0; i < settings.positions.size(); ++i) {
        if (!std::isfinite(settings.yawDegrees[i])) throw std::invalid_argument("Invalid AquaBio yaw");
        validatePose({settings.positions[i], rotation(settings.yawDegrees[i]), settings.scale[i]});
    }
    validateLighting(settings.lighting, {0, 1.7f, 6});
}

// ---- Animaciones ----

AnimationDescriptor simpleRotation(float speed, glm::vec3 axis) {
    return {SimpleAnimation{SimpleMotion::rotation, axis, speed, 0}};
}

// canaleta que se inclina de un lado a otro
AnimationDescriptor gutterAnimation(const aquabio::Settings& settings) {
    const double duration = settings.gutterDurationSeconds;
    const Pose left{{}, rotation(-settings.gutterSwingDegrees, {0, 0, 1}), glm::vec3(1)};
    const Pose right{{}, rotation(settings.gutterSwingDegrees, {0, 0, 1}), glm::vec3(1)};
    KeyframeTrack track({
        {0, left},
        {duration * .25, Pose{}},
        {duration * .5, right},
        {duration * .75, Pose{}},
        {duration, left}
    });
    return {std::move(track), duration};
}

// cartucho que sube, se mueve a un lado y regresa
AnimationDescriptor cartridgeAnimation(const aquabio::Settings& settings) {
    const double duration = settings.cartridgeDurationSeconds;
    Pose lifted;
    lifted.position = {0, settings.cartridgeLiftMeters, 0};
    Pose placed;
    placed.position = {settings.cartridgeTravelMeters, settings.cartridgeLiftMeters, 0};
    placed.orientation = rotation(24);
    KeyframeTrack track({
        {0, Pose{}},
        {duration * .2, lifted},
        {duration * .4, placed},
        {duration * .65, placed},
        {duration * .85, lifted},
        {duration, Pose{}}
    });
    return {std::move(track), duration};
}

// planta flotante que sube y baja
AnimationDescriptor floatingAnimation(const aquabio::Settings& settings) {
    const float amplitude = settings.floatingAmplitudeMeters;
    const double frequency = settings.floatingFrequencyHz;
    ProceduralAnimation motion = [amplitude, frequency](double seconds) {
        const double phase = std::fmod(seconds * frequency, 1.0) * glm::two_pi<double>();
        Pose pose;
        pose.position.y = amplitude * static_cast<float>(std::sin(phase));
        pose.orientation = rotation(2 * static_cast<float>(std::sin(phase + .4)), {0, 0, 1});
        return pose;
    };
    return {std::move(motion)};
}

// juncos que se mecen
AnimationDescriptor reedAnimation(const aquabio::Settings& settings) {
    const float amplitude = settings.reedAmplitudeDegrees;
    const double frequency = settings.reedFrequencyHz;
    ProceduralAnimation motion = [amplitude, frequency](double seconds) {
        const double phase = std::fmod(seconds * frequency, 1.0) * glm::two_pi<double>();
        const float wave = static_cast<float>(.75 * std::sin(phase) + .25 * std::sin(2 * phase + .3));
        Pose pose;
        pose.orientation = rotation(amplitude * wave, {0, 0, 1});
        return pose;
    };
    return {std::move(motion)};
}

class AquaBioModule final : public IModule {
public:
    explicit AquaBioModule(aquabio::Settings settings) : settings_(std::move(settings)) {}

    std::string_view id() const noexcept override { return "aquabio"; }

    void initialize(ModuleServices& services) override {
        services_ = &services;
        services.log("aquabio");
        services.log(services.content() ? "initialize content" : "initialize stub");
        services.setClearColor(settings_.clearColor);
        auto* content = services.content();
        if (!content) return;
        const auto entities = makeAquaBioInstances(settings_);
        content->configureLighting(settings_.lighting);
        for (const auto& entity : entities) content->add(entity);
        for (const auto& entity : shared::instances("aquabio")) content->add(entity);
    }

    void update(double) override {}

    void shutdown() noexcept override {
        if (!services_) return;
        services_->log("aquabio");
        services_->log(services_->content() ? "shutdown content" : "shutdown stub");
        services_ = nullptr;
    }

private:
    aquabio::Settings settings_;
    ModuleServices* services_ = nullptr;
};
}

std::string aquaBioModelId(aquabio::Model model) {
    const int number = static_cast<int>(model);
    if (number < 1 || number > 28) throw std::invalid_argument("Invalid AquaBio model number");
    return std::string("AQUA-") + (number < 10 ? "00" : "0") + std::to_string(number);
}

std::vector<EntityDescriptor> makeAquaBioInstances(const aquabio::Settings& settings) {
    validate(settings);
    using aquabio::Model;
    std::vector<EntityDescriptor> result;

    // agrega una copia del modelo con los ajustes de AquaBioSettings.hpp
    const auto add = [&](Model model, const std::string& suffix, glm::vec3 position,
                         bool interactive = notSelectable,
                         std::optional<AnimationDescriptor> animation = std::nullopt,
                         float yaw = 0, glm::vec3 instanceScale = glm::vec3(1)) {
        const auto index = static_cast<std::size_t>(model) - 1;
        EntityDescriptor entity;
        entity.id = aquaBioModelId(model) + "-" + suffix;
        entity.model = "aquabio/models/" + aquaBioModelId(model) + ".glb";
        entity.transform = {position, rotation(settings.yawDegrees[index] + yaw),
                            settings.scale[index] * instanceScale};
        validatePose(entity.transform);
        if (interactive) entity.interaction = InteractionDescriptor{};
        entity.animation = std::move(animation);
        if (settings.visible[index]) result.push_back(std::move(entity));
    };
    const auto positionOf = [&](Model model) {
        return settings.positions[static_cast<std::size_t>(model) - 1];
    };

    add(Model::impeller, "demo", settings.impellerDemoPosition, selectable);

    // ---- Los 28 modelos (id, posición, ¿se puede tomar?, animación) ----
    add(Model::platform, "main", positionOf(Model::platform));
    add(Model::gateway, "entry", positionOf(Model::gateway));
    add(Model::walkway, "entry", positionOf(Model::walkway), notSelectable, std::nullopt, 0,
        settings.mainWalkwayScale);
    add(Model::rainRoof, "collector", positionOf(Model::rainRoof));
    add(Model::gutter, "adjustable", positionOf(Model::gutter), notSelectable, gutterAnimation(settings));
    add(Model::cistern, "cistern", positionOf(Model::cistern), selectable);
    add(Model::grate, "inlet", positionOf(Model::grate));
    add(Model::pump, "housing", positionOf(Model::pump));
    add(Model::impeller, "pump", positionOf(Model::impeller), notSelectable,
        simpleRotation(settings.pumpSpeedDeg, {0, 0, 1}));
    add(Model::sedimentation, "tank", positionOf(Model::sedimentation));
    add(Model::scraper, "scraper", positionOf(Model::scraper), notSelectable,
        simpleRotation(settings.scraperSpeedDeg, {0, 1, 0}));
    add(Model::sandFilter, "sand", positionOf(Model::sandFilter));
    add(Model::carbonFilter, "carbon", positionOf(Model::carbonFilter), selectable);
    add(Model::cartridge, "extraction", positionOf(Model::cartridge), notSelectable,
        cartridgeAnimation(settings));
    add(Model::uvChamber, "uv", positionOf(Model::uvChamber));
    add(Model::treatedTank, "treated", positionOf(Model::treatedTank));
    add(Model::pipes, "distribution", positionOf(Model::pipes));
    add(Model::valve, "manual", positionOf(Model::valve), selectable);
    add(Model::flowIndicator, "indicator", positionOf(Model::flowIndicator), notSelectable,
        simpleRotation(settings.flowSpeedDeg, {0, 0, 1}));
    add(Model::sampling, "sampling", positionOf(Model::sampling), selectable);
    add(Model::waterCycle, "cycle", positionOf(Model::waterCycle), notSelectable,
        simpleRotation(settings.cycleSpeedDeg, {0, 0, 1}));
    add(Model::cloud, "precipitation", positionOf(Model::cloud));
    add(Model::wetland, "basin", positionOf(Model::wetland));
    add(Model::floatingPlant, "floating", positionOf(Model::floatingPlant), notSelectable,
        floatingAnimation(settings));
    add(Model::reeds, "sway", positionOf(Model::reeds), notSelectable, reedAnimation(settings));
    add(Model::riparianTree, "tree", positionOf(Model::riparianTree));
    add(Model::pollinatorShelter, "shelter", positionOf(Model::pollinatorShelter));
    add(Model::fish, "demo", positionOf(Model::fish), selectable);

    // ---- Copias extra ----
    add(Model::cartridge, "demo", settings.cartridgeDemoPosition, selectable);
    for (std::size_t i = 0; i < settings.extraWalkways.size(); ++i) {
        const std::string name = "path-" + std::to_string(i);
        add(Model::walkway, name, settings.extraWalkways[i], notSelectable, std::nullopt,
            settings.extraWalkwayYaws[i], settings.extraWalkwayScales[i]);
    }
    add(Model::pipes, "branch", settings.extraPipesPosition);
    for (std::size_t i = 0; i < settings.extraFloatingPlants.size(); ++i) {
        const std::string name = "static-" + std::to_string(i);
        add(Model::floatingPlant, name, settings.extraFloatingPlants[i], notSelectable,
            floatingAnimation(settings), static_cast<float>(i) * 47);
    }
    for (std::size_t i = 0; i < settings.extraReeds.size(); ++i) {
        const std::string name = "static-" + std::to_string(i);
        add(Model::reeds, name, settings.extraReeds[i], notSelectable, reedAnimation(settings),
            static_cast<float>(i) * 31);
    }
    for (std::size_t i = 0; i < settings.extraTrees.size(); ++i)
        add(Model::riparianTree, "bank-" + std::to_string(i), settings.extraTrees[i]);
    return result;
}

std::unique_ptr<IModule> makeAquaBio(const aquabio::Settings& settings) {
    return std::make_unique<AquaBioModule>(settings);
}

std::unique_ptr<IModule> makeAquaBio() { return makeAquaBio(aquabio::settings); }
}
