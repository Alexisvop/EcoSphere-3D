#pragma once
#include "Transform.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace ecosphere {
class SceneNode final {
public:
    Transform transform;
    glm::vec3 color{1.0f};
    SceneNode() = default;
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // Solo se crean hijos nuevos: no hay reparentado, ciclos ni propiedad compartida.
    SceneNode& addChild(const Transform& local, glm::vec3 tint) {
        auto node = std::make_unique<SceneNode>();
        node->transform = local;
        node->color = tint;
        node->drawable_ = true;
        children_.push_back(std::move(node));
        return *children_.back();
    }

    template<class Visitor>
    void forEach(const Visitor& visit, const glm::mat4& parent = glm::mat4(1.0f)) const {
        const glm::mat4 world = parent * transform.matrix();
        if (drawable_) visit(*this, world);
        for (const auto& child : children_) child->forEach(visit, world);
    }
private:
    bool drawable_ = false; // La raíz solo agrupa; los hijos representan cubos.
    std::vector<std::unique_ptr<SceneNode>> children_;
};

class Scene final {
public:
    Scene();
    template<class Visitor>
    void forEach(const Visitor& visit) const { root_.forEach(visit); }
private:
    SceneNode root_;
};
}
