#include "InteractionDemoInput.hpp"
#include "world/InteractionAnimationDemoSettings.hpp"
#include "content/ContentRegistry.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ecosphere {
void logInteractionState(const InteractionAnimationDemo& demo) {
    const auto& system = demo.interaction;
    std::cout << "Interaction state: selected=" << system.selected().value_or("none") << " held=" << system.holding()
              << " playing=" << demo.playing() << " time=" << demo.time();
    if (system.selected()) {
        const auto matrix = system.world(*system.selected());
        std::cout << " world=" << matrix[3].x << ',' << matrix[3].y << ',' << matrix[3].z;
        const auto& m = system.object(*system.selected()).manipulation;
        std::cout << " offset=" << m.position.x << ',' << m.position.y << ',' << m.position.z
                  << " scale=" << m.scale.x << " quaternion=" << m.orientation.w << ',' << m.orientation.x << ',' << m.orientation.y << ',' << m.orientation.z;
    }
    std::cout << std::endl;
}
void updateObjectInput(InteractionSystem& system, const Camera& camera, const glm::mat4& projection,
    const std::vector<int>& pressed, const std::function<float(int)>& held, double delta,
    const std::function<void(int)>& onPressed) {
    const auto ray = cameraRay(camera.view(),projection);
    for (int key : pressed) {
        if (key == GLFW_KEY_SPACE) system.pick(ray);
        if (key == GLFW_KEY_BACKSPACE) system.deselect();
        if (key == GLFW_KEY_TAB) {
            const auto& objects = system.objects();
            auto next = objects.begin();
            if (system.selected()) {
                next = std::find_if(objects.begin(),objects.end(),[&](const auto& o) { return o.id == *system.selected(); });
                if (next != objects.end()) ++next;
                if (next == objects.end()) next = objects.begin();
            }
            if (next != objects.end()) system.select(next->id);
        }
        if (key == GLFW_KEY_G) {
            if (system.holding()) system.drop(); else system.take(ray,interactionDemo::holdDistance);
        }
        if (onPressed) onPressed(key);
    }
    const float dt = static_cast<float>(delta);
    const glm::vec3 translation{held(GLFW_KEY_L)-held(GLFW_KEY_J),held(GLFW_KEY_O)-held(GLFW_KEY_U),held(GLFW_KEY_K)-held(GLFW_KEY_I)};
    const float turn = held(GLFW_KEY_X)-held(GLFW_KEY_Z), scale = held(GLFW_KEY_V)-held(GLFW_KEY_C);
    if (translation != glm::vec3(0) || turn != 0 || scale != 0)
        system.manipulate(translation*interactionDemo::translationSpeed*dt,
        glm::angleAxis(glm::radians(turn*interactionDemo::manipulationRotationSpeed*dt),glm::vec3(0,1,0)),
        std::exp(scale*interactionDemo::scaleSpeed*dt));
    if (system.holding()) system.follow(ray);
}
void updateInteractionInput(InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
    const std::vector<int>& pressed, const std::function<float(int)>& held, double delta) {
    updateObjectInput(demo.interaction,camera,projection,pressed,held,delta,[&](int key) {
        if (key == GLFW_KEY_P) demo.togglePause();
        if (key == GLFW_KEY_R) demo.restart();
    });
    demo.synchronize();
    if (!pressed.empty()) logInteractionState(demo);
}
void updateContentInput(ContentRegistry& registry, const std::string& owner, const Camera& camera,
    const glm::mat4& projection, const std::vector<int>& pressed,
    const std::function<float(int)>& held, double deltaSeconds) {
    updateObjectInput(registry.interactions(owner),camera,projection,pressed,held,deltaSeconds,[&](int key) {
        if (key==GLFW_KEY_P) {
            registry.setPlaying(owner,!registry.playing(owner));
            std::cout<<"Content animations: "<<owner<<" playing="<<registry.playing(owner)<<std::endl;
        }
        if (key==GLFW_KEY_R) {
            registry.restart(owner);
            std::cout<<"Content animations: "<<owner<<" restart playing="<<registry.playing(owner)<<std::endl;
        }
    });
}
}
