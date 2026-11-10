#include "Interaction.hpp"
#include "scene/Lighting.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ecosphere {
namespace {
void validateRay(const Ray& ray) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(ray.origin[axis]) || !std::isfinite(ray.direction[axis])) {
            throw std::invalid_argument("Ray must be finite");
        }
    }
    const float length = glm::length(ray.direction);
    if (!std::isfinite(length) || std::abs(length - 1) > 0.0001f) {
        throw std::invalid_argument("Ray direction must be normalized");
    }
}

void validateBounds(const Aabb& box) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(box.minimum[axis]) || !std::isfinite(box.maximum[axis]) ||
            box.minimum[axis] > box.maximum[axis]) {
            throw std::invalid_argument("AABB requires finite ordered bounds");
        }
    }
}
}

// ---- Rayo y AABB ----

// rayo desde la cámara: se deshace la proyección de un punto en NDC
Ray cameraRay(const glm::mat4& view, const glm::mat4& projection, glm::vec2 ndc) {
    const bool outside = glm::any(glm::greaterThan(glm::abs(ndc), glm::vec2(1)));
    if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || outside) {
        throw std::invalid_argument("Ray NDC must be inside viewport");
    }
    const auto viewProjection = projection * view;
    const float determinant = glm::determinant(viewProjection);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f) {
        throw std::invalid_argument("Ray requires invertible view projection");
    }
    const auto inverse = glm::inverse(viewProjection);
    // z = -1 es el plano cercano y z = 1 el lejano
    const auto nearPoint = inverse * glm::vec4(ndc, -1, 1);
    const auto farPoint = inverse * glm::vec4(ndc, 1, 1);
    if (std::abs(nearPoint.w) < 1e-8f || std::abs(farPoint.w) < 1e-8f) {
        throw std::invalid_argument("Ray has invalid homogeneous coordinate");
    }
    const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
    Ray ray{origin, glm::normalize(glm::vec3(farPoint) / farPoint.w - origin)};
    validateRay(ray);
    return ray;
}

// método de slabs: el rayo pasa a espacio local y se recorta eje por eje
std::optional<float> intersectAabb(const Ray& ray, const Aabb& box, const glm::mat4& world) {
    validateRay(ray);
    validateBounds(box);
    (void)normalMatrix(world);
    const auto inverse = glm::inverse(world);
    const auto origin = glm::vec3(inverse * glm::vec4(ray.origin, 1));
    // la dirección no se normaliza para que t siga siendo distancia en mundo
    const auto direction = glm::vec3(inverse * glm::vec4(ray.direction, 0));
    float entry = 0;
    float exit = std::numeric_limits<float>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0) {
            // rayo paralelo a este slab
            if (origin[axis] < box.minimum[axis] || origin[axis] > box.maximum[axis]) return std::nullopt;
            continue;
        }
        float tMin = (box.minimum[axis] - origin[axis]) / direction[axis];
        float tMax = (box.maximum[axis] - origin[axis]) / direction[axis];
        if (tMin > tMax) std::swap(tMin, tMax);
        entry = std::max(entry, tMin);
        exit = std::min(exit, tMax);
        if (entry > exit) return std::nullopt;
    }
    return entry;
}

// ---- Objetos interactivos ----

const InteractiveObject& InteractionSystem::object(const std::string& id) const {
    const auto found = std::find_if(objects_.begin(), objects_.end(),
                                    [&](const auto& item) { return item.id == id; });
    if (found == objects_.end()) throw std::out_of_range("Unknown interaction ID: " + id);
    return *found;
}

InteractiveObject& InteractionSystem::object(const std::string& id) {
    return const_cast<InteractiveObject&>(static_cast<const InteractionSystem&>(*this).object(id));
}

void InteractionSystem::add(InteractiveObject newObject) {
    const bool duplicated = std::any_of(objects_.begin(), objects_.end(),
                                        [&](const auto& item) { return item.id == newObject.id; });
    if (newObject.id.empty() || duplicated) {
        throw std::invalid_argument("Interaction ID must be nonempty and unique");
    }
    // el padre ya debe estar registrado
    if (!newObject.parent.empty()) object(newObject.parent);
    validatePose(newObject.base);
    validatePose(newObject.animation);
    validatePose(newObject.manipulation);
    validateBounds(newObject.bounds);
    objects_.push_back(std::move(newObject));
}

// matriz de mundo subiendo por los padres
glm::mat4 InteractionSystem::world(const std::string& id) const {
    glm::mat4 result(1);
    auto current = id;
    for (std::size_t step = 0; step <= objects_.size(); ++step) {
        const auto& item = object(current);
        const auto& base = item.base;
        const auto& manipulation = item.manipulation;
        // la traslación se suma y la rotación y escala se componen
        const Pose local{base.position + manipulation.position,
                         glm::normalize(manipulation.orientation) * glm::normalize(base.orientation),
                         base.scale * manipulation.scale};
        result = local.matrix() * item.animation.matrix() * result;
        if (item.parent.empty()) {
            (void)normalMatrix(result);
            return result;
        }
        current = item.parent;
    }
    throw std::invalid_argument("Interaction hierarchy cycle");
}

bool InteractionSystem::select(const std::string& id) {
    deselect();
    const auto found = std::find_if(objects_.begin(), objects_.end(),
                                    [&](const auto& item) { return item.id == id; });
    if (found == objects_.end()) return false;
    selected_ = id;
    return true;
}

// selecciona el objeto más cercano que toca el rayo
std::optional<std::string> InteractionSystem::pick(const Ray& ray) {
    validateRay(ray);
    std::optional<std::string> best;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& item : objects_) {
        const auto distance = intersectAabb(ray, item.bounds, world(item.id));
        if (distance && *distance < nearest) {
            nearest = *distance;
            best = item.id;
        }
    }
    deselect();
    selected_ = best;
    return best;
}

// ---- Manipulación ----

void InteractionSystem::manipulate(glm::vec3 translation, glm::quat rotation, float scaleFactor) {
    Pose delta{translation, rotation, glm::vec3(scaleFactor)};
    validatePose(delta);
    if (!selected_) return;
    auto& current = object(*selected_).manipulation;
    Pose next{current.position + translation,
              glm::normalize(rotation) * glm::normalize(current.orientation),
              current.scale * scaleFactor};
    validatePose(next);
    current = next;
}

void InteractionSystem::take(const Ray& ray, float distance) {
    validateRay(ray);
    if (!std::isfinite(distance) || distance <= 0) {
        throw std::invalid_argument("Hold distance must be positive");
    }
    if (!selected_) return;
    holdDistance_ = distance;
    held_ = true;
    try {
        follow(ray);
    } catch (...) {
        held_ = false;
        throw;
    }
}

// mueve el objeto tomado al punto del rayo a holdDistance_
void InteractionSystem::follow(const Ray& ray) {
    validateRay(ray);
    if (!selected_ || !held_) return;
    auto& item = object(*selected_);
    const auto parentWorld = item.parent.empty() ? glm::mat4(1) : world(item.parent);
    (void)normalMatrix(parentWorld);
    const auto inverse = glm::inverse(parentWorld);
    const auto current = world(item.id) * glm::vec4(0, 0, 0, 1);
    const auto desired = glm::vec4(ray.origin + ray.direction * holdDistance_, 1);
    Pose next = item.manipulation;
    next.position += glm::vec3(inverse * (desired - current));
    validatePose(next);
    item.manipulation = next;
}
}
