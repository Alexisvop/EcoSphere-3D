#pragma once
#include <ecosphere/sdk/IModule.hpp>
#include <memory>

namespace ecosphere {
class ModuleSession final {
public:
    // Propiedad exclusiva del módulo; services debe sobrevivir a la sesión.
    ModuleSession(std::unique_ptr<IModule> module, ModuleServices& services);
    ~ModuleSession();
    ModuleSession(const ModuleSession&) = delete;
    ModuleSession& operator=(const ModuleSession&) = delete;
    void update(double deltaSeconds);
private:
    std::unique_ptr<ModuleServices> services_;
    std::unique_ptr<IModule> module_;
};
}
