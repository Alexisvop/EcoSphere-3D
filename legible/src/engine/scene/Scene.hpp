#pragma once
#include "Transform.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace ecosphere {
// nodo del grafo de escena: cada hijo es un cubo de color
class SceneNode final {
public:
    Transform transform;
    glm::vec3 color{1.0f};
    SceneNode() = default;
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    SceneNode& addChild(const Transform& local, glm::vec3 tint) {
        auto node = std::make_unique<SceneNode>();
        node->transform = local;
        node->color = tint;
        node->drawable_ = true;
        children_.push_back(std::move(node));
        return *children_.back();
    }

    // mundo del hijo = mundo del padre * local
    template<class Visitor>
    void forEach(const Visitor& visit, const glm::mat4& parent = glm::mat4(1.0f)) const {
        const glm::mat4 world = parent * transform.matrix();
        if (drawable_) visit(*this, world);
        for (const auto& child : children_) child->forEach(visit, world);
    }
private:
    bool drawable_ = false;
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
