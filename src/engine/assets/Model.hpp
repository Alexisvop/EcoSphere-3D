#pragma once
#include "Material.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ecosphere {
struct Vertex {
    glm::vec3 position{0};
    glm::vec3 normal{0}; // Ausente: el shader iluminado usa la normal geométrica por derivadas.
    glm::vec2 uv{0};
};
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgba;
};
struct TextureData {
    std::size_t image = 0;
    int minFilter = 9729, magFilter = 9729; // LINEAR, valores del estándar glTF/OpenGL.
    int wrapS = 10497, wrapT = 10497;       // REPEAT.
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
    glm::mat4 local{1}; // Matriz glTF original; nunca se convierte a Euler.
    int parent = -1;    // Índice anterior al hijo; solo se carga la escena seleccionada.
    std::vector<std::size_t> meshes;
    bool visible = true;
};
struct Model {
    std::vector<Mesh> meshes; // Una por primitiva, compartida por las instancias de nodos.
    std::vector<ModelNode> nodes;
    std::vector<Image> images;
    std::vector<TextureData> textures;
    std::vector<std::string> warnings;
    template<class Visitor>
    void forEach(const Visitor& visit, const glm::mat4& placement = glm::mat4(1)) const {
        std::vector<glm::mat4> world(nodes.size());
        std::vector<bool> visible(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];
            world[i] = (node.parent < 0 ? placement : world.at(static_cast<std::size_t>(node.parent))) * node.local;
            visible[i] = node.visible && (node.parent < 0 || visible.at(static_cast<std::size_t>(node.parent)));
            if (visible[i]) for (const auto mesh : node.meshes) if (meshes.at(mesh).visible) visit(mesh, world[i]);
        }
    }
};
}
