#include "ZoneNavigator.hpp"
#include "Plaza.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace ecosphere {
namespace {
std::unique_ptr<IModule> makeZone(Zone zone) {
    switch (zone) {
    case Zone::hub: return makePlaza();
    case Zone::ecoenergy: return makeEcoEnergy();
    case Zone::aquabio: return makeAquaBio();
    case Zone::ecocycle: return makeEcoCycle();
    }
    throw std::invalid_argument("Unknown zone");
}
}

std::string_view zoneId(Zone zone) {
    switch (zone) {
    case Zone::hub: return "hub";
    case Zone::ecoenergy: return "ecoenergy";
    case Zone::aquabio: return "aquabio";
    case Zone::ecocycle: return "ecocycle";
    }
    throw std::invalid_argument("Unknown zone");
}

std::string_view zoneTitle(Zone zone) {
    switch (zone) {
    case Zone::hub: return "PLAZA CENTRAL";
    case Zone::ecoenergy: return "ECOENERGY";
    case Zone::aquabio: return "AQUABIO";
    case Zone::ecocycle: return "ECOCYCLE";
    }
    throw std::invalid_argument("Unknown zone");
}

Camera ZoneCamera::make() const {
    const bool validMode = mode == CameraMode::firstPerson || mode == CameraMode::free;
    if (!finiteVector(position) || !std::isfinite(yaw) || !std::isfinite(pitch) || std::abs(pitch) >= 89 ||
        !std::isfinite(height) || height <= 0 || !std::isfinite(speed) || speed <= 0 || !validMode)
        throw std::invalid_argument("Invalid zone camera");
    Camera camera;
    camera.position = position;
    camera.yawDegrees = yaw;
    camera.pitchDegrees = pitch;
    camera.observerHeight = height;
    camera.speed = speed;
    camera.setMode(mode);
    return camera;
}

bool Portal::contains(glm::vec3 position) const {
    return finiteVector(position) && glm::all(glm::lessThanEqual(glm::abs(position - center), halfExtent));
}

// servicios que recibe el módulo de la zona activa
struct ZoneNavigator::ActiveSession final : ModuleServices {
    ContentRegistry& registry;
    glm::vec3 color{0};
    std::unique_ptr<ModuleSession> module;

    explicit ActiveSession(ContentRegistry& contentRegistry) : registry(contentRegistry) {}
    void log(std::string_view text) noexcept override { std::clog << "[zone] " << text << '\n'; }
    void setClearColor(glm::vec3 clearColor) noexcept override { color = clearColor; }
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override {
        return registry.open(owner);
    }
};

ZoneNavigator::ZoneNavigator(const std::filesystem::path& assets, ZoneSettings settings, Factory factory)
    : settings_(std::move(settings)), factory_(factory ? std::move(factory) : makeZone), registry_(assets) {
    for (const auto& entry : settings_.entries) (void)entry.make();
    for (std::size_t i = 0; i < settings_.portals.size(); ++i) {
        const auto& portal = settings_.portals[i];
        if (portal.destination != static_cast<Zone>(i + 1) || !finiteVector(portal.center) ||
            !finiteVector(portal.halfExtent) || glm::any(glm::lessThanEqual(portal.halfExtent, glm::vec3(0))))
            throw std::invalid_argument("Invalid zone portal");
        (void)portal.returnCamera.make();
    }
    // la plaza es la zona inicial
    session_ = std::make_unique<ActiveSession>(registry_);
    auto module = factory_(Zone::hub);
    if (!module || module->id() != zoneId(Zone::hub)) throw std::invalid_argument("Wrong hub factory");
    session_->module = std::make_unique<ModuleSession>(std::move(module), *session_);
    camera_ = settings_.entries[0].make();
}

ZoneNavigator::~ZoneNavigator() = default;

std::optional<Zone> ZoneNavigator::nearby(glm::vec3 position) const {
    if (activeZone_ != Zone::hub) return std::nullopt;
    for (const auto& portal : settings_.portals) {
        if (portal.contains(position)) return portal.destination;
    }
    return std::nullopt;
}

void ZoneNavigator::cancelInput() {
    enter_ = home_ = false;
    inputBlocked_ = true;
}

bool ZoneNavigator::input(glm::vec3 position, bool enter, bool home, const Prepare& prepare) {
    // solo cuenta el momento en que se presiona la tecla
    const bool enterPressed = enter && !enter_;
    const bool homePressed = home && !home_;
    enter_ = enter;
    home_ = home;
    if (state_ != NavigationState::active) return false;
    if (inputBlocked_) {
        if (!enter && !home) inputBlocked_ = false;
        return false;
    }
    if (homePressed && activeZone_ != Zone::hub) return transition(Zone::hub, prepare);
    const auto target = nearby(position);
    if (enterPressed && target) return transition(*target, prepare);
    return false;
}

bool ZoneNavigator::transition(Zone destination, const Prepare& prepare) {
    state_ = NavigationState::preparing;
    inputBlocked_ = true;
    error_.clear();
    try {
        // se prepara la zona nueva sin cerrar la actual
        auto next = std::make_unique<ActiveSession>(registry_);
        auto module = factory_(destination);
        if (!module || module->id() != zoneId(destination)) throw std::invalid_argument("Wrong zone factory");
        next->module = std::make_unique<ModuleSession>(std::move(module), *next);
        Camera camera = settings_.entries.at(static_cast<std::size_t>(destination)).make();
        if (destination == Zone::hub) {
            const auto portalIndex = static_cast<std::size_t>(lastInstallation_) - 1;
            camera = settings_.portals.at(portalIndex).returnCamera.make();
        }
        const auto light = registry_.lighting(std::string(zoneId(destination))).value_or(LightingSettings{});
        std::vector<ContentPlacement> candidate;
        for (const auto& placement : registry_.placements()) {
            if (placement.owner == zoneId(destination)) candidate.push_back(placement);
        }
        if (prepare) prepare(candidate, light, camera);

        // cambio a la zona nueva
        session_ = std::move(next);
        if (destination != Zone::hub) lastInstallation_ = destination;
        activeZone_ = destination;
        camera_ = camera;
        skipUpdate_ = true;
        state_ = NavigationState::active;
        return true;
    } catch (const std::exception& exception) {
        error_ = exception.what();
    } catch (...) {
        error_ = "Unknown zone initialization failure";
    }
    state_ = NavigationState::active;
    return false;
}

void ZoneNavigator::update(double seconds) {
    validateSeconds(seconds);
    if (state_ != NavigationState::active) return;
    const double delta = skipUpdate_ ? 0 : std::min(seconds, .1);
    skipUpdate_ = false;
    session_->module->update(delta);
    registry_.update(delta);
}

LightingSettings ZoneNavigator::lighting() const {
    return registry_.lighting(std::string(zoneId(activeZone_))).value_or(LightingSettings{});
}

glm::vec3 ZoneNavigator::clearColor() const { return session_->color; }

std::vector<ContentPlacement> ZoneNavigator::placements() const {
    std::vector<ContentPlacement> result;
    for (const auto& placement : registry_.placements()) {
        if (placement.owner == zoneId(activeZone_)) result.push_back(placement);
    }
    return result;
}

// texto del HUD con la zona actual y las teclas
std::string ZoneNavigator::message(glm::vec3 position) const {
    std::string text = std::string(zoneTitle(activeZone_)) + " | ";
    const auto target = nearby(position);
    if (target) text += std::string(zoneTitle(*target)) + "\nENTER PARA ENTRAR";
    else if (activeZone_ == Zone::hub) text += "ACERCATE A UN ACCESO";
    else text += "H VOLVER A LA PLAZA";
    if (!error_.empty()) text += "\nERROR DE CARGA - ZONA CONSERVADA";
    if (activeZone_ == Zone::hub) {
        text += "\nWASD MOVER | F1 A PIE | F2 LIBRE";
        return text;
    }
    if (registry_.playing(std::string(zoneId(activeZone_)))) text += "\nP PAUSA | R REINICIAR";
    else text += "\nEN PAUSA | P CONTINUAR | R REINICIAR";
    text += "\nTAB O ESPACIO SELECCIONAR | G TOMAR O SOLTAR\nWASD MOVER | F1 A PIE | F2 LIBRE";
    return text;
}
}
