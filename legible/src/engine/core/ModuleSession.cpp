#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
// servicios que ve un módulo: los del host más su propio contenido
class SessionServices final : public ModuleServices {
public:
    SessionServices(ModuleServices& host, std::string_view owner)
        : host_(host), content_(host.openContent(owner)) {}
    void log(std::string_view message) noexcept override { host_.log(message); }
    void setClearColor(glm::vec3 color) noexcept override { host_.setClearColor(color); }
    ContentServices* content() noexcept override { return content_.get(); }
private:
    ModuleServices& host_;
    std::unique_ptr<ContentServices> content_;
};
}

ModuleSession::ModuleSession(std::unique_ptr<IModule> module, ModuleServices& services)
    : module_(std::move(module)) {
    if (!module_ || module_->id().empty()) {
        throw std::invalid_argument("Module must exist and have a stable nonempty ID");
    }
    try {
        services_ = std::make_unique<SessionServices>(services, module_->id());
        module_->initialize(*services_);
    } catch (...) {
        // si initialize falla se apaga el módulo aquí
        module_->shutdown();
        throw;
    }
}

ModuleSession::~ModuleSession() { module_->shutdown(); }

void ModuleSession::update(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        throw std::invalid_argument("deltaSeconds must be finite and nonnegative");
    }
    module_->update(deltaSeconds);
}
}
