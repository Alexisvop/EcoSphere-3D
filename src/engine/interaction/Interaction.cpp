#include "Interaction.hpp"
#include "scene/Lighting.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ecosphere {
namespace {
void validateRay(const Ray& ray) {
    for (int axis=0; axis<3; ++axis)
        if (!std::isfinite(ray.origin[axis]) || !std::isfinite(ray.direction[axis])) throw std::invalid_argument("Ray must be finite");
    const float length = glm::length(ray.direction);
    if (!std::isfinite(length) || std::abs(length-1) > 0.0001f) throw std::invalid_argument("Ray direction must be normalized");
}
void validateBounds(const Aabb& box) {
    for (int axis=0; axis<3; ++axis)
        if (!std::isfinite(box.minimum[axis]) || !std::isfinite(box.maximum[axis]) || box.minimum[axis] > box.maximum[axis])
            throw std::invalid_argument("AABB requires finite ordered bounds");
}
}
Ray cameraRay(const glm::mat4& view, const glm::mat4& projection, glm::vec2 ndc) {
    if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || glm::any(glm::greaterThan(glm::abs(ndc),glm::vec2(1))))
        throw std::invalid_argument("Ray NDC must be inside viewport");
    const auto vp = projection * view;
    const float determinant = glm::determinant(vp);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f) throw std::invalid_argument("Ray requires invertible view projection");
    const auto inverse = glm::inverse(vp);
    auto near = inverse * glm::vec4(ndc,-1,1), far = inverse * glm::vec4(ndc,1,1);
    if (std::abs(near.w) < 1e-8f || std::abs(far.w) < 1e-8f) throw std::invalid_argument("Ray has invalid homogeneous coordinate");
    const glm::vec3 origin = glm::vec3(near)/near.w;
    Ray ray{origin,glm::normalize(glm::vec3(far)/far.w-origin)};
    validateRay(ray); return ray;
}
std::optional<float> intersectAabb(const Ray& ray, const Aabb& box, const glm::mat4& world) {
    validateRay(ray); validateBounds(box); (void)normalMatrix(world);
    const auto inverse = glm::inverse(world);
    const auto origin = glm::vec3(inverse*glm::vec4(ray.origin,1));
    // No normalizar aquí: t conserva la distancia mundial bajo escala no uniforme.
    const auto direction = glm::vec3(inverse*glm::vec4(ray.direction,0));
    float entry = 0, exit = std::numeric_limits<float>::infinity();
    for (int axis=0; axis<3; ++axis) {
        if (direction[axis] == 0) {
            if (origin[axis] < box.minimum[axis] || origin[axis] > box.maximum[axis]) return std::nullopt;
        } else {
            float a = (box.minimum[axis]-origin[axis])/direction[axis], b = (box.maximum[axis]-origin[axis])/direction[axis];
            if (a>b) std::swap(a,b);
            entry = std::max(entry,a); exit = std::min(exit,b);
            if (entry>exit) return std::nullopt;
        }
    }
    return entry;
}
const InteractiveObject& InteractionSystem::object(const std::string& id) const {
    const auto found = std::find_if(objects_.begin(),objects_.end(),[&](const auto& o) { return o.id == id; });
    if (found == objects_.end()) throw std::out_of_range("Unknown interaction ID: " + id);
    return *found;
}
InteractiveObject& InteractionSystem::object(const std::string& id) {
    return const_cast<InteractiveObject&>(static_cast<const InteractionSystem&>(*this).object(id));
}
void InteractionSystem::add(InteractiveObject o) {
    if (o.id.empty() || std::any_of(objects_.begin(),objects_.end(),[&](const auto& item) { return item.id == o.id; }))
        throw std::invalid_argument("Interaction ID must be nonempty and unique");
    if (!o.parent.empty()) object(o.parent); // Padres anteriores impiden ciclos al registrar.
    validatePose(o.base); validatePose(o.animation); validatePose(o.manipulation); validateBounds(o.bounds);
    objects_.push_back(std::move(o));
}
glm::mat4 InteractionSystem::world(const std::string& id) const {
    glm::mat4 result(1);
    // Recorrido acotado: también detecta modificaciones inválidas del padre por el consumidor.
    auto current = id;
    for (std::size_t i=0; i<=objects_.size(); ++i) {
        const auto& o = object(current);
        // Sumar traslación evita que rotar/escalar arrastre el centro alrededor del padre.
        const Pose local{o.base.position+o.manipulation.position,
            glm::normalize(o.manipulation.orientation)*glm::normalize(o.base.orientation),o.base.scale*o.manipulation.scale};
        result = local.matrix()*o.animation.matrix()*result;
        if (o.parent.empty()) { (void)normalMatrix(result); return result; }
        current = o.parent;
    }
    throw std::invalid_argument("Interaction hierarchy cycle");
}
bool InteractionSystem::select(const std::string& id) {
    deselect();
    const auto found = std::find_if(objects_.begin(),objects_.end(),[&](const auto& o) { return o.id == id; });
    if (found == objects_.end()) return false;
    selected_ = id; return true;
}
std::optional<std::string> InteractionSystem::pick(const Ray& ray) {
    validateRay(ray);
    std::optional<std::string> best;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& o : objects_) {
        const auto distance = intersectAabb(ray,o.bounds,world(o.id));
        if (distance && *distance < nearest) { nearest = *distance; best = o.id; }
    }
    deselect(); selected_ = best; return best;
}
void InteractionSystem::manipulate(glm::vec3 translation, glm::quat rotation, float scaleFactor) {
    Pose delta{translation,rotation,glm::vec3(scaleFactor)}; validatePose(delta);
    if (!selected_) return;
    auto& m = object(*selected_).manipulation;
    Pose next{m.position+translation,glm::normalize(rotation)*glm::normalize(m.orientation),m.scale*scaleFactor};
    validatePose(next); m = next;
}
void InteractionSystem::take(const Ray& ray, float distance) {
    validateRay(ray);
    if (!std::isfinite(distance) || distance <= 0) throw std::invalid_argument("Hold distance must be positive");
    if (!selected_) return;
    holdDistance_ = distance; held_ = true;
    try { follow(ray); } catch (...) { held_ = false; throw; }
}
void InteractionSystem::follow(const Ray& ray) {
    validateRay(ray);
    if (!selected_ || !held_) return;
    auto& o = object(*selected_);
    const auto parentWorld = o.parent.empty() ? glm::mat4(1) : world(o.parent);
    (void)normalMatrix(parentWorld);
    const auto inverse = glm::inverse(parentWorld);
    const auto current = world(o.id)*glm::vec4(0,0,0,1);
    const auto desired = glm::vec4(ray.origin+ray.direction*holdDistance_,1);
    Pose next = o.manipulation;
    next.position += glm::vec3(inverse*(desired-current));
    validatePose(next); o.manipulation = next;
}
}
