#pragma once
#include "animation/Animation.hpp"
#include <ecosphere/sdk/Bounds.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ecosphere {
struct Ray { glm::vec3 origin, direction; };

// ndc va de (-1,-1) a (1,1), (0,0) es el centro de la pantalla
Ray cameraRay(const glm::mat4& view, const glm::mat4& projection, glm::vec2 ndc = {0,0});
std::optional<float> intersectAabb(const Ray& ray, const Aabb& bounds, const glm::mat4& world = glm::mat4(1));

struct InteractiveObject {
    std::string id;
    Pose base, animation, manipulation;
    Aabb bounds;
    std::string parent; // vacío si no tiene padre
};

// selección con rayo, mover/rotar/escalar y tomar objetos
class InteractionSystem final {
public:
    void add(InteractiveObject object);
    InteractiveObject& object(const std::string& id);
    const InteractiveObject& object(const std::string& id) const;
    glm::mat4 world(const std::string& id) const;
    const std::vector<InteractiveObject>& objects() const { return objects_; }
    bool select(const std::string& id);
    std::optional<std::string> pick(const Ray& ray);
    void deselect() { selected_.reset(); held_ = false; }
    const std::optional<std::string>& selected() const { return selected_; }
    void manipulate(glm::vec3 translation, glm::quat rotation, float scaleFactor);
    void take(const Ray& ray, float distance);
    void follow(const Ray& ray);
    void drop() { held_ = false; }
    bool holding() const { return held_; }
private:
    std::vector<InteractiveObject> objects_;
    std::optional<std::string> selected_;
    bool held_ = false;
    float holdDistance_ = 0;
};
}
