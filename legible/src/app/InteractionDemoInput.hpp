#pragma once
#include "world/InteractionAnimationDemo.hpp"
#include "camera/Camera.hpp"
#include <functional>
#include <vector>

namespace ecosphere {
class ContentRegistry;

// seleccionar y mover objetos con el teclado
void updateObjectInput(InteractionSystem& system, const Camera& camera, const glm::mat4& projection,
                       const std::vector<int>& pressed, const std::function<float(int)>& held,
                       double deltaSeconds, const std::function<void(int)>& onPressed = {},
                       const std::function<double(const std::string&)>& meshDeterminant = {});
void updateInteractionInput(InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
                            const std::vector<int>& pressed, const std::function<float(int)>& held,
                            double deltaSeconds);
void logInteractionState(const InteractionAnimationDemo& demo);
void updateContentInput(ContentRegistry& registry, const std::string& owner, const Camera& camera,
                        const glm::mat4& projection, const std::vector<int>& pressed,
                        const std::function<float(int)>& held, double deltaSeconds);
}
