#pragma once
#include <string_view>

namespace ecosphere {
class ModuleServices;

class IModule {
public:
    virtual ~IModule() = default;
    // La vista debe permanecer válida durante toda la vida del módulo.
    virtual std::string_view id() const noexcept = 0;
    // Servicios prestados: su propietario debe sobrevivir a ModuleSession.
    virtual void initialize(ModuleServices& services) = 0;
    // Segundos reales, finitos y >= 0; se llama desde el hilo principal.
    virtual void update(double deltaSeconds) = 0;
    // No lanzar; tolerar inicialización parcial y llamadas repetidas.
    virtual void shutdown() noexcept = 0;
};
}
