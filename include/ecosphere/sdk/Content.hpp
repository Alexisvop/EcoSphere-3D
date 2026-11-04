#pragma once
#include <ecosphere/sdk/Animation.hpp>
#include <ecosphere/sdk/Bounds.hpp>
#include <ecosphere/sdk/Lighting.hpp>
#include <ecosphere/sdk/Material.hpp>
#include <optional>
#include <string>
#include <variant>

namespace ecosphere {
// SDK de contenido v1.0: estabilidad de código fuente; datos CPU sin ventana/GPU.
struct VisualProperties {
    glm::vec4 tint{1}; // Multiplica el color original; RGB y alfa lineales en [0,1].
    MaterialSettings material;
};
struct InteractionDescriptor {
    std::optional<Aabb> bounds; // Vacío: calcular desde la geometría y jerarquía GLB.
};
struct AnimationDescriptor {
    std::variant<SimpleAnimation, KeyframeTrack, ProceduralAnimation> source;
    double duration = 8;
    bool repeat = true;
    double speed = 1;
};
struct EntityDescriptor {
    std::string id; // Local a la sesión; letras ASCII, números, guion, punto o guion bajo.
    std::string model; // Ruta GLB relativa a la raíz de assets del anfitrión.
    Pose transform;
    std::optional<VisualProperties> visual; // Vacío: conservar materiales originales.
    std::optional<InteractionDescriptor> interaction;
    std::optional<AnimationDescriptor> animation;
};
class ContentServices {
public:
    virtual ~ContentServices() = default;
    // Registro atómico por entidad. Error: excepción; no queda entidad parcial.
    virtual void add(const EntityDescriptor& entity) = 0;
    virtual void setTransform(const std::string& id, const Pose& transform) = 0;
    virtual void manipulate(const std::string& id, glm::vec3 translation, glm::quat rotation, float scaleFactor) = 0;
    virtual void setPlaying(bool playing) = 0;
    virtual void restart() = 0;
    // Una configuración por instalación: no cambia las luces de otras sesiones.
    virtual void configureLighting(const LightingSettings& lighting) = 0;
};
}
