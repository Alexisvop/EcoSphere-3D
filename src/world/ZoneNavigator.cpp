#include "ZoneNavigator.hpp"
#include "Plaza.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace ecosphere {
std::string_view zoneId(Zone z) {
    switch(z) {
    case Zone::hub: return "hub";
    case Zone::ecoenergy: return "ecoenergy";
    case Zone::aquabio: return "aquabio";
    case Zone::ecocycle: return "ecocycle";
    }
    throw std::invalid_argument("Unknown zone");
}
std::string_view zoneTitle(Zone z) {
    switch(z) {
    case Zone::hub: return "PLAZA CENTRAL";
    case Zone::ecoenergy: return "ECOENERGY";
    case Zone::aquabio: return "AQUABIO";
    case Zone::ecocycle: return "ECOCYCLE";
    }
    throw std::invalid_argument("Unknown zone");
}
Camera ZoneCamera::make() const {
    if (!finiteVector(position) || !std::isfinite(yaw) || !std::isfinite(pitch) || std::abs(pitch)>=89 ||
        !std::isfinite(height) || height<=0 || !std::isfinite(speed) || speed<=0 ||
        (mode!=CameraMode::firstPerson && mode!=CameraMode::free)) throw std::invalid_argument("Invalid zone camera");
    Camera c; c.position=position;c.yawDegrees=yaw;c.pitchDegrees=pitch;c.observerHeight=height;c.speed=speed;c.setMode(mode);
    return c;
}
bool Portal::contains(glm::vec3 p) const {
    return finiteVector(p) && glm::all(glm::lessThanEqual(glm::abs(p-center),halfExtent));
}
namespace {
std::unique_ptr<IModule> makeZone(Zone z) {
    switch(z) {
    case Zone::hub: return makePlaza();
    case Zone::ecoenergy: return makeEcoEnergy();
    case Zone::aquabio: return makeAquaBio();
    case Zone::ecocycle: return makeEcoCycle();
    }
    throw std::invalid_argument("Unknown zone");
}
}
struct ZoneNavigator::ActiveSession final : ModuleServices {
    ContentRegistry& registry;
    glm::vec3 color{0};
    std::unique_ptr<ModuleSession> module;
    explicit ActiveSession(ContentRegistry& r) : registry(r) {}
    void log(std::string_view text) noexcept override { std::clog<<"[zone] "<<text<<'\n'; }
    void setClearColor(glm::vec3 c) noexcept override { color=c; }
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override { return registry.open(owner); }
};
ZoneNavigator::ZoneNavigator(const std::filesystem::path& assets, ZoneSettings settings, Factory factory)
    : settings_(std::move(settings)), factory_(factory ? std::move(factory) : makeZone), registry_(assets) {
    for (const auto& entry:settings_.entries) (void)entry.make();
    for (std::size_t i=0;i<settings_.portals.size();++i) {
        const auto& p=settings_.portals[i];
        if (p.destination!=static_cast<Zone>(i+1) || !finiteVector(p.center) || !finiteVector(p.halfExtent) ||
            glm::any(glm::lessThanEqual(p.halfExtent,glm::vec3(0)))) throw std::invalid_argument("Invalid zone portal");
        (void)p.returnCamera.make();
    }
    session_=std::make_unique<ActiveSession>(registry_);
    auto module=factory_(Zone::hub);
    if (!module || module->id()!=zoneId(Zone::hub)) throw std::invalid_argument("Wrong hub factory");
    session_->module=std::make_unique<ModuleSession>(std::move(module),*session_);
    camera_=settings_.entries[0].make();
}
ZoneNavigator::~ZoneNavigator() = default;
std::optional<Zone> ZoneNavigator::nearby(glm::vec3 position) const {
    if (activeZone_==Zone::hub) for (const auto& portal:settings_.portals)
        if (portal.contains(position)) return portal.destination;
    return std::nullopt;
}
void ZoneNavigator::cancelInput() { enter_=home_=false; inputBlocked_=true; }
bool ZoneNavigator::input(glm::vec3 position,bool enter,bool home,const Prepare& prepare) {
    const bool enterEdge=enter && !enter_, homeEdge=home && !home_;
    enter_=enter; home_=home;
    if (state_!=NavigationState::active) return false;
    if (inputBlocked_) { if (!enter && !home) inputBlocked_=false; return false; }
    if (homeEdge && activeZone_!=Zone::hub) return transition(Zone::hub,prepare);
    const auto target=nearby(position);
    if (enterEdge && target) return transition(*target,prepare);
    return false;
}
bool ZoneNavigator::transition(Zone destination,const Prepare& prepare) {
    state_=NavigationState::preparing;
    inputBlocked_=true; error_.clear();
    try {
        auto next=std::make_unique<ActiveSession>(registry_);
        auto module=factory_(destination);
        if (!module || module->id()!=zoneId(destination)) throw std::invalid_argument("Wrong zone factory");
        next->module=std::make_unique<ModuleSession>(std::move(module),*next);
        Camera camera=settings_.entries.at(static_cast<std::size_t>(destination)).make();
        if (destination==Zone::hub)
            camera=settings_.portals.at(static_cast<std::size_t>(lastInstallation_)-1).returnCamera.make();
        const auto light=registry_.lighting(std::string(zoneId(destination))).value_or(LightingSettings{});
        std::vector<ContentPlacement> candidate;
        for (const auto& p:registry_.placements()) if(p.owner==zoneId(destination)) candidate.push_back(p);
        if (prepare) prepare(candidate,light,camera);
        // Solo a partir de aquí el destino CPU/GPU está listo; shutdown aún tiene servicios válidos.
        session_=std::move(next);
        if (destination!=Zone::hub) lastInstallation_=destination;
        activeZone_=destination;camera_=camera;skipUpdate_=true;
        state_=NavigationState::active;
        return true;
    } catch (const std::exception& e) { error_=e.what(); }
      catch (...) { error_="Unknown zone initialization failure"; }
    state_=NavigationState::active;
    return false;
}
void ZoneNavigator::update(double seconds) {
    validateSeconds(seconds);
    if (state_!=NavigationState::active) return;
    const double delta=skipUpdate_ ? 0 : std::min(seconds,.1);
    skipUpdate_=false;session_->module->update(delta);registry_.update(delta);
}
LightingSettings ZoneNavigator::lighting() const { return registry_.lighting(std::string(zoneId(activeZone_))).value_or(LightingSettings{}); }
glm::vec3 ZoneNavigator::clearColor() const { return session_->color; }
std::vector<ContentPlacement> ZoneNavigator::placements() const {
    std::vector<ContentPlacement> result;
    for (const auto& p:registry_.placements()) if(p.owner==zoneId(activeZone_)) result.push_back(p);
    return result;
}
std::string ZoneNavigator::message(glm::vec3 position) const {
    std::string text=std::string(zoneTitle(activeZone_))+" | ";
    const auto target=nearby(position);
    text+=target ? std::string(zoneTitle(*target))+"\nENTER PARA ENTRAR" :
        activeZone_==Zone::hub ? "ACERCATE A UN ACCESO" : "H VOLVER A LA PLAZA";
    if (!error_.empty()) text+="\nERROR DE CARGA - ZONA CONSERVADA";
    if (activeZone_==Zone::hub) text+="\nWASD MOVER | F1 A PIE | F2 LIBRE";
    else {
        text+=registry_.playing(std::string(zoneId(activeZone_))) ?
            "\nP PAUSA | R REINICIAR" : "\nEN PAUSA | P CONTINUAR | R REINICIAR";
        text+="\nTAB O ESPACIO SELECCIONAR | G TOMAR O SOLTAR\nWASD MOVER | F1 A PIE | F2 LIBRE";
    }
    return text;
}
}
