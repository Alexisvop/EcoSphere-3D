#pragma once
#include "Material.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ecosphere {
// un vértice tal como se sube al VBO
struct Vertex {
    glm::vec3 position{0};
    glm::vec3 normal{0};
    glm::vec2 uv{0};
};

struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgba;
};

// filtros y repetición con los números de OpenGL (9729 LINEAR, 10497 REPEAT)
struct TextureData {
    std::size_t image = 0;
    int minFilter = 9729, magFilter = 9729;
    int wrapS = 10497, wrapT = 10497;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    glm::vec4 color{1};
    int texture = -1;
    bool visible = true;
    MaterialSettings material;
};

struct ModelNode {
    std::string name;
    glm::mat4 local{1};
    int parent = -1;
    std::vector<std::size_t> meshes;
    bool visible = true;
};

struct Model {
    std::vector<Mesh> meshes;
    std::vector<ModelNode> nodes;
    std::vector<Image> images;
    std::vector<TextureData> textures;
    std::vector<std::string> warnings;

    // recorre los nodos y llama visit(malla, matriz de mundo) por cada malla visible
    template<class Visitor>
    void forEach(const Visitor& visit, const glm::mat4& placement = glm::mat4(1)) const {
        std::vector<glm::mat4> world(nodes.size());
        std::vector<bool> visible(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];
            const bool isRoot = node.parent < 0;
            const auto parentIndex = static_cast<std::size_t>(node.parent);
            // mundo = mundo del padre * local
            world[i] = (isRoot ? placement : world.at(parentIndex)) * node.local;
            visible[i] = node.visible && (isRoot || visible.at(parentIndex));
            if (!visible[i]) continue;
            for (const auto mesh : node.meshes) {
                if (meshes.at(mesh).visible) visit(mesh, world[i]);
            }
        }
    }
};
}
