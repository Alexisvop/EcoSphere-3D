#include "InteractionDemoInput.hpp"
#include "world/InteractionAnimationDemoSettings.hpp"
#include "content/ContentRegistry.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ecosphere {
namespace {
bool affectedByScale(const InteractionSystem& system,std::string id,const std::string& selected) {
    for(std::size_t step=0;step<=system.objects().size();++step) {
        if(id==selected) return true;
        const auto& parent=system.object(id).parent;
        if(parent.empty()) return false;
        id=parent;
    }
    throw std::invalid_argument("Interaction hierarchy cycle");
}
float boundedShrink(const InteractionSystem& system,float requested,
    const std::function<double(const std::string&)>& meshDeterminant) {
    if(!system.selected()) return requested;
    const auto& selected=system.object(*system.selected());
    double floor=0;
    // Stay a decade above normalMatrix's 1e-8 cutoff, including the thin selection bars.
    constexpr double determinantMargin=1e-7,outlineWidth=.025,poseMargin=1.1e-4;
    for(int axis=0;axis<3;++axis) {
        floor=std::max(floor,poseMargin/double(selected.manipulation.scale[axis]));
        floor=std::max(floor,poseMargin/(double(selected.base.scale[axis])*selected.manipulation.scale[axis]));
    }
    for(const auto& object:system.objects()) {
        if(!affectedByScale(system,object.id,selected.id)) continue;
        const auto size=object.bounds.maximum-object.bounds.minimum;
        // Same bar dimensions as main.cpp and InteractionAnimationDemo::synchronize.
        const double outline=outlineWidth*outlineWidth*std::max(outlineWidth,
            std::min({double(size.x),double(size.y),double(size.z)}));
        const double mesh=meshDeterminant ? meshDeterminant(object.id) : 1.;
        const double determinant=std::abs(glm::determinant(glm::dmat3(system.world(object.id))))*
            std::min({1.,outline,mesh});
        if(!std::isfinite(determinant) || determinant<=0)
            throw std::invalid_argument("Invalid interaction render determinant");
        floor=std::max(floor,std::cbrt(determinantMargin/determinant));
    }
    // Round toward the valid state; at the floor, repeated C does not accumulate shrinkage.
    return std::min(1.f,std::max(requested,std::nextafter(float(floor),1.f)));
}
}
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
    const std::function<void(int)>& onPressed,
    const std::function<double(const std::string&)>& meshDeterminant) {
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
    float factor=std::exp(scale*interactionDemo::scaleSpeed*dt);
    if(scale<0) factor=boundedShrink(system,factor,meshDeterminant);
    if (translation != glm::vec3(0) || turn != 0 || factor != 1)
        system.manipulate(translation*interactionDemo::translationSpeed*dt,
        glm::angleAxis(glm::radians(turn*interactionDemo::manipulationRotationSpeed*dt),glm::vec3(0,1,0)),
        factor);
    if (system.holding()) system.follow(ray);
}
void updateInteractionInput(InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
    const std::vector<int>& pressed, const std::function<float(int)>& held, double delta) {
    updateObjectInput(demo.interaction,camera,projection,pressed,held,delta,[&](int key) {
        if (key == GLFW_KEY_P) demo.togglePause();
        if (key == GLFW_KEY_R) demo.restart();
    },[&](const std::string& id) { return id=="imported" ? demo.importedMeshDeterminant : 1.; });
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
    },[&](const std::string& id) {
        for(const auto& p:registry.placements()) if(p.owner==owner && p.id==id) {
            double minimum=1;
            p.model->forEach([&](std::size_t,const glm::mat4& matrix) {
                minimum=std::min(minimum,std::abs(glm::determinant(glm::dmat3(matrix))));
            });
            return minimum;
        }
        throw std::out_of_range("Selected content model missing");
    });
}
}
