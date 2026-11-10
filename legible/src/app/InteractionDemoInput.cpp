#include "InteractionDemoInput.hpp"
#include "world/InteractionAnimationDemoSettings.hpp"
#include "content/ContentRegistry.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ecosphere {
namespace {
// true si el objeto es el seleccionado o cuelga de él
bool affectedByScale(const InteractionSystem& system, std::string id, const std::string& selected) {
    for (std::size_t step = 0; step <= system.objects().size(); ++step) {
        if (id == selected) return true;
        const auto& parent = system.object(id).parent;
        if (parent.empty()) return false;
        id = parent;
    }
    throw std::invalid_argument("Interaction hierarchy cycle");
}

// límite para que al encoger nada quede con determinante casi cero
float boundedShrink(const InteractionSystem& system, float requested,
                    const std::function<double(const std::string&)>& meshDeterminant) {
    if (!system.selected()) return requested;
    const auto& selected = system.object(*system.selected());
    constexpr double determinantMargin = 1e-7;
    constexpr double outlineWidth = .025;
    constexpr double poseMargin = 1.1e-4;

    double minimumFactor = 0;
    for (int axis = 0; axis < 3; ++axis) {
        minimumFactor = std::max(minimumFactor, poseMargin / double(selected.manipulation.scale[axis]));
        minimumFactor = std::max(minimumFactor,
            poseMargin / (double(selected.base.scale[axis]) * selected.manipulation.scale[axis]));
    }

    for (const auto& object : system.objects()) {
        if (!affectedByScale(system, object.id, selected.id)) continue;
        const auto size = object.bounds.maximum - object.bounds.minimum;
        // barras del contorno de selección
        const double outline = outlineWidth * outlineWidth * std::max(outlineWidth,
            std::min({double(size.x), double(size.y), double(size.z)}));
        const double mesh = meshDeterminant ? meshDeterminant(object.id) : 1.;
        const double determinant = std::abs(glm::determinant(glm::dmat3(system.world(object.id))))
            * std::min({1., outline, mesh});
        if (!std::isfinite(determinant) || determinant <= 0)
            throw std::invalid_argument("Invalid interaction render determinant");
        minimumFactor = std::max(minimumFactor, std::cbrt(determinantMargin / determinant));
    }
    return std::min(1.f, std::max(requested, std::nextafter(float(minimumFactor), 1.f)));
}

// Tab pasa al siguiente objeto de la lista
void selectNext(InteractionSystem& system) {
    const auto& objects = system.objects();
    auto next = objects.begin();
    if (system.selected()) {
        next = std::find_if(objects.begin(), objects.end(),
                            [&](const auto& object) { return object.id == *system.selected(); });
        if (next != objects.end()) ++next;
        if (next == objects.end()) next = objects.begin();
    }
    if (next != objects.end()) system.select(next->id);
}
}

void logInteractionState(const InteractionAnimationDemo& demo) {
    const auto& system = demo.interaction;
    std::cout << "Interaction state: selected=" << system.selected().value_or("none")
              << " held=" << system.holding() << " playing="<< demo.playing() << " time=" << demo.time();
    if (system.selected()) {
        const auto matrix = system.world(*system.selected());
        std::cout << " world=" << matrix[3].x << ',' << matrix[3].y << ',' << matrix[3].z;
        const auto& manipulation = system.object(*system.selected()).manipulation;
        std::cout << " offset=" << manipulation.position.x << ',' << manipulation.position.y << ','
                  << manipulation.position.z << " scale=" << manipulation.scale.x
                  << " quaternion=" << manipulation.orientation.w << ',' << manipulation.orientation.x << ','
                  << manipulation.orientation.y << ',' << manipulation.orientation.z;
    }
    std::cout << std::endl;
}

void updateObjectInput(InteractionSystem& system, const Camera& camera, const glm::mat4& projection,
                       const std::vector<int>& pressed, const std::function<float(int)>& held, double delta,
                       const std::function<void(int)>& onPressed,
                       const std::function<double(const std::string&)>& meshDeterminant) {
    // rayo desde el centro de la cámara
    const auto ray = cameraRay(camera.view(), projection);
    for (int key : pressed) {
        if (key == GLFW_KEY_SPACE) system.pick(ray);
        if (key == GLFW_KEY_BACKSPACE) system.deselect();
        if (key == GLFW_KEY_TAB) selectNext(system);
        if (key == GLFW_KEY_G) {
            if (system.holding()) {
                system.drop();
            } else {
                system.take(ray, interactionDemo::holdDistance);
            }
        }
        if (onPressed) onPressed(key);
    }

    // teclas sostenidas: J/L, U/O, I/K mueven, Z/X giran, C/V escalan
    const float dt = static_cast<float>(delta);
    const glm::vec3 translation{held(GLFW_KEY_L) - held(GLFW_KEY_J), held(GLFW_KEY_O) - held(GLFW_KEY_U),
                                held(GLFW_KEY_K) - held(GLFW_KEY_I)};
    const float turn = held(GLFW_KEY_X) - held(GLFW_KEY_Z);
    const float scale = held(GLFW_KEY_V) - held(GLFW_KEY_C);
    float factor = std::exp(scale * interactionDemo::scaleSpeed * dt);
    if (scale < 0) factor = boundedShrink(system, factor, meshDeterminant);
    if (translation != glm::vec3(0) || turn != 0 || factor != 1) {
        const float angle = glm::radians(turn * interactionDemo::manipulationRotationSpeed * dt);
        system.manipulate(translation * interactionDemo::translationSpeed * dt,
                          glm::angleAxis(angle, glm::vec3(0, 1, 0)), factor);
    }
    if (system.holding()) system.follow(ray);
}

void updateInteractionInput(InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
                            const std::vector<int>& pressed, const std::function<float(int)>& held,
                            double delta) {
    const auto onPressed = [&](int key) {
        if (key == GLFW_KEY_P) demo.togglePause();
        if (key == GLFW_KEY_R) demo.restart();
    };
    const auto meshDeterminant = [&](const std::string& id) {
        return id == "imported" ? demo.importedMeshDeterminant : 1.;
    };
    updateObjectInput(demo.interaction, camera, projection, pressed, held, delta, onPressed, meshDeterminant);
    demo.synchronize();
    if (!pressed.empty()) logInteractionState(demo);
}

void updateContentInput(ContentRegistry& registry, const std::string& owner, const Camera& camera,
                        const glm::mat4& projection, const std::vector<int>& pressed,
                        const std::function<float(int)>& held, double deltaSeconds) {
    // P pausa, R reinicia las animaciones de la zona
    const auto onPressed = [&](int key) {
        if (key == GLFW_KEY_P) {
            registry.setPlaying(owner, !registry.playing(owner));
            std::cout << "Content animations: " << owner << " playing=" << registry.playing(owner)
                      << std::endl;
        }
        if (key == GLFW_KEY_R) {
            registry.restart(owner);
            std::cout << "Content animations: " << owner << " restart playing=" << registry.playing(owner)
                      << std::endl;
        }
    };
    // determinante más chico entre las piezas del modelo
    const auto meshDeterminant = [&](const std::string& id) {
        for (const auto& placement : registry.placements()) {
            if (placement.owner != owner || placement.id != id) continue;
            double minimum = 1;
            placement.model->forEach([&](std::size_t, const glm::mat4& matrix) {
                minimum = std::min(minimum, std::abs(glm::determinant(glm::dmat3(matrix))));
            });
            return minimum;
        }
        throw std::out_of_range("Selected content model missing");
    };
    updateObjectInput(registry.interactions(owner), camera, projection, pressed, held, deltaSeconds,
                      onPressed, meshDeterminant);
}
}
