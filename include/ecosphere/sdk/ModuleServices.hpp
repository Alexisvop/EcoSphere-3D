#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string_view>

namespace ecosphere {
class ModuleServices {
public:
    virtual ~ModuleServices() = default;
    // El texto solo es válido durante la llamada; no conservar la vista.
    virtual void log(std::string_view message) noexcept = 0;
    // RGB finito en [0,1]; el servicio modifica estado CPU, no expone OpenGL.
    virtual void setClearColor(glm::vec3 color) noexcept = 0;
    // La sesión abre y posee la capacidad; módulos antiguos no necesitan implementarla.
    virtual std::unique_ptr<ContentServices> openContent(std::string_view) { return {}; }
    // Prestado solo hasta shutdown; el anfitrión original devuelve nullptr.
    virtual ContentServices* content() noexcept { return nullptr; }
};
}
