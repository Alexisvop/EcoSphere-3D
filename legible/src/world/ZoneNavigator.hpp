#pragma once
#include "ZoneSettings.hpp"
#include "content/ContentRegistry.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <functional>

namespace ecosphere {
enum class NavigationState { active, preparing };
// cambia entre la plaza y las tres zonas
class ZoneNavigator final {
public:
    using Factory = std::function<std::unique_ptr<IModule>(Zone)>;
    using Prepare = std::function<void(const std::vector<ContentPlacement>&, const LightingSettings&,
                                       const Camera&)>;
    explicit ZoneNavigator(const std::filesystem::path& assets, ZoneSettings settings = {},
                           Factory factory = {});
    ~ZoneNavigator();
    Zone active() const { return activeZone_; }
    NavigationState state() const { return state_; }
    std::optional<Zone> nearby(glm::vec3 position) const;
    bool input(glm::vec3 position, bool enter, bool home, const Prepare& prepare = {});
    void cancelInput();
    void update(double seconds);
    const Camera& entryCamera() const { return camera_; }
    LightingSettings lighting() const;
    glm::vec3 clearColor() const;
    std::string message(glm::vec3 position) const;
    const std::string& error() const { return error_; }
    std::vector<ContentPlacement> placements() const;
    ContentRegistry& registry() { return registry_; }
private:
    struct ActiveSession;
    bool transition(Zone destination, const Prepare& prepare);
    ZoneSettings settings_;
    Factory factory_;
    ContentRegistry registry_;
    std::unique_ptr<ActiveSession> session_;
    Zone activeZone_ = Zone::hub, lastInstallation_ = Zone::ecoenergy;
    NavigationState state_ = NavigationState::active;
    Camera camera_;
    bool enter_ = false, home_ = false, inputBlocked_ = false, skipUpdate_ = true;
    std::string error_;
};
}
