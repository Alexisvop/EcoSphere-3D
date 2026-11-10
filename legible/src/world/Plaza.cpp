#include "Plaza.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
void validate(const plaza::Settings& settings) {
    for (float size : {settings.radius, settings.pathWidth, settings.pathLength, settings.treeRingOffset}) {
        if (!std::isfinite(size) || size <= 0)
            throw std::invalid_argument("Plaza dimensions must be finite and positive");
    }
    if (settings.benchCount > settings.benches.size() ||
        settings.smallTreeCount > settings.smallTrees.size() || settings.broadTreeCount > 32 ||
        settings.lampCount > 32 || settings.rockCount > 64 || settings.grassCount > 64)
        throw std::invalid_argument("Plaza instance count exceeds configured capacity");
    if (!finiteVector(settings.cameraStart) || !std::isfinite(settings.cameraYaw) ||
        !std::isfinite(settings.cameraPitch) || std::abs(settings.cameraPitch) >= 89 ||
        settings.windowWidth <= 0 || settings.windowHeight <= 0)
        throw std::invalid_argument("Invalid plaza camera/window");
    validateLighting(settings.lighting, settings.cameraStart);
}

// ángulo para que el objeto mire al centro de la plaza
float towardCenter(glm::vec3 position) {
    return glm::degrees(std::atan2(-position.x, -position.z));
}

// punto sobre un círculo de radio dado
glm::vec3 onCircle(float radius, float angle) {
    return {radius * std::sin(angle), 0, radius * std::cos(angle)};
}

// reparte objetos en los cuatro cuadrantes, entre los senderos
float quadrantAngle(unsigned i, unsigned count) {
    const unsigned slots = (count + 3) / 4;
    return glm::radians((i % 4) * 90 + 15 + 60 * (float(i / 4) + .5f) / slots);
}

class PlazaModule final : public IModule {
public:
    explicit PlazaModule(plaza::Settings settings) : settings_(std::move(settings)) {}

    std::string_view id() const noexcept override { return "hub"; }

    void initialize(ModuleServices& services) override {
        services_ = &services;
        auto* content = services.content();
        if (!content) throw std::runtime_error("Plaza requires the candidate content service");
        const auto entities = makePlazaInstances(settings_);
        content->configureLighting(settings_.lighting);
        for (const auto& entity : entities) content->add(entity);
        for (const auto& entity : shared::instances("hub")) content->add(entity);
        services.log("hub initialize: plaza GLB content registered");
    }

    void update(double) override {}

    void shutdown() noexcept override {
        if (services_) services_->log("hub shutdown: session releases plaza content");
        services_ = nullptr;
    }

private:
    plaza::Settings settings_;
    ModuleServices* services_ = nullptr;
};
}

std::string plazaModelId(plaza::Model model) {
    const int number = static_cast<int>(model);
    if (number < 1 || number > 25) throw std::invalid_argument("Invalid plaza model number");
    return std::string("PLAZA-") + (number < 10 ? "00" : "0") + std::to_string(number);
}

std::vector<EntityDescriptor> makePlazaInstances(const plaza::Settings& settings) {
    validate(settings);
    using plaza::Model;
    std::vector<EntityDescriptor> result;

    // agrega una copia del modelo (tint es un color opcional {r, g, b, a}
    const auto add = [&](Model model, const std::string& name, glm::vec3 position, float yaw = 0,
                         glm::vec3 scale = glm::vec3(1), std::optional<glm::vec4> tint = std::nullopt) {
        const auto index = static_cast<std::size_t>(model) - 1;
        if (!settings.visible.at(index)) return;
        yaw += settings.modelYawDegrees.at(index);
        if (!std::isfinite(yaw)) throw std::invalid_argument("Invalid plaza orientation");
        EntityDescriptor entity;
        entity.id = plazaModelId(model) + "-" + name;
        entity.model = "plaza/models/" + plazaModelId(model) + ".glb";
        // los arcos de agua y reciclaje tienen su propio modelo
        if (model == Model::arch && name != "energy") {
            const std::string variant = name == "water" ? "aquabio" : "ecocycle";
            entity.model = "plaza/models/variants/PLAZA-008-" + variant + ".glb";
        }
        entity.transform = {position, glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0)),
                            scale * settings.modelScale.at(index)};
        validatePose(entity.transform);
        if (tint) {
            entity.visual = VisualProperties{};
            entity.visual->tint = *tint;
            entity.visual->material.unlit = false;
            entity.visual->material.metallic = .45f;
        }
        if (model == Model::kiosk) entity.interaction = InteractionDescriptor{};
        result.push_back(std::move(entity));
    };

    const glm::vec3 center{0, 0, 0};
    const float groundScale = settings.radius / 10.5f;
    const float perimeterScale = (settings.radius + 1.5f) / 12;
    const float access = settings.radius + settings.pathLength - .5f;
    const float pathCenter = settings.radius + settings.pathLength * .5f - .5f;

    // ---- Centro de la plaza ----
    add(Model::pavement, "main", center, 0, {groundScale, 1, groundScale});
    add(Model::perimeter, "verge", center, 0, {perimeterScale, 1, perimeterScale});
    add(Model::pedestal, "center", center);
    add(Model::globe, "sculpture", settings.sculpturePosition);
    add(Model::basin, "fountain", center);
    add(Model::jets, "water", center);

    // ---- Senderos, arcos y plataformas hacia cada zona ----
    const glm::vec3 pathScale{settings.pathWidth / 3.6f, 1, settings.pathLength / 8};
    add(Model::path, "entry",  {0, 0, pathCenter}, 0, pathScale);
    add(Model::path, "energy", {-pathCenter, 0, 0}, 90, pathScale);
    add(Model::path, "water",  {0, 0, -pathCenter}, 0, pathScale);
    add(Model::path, "cycle",  {pathCenter, 0, 0}, 90, pathScale);
    add(Model::arch, "energy", {-access, 0, 0}, 90);
    add(Model::arch, "water",  {0, 0, -access});
    add(Model::arch, "cycle",  {access, 0, 0}, -90);
    add(Model::platform, "energy", {-access, 0, 0}, 90);
    add(Model::platform, "water",  {0, 0, -access});
    add(Model::platform, "cycle",  {access, 0, 0}, -90);

    // ---- Mobiliario ----
    add(Model::pergola, "garden", settings.pergolaPosition);
    add(Model::solarCanopy, "energy", settings.solarPosition);
    add(Model::signpost, "entry", settings.signPosition);
    add(Model::kiosk, "information", settings.kioskPosition);
    add(Model::panel, "water", settings.panels[0]);
    add(Model::panel, "cycle", settings.panels[1], -45);
    for (unsigned i = 0; i < settings.benchCount; ++i) {
        const glm::vec3 position = settings.benches[i];
        add(Model::bench, "bench-" + std::to_string(i), position, towardCenter(position));
    }
    // lámparas en círculo
    for (unsigned i = 0; i < settings.lampCount; ++i) {
        const float angle = glm::radians(22.5f + 360.0f * i / settings.lampCount);
        const glm::vec3 position = onCircle(settings.radius - 1, angle);
        add(Model::lamp, "lamp-" + std::to_string(i), position, towardCenter(position));
    }
    add(Model::bicycleRack, "entry", settings.bicyclePosition);
    for (std::size_t i = 0; i < settings.wasteStations.size(); ++i) {
        const glm::vec3 position = settings.wasteStations[i];
        add(Model::wasteStation, "station-" + std::to_string(i), position, towardCenter(position));
    }
    add(Model::drinkingFountain, "west", settings.drinkingFountains[0]);
    add(Model::drinkingFountain, "east", settings.drinkingFountains[1]);

    // ---- Vegetación ----
    for (std::size_t i = 0; i < settings.planters.size(); ++i) {
        const glm::vec3 position = settings.planters[i];
        const std::string name = "bed-" + std::to_string(i);
        add(Model::planter, name, position);
        add(Model::flowerBush, name + "-left", position + glm::vec3(-.62f, .64f, 0));
        add(Model::flowerBush, name + "-right", position + glm::vec3(.62f, .64f, 0));
    }
    for (unsigned i = 0; i < settings.broadTreeCount; ++i) {
        const float angle = glm::radians(22.5f + 360.0f * i / settings.broadTreeCount);
        const float radius = settings.radius + settings.treeRingOffset;
        add(Model::broadTree, "tree-" + std::to_string(i), onCircle(radius, angle), i * 27.0f);
    }
    for (unsigned i = 0; i < settings.smallTreeCount; ++i)
        add(Model::smallTree, "ornamental-" + std::to_string(i), settings.smallTrees[i], i * 40.0f);
    for (unsigned i = 0; i < settings.grassCount; ++i) {
        const float angle = quadrantAngle(i, settings.grassCount);
        const float radius = settings.radius + 2.4f + (i % 2) * .6f;
        add(Model::grasses, "grass-" + std::to_string(i), onCircle(radius, angle), i * 31.0f);
    }
    for (unsigned i = 0; i < settings.rockCount; ++i) {
        const float angle = quadrantAngle(i, settings.rockCount);
        const float radius = settings.radius + 4;
        add(Model::rocks, "rocks-" + std::to_string(i), onCircle(radius, angle), i * 37.0f);
    }
    return result;
}

std::unique_ptr<IModule> makePlaza(const plaza::Settings& settings) {
    return std::make_unique<PlazaModule>(settings);
}
}
