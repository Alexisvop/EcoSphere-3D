#include "SceneRenderer.hpp"
#include "Shader.hpp"
#include "world/DemoSceneSettings.hpp"
#include "world/LightingDemo.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>

namespace {
std::string readShader(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open shader: " + path.string());
    std::ostringstream text;
    text << input.rdbuf();
    if (input.bad()) throw std::runtime_error("Cannot read shader: " + path.string());
    return text.str();
}
}

namespace ecosphere {
SceneRenderer::SceneRenderer(const std::filesystem::path& shaderDirectory) {
    try {
        const auto vertexText = readShader(shaderDirectory / "scene.vert");
        const auto fragmentText = readShader(shaderDirectory / "scene.frag");
        Shader vertex(GL_VERTEX_SHADER, vertexText.c_str());
        Shader fragment(GL_FRAGMENT_SHADER, fragmentText.c_str());
        program_ = linkProgram(vertex.handle, fragment.handle);
        modelLocation_ = glGetUniformLocation(program_, "model");
        viewLocation_ = glGetUniformLocation(program_, "view");
        projectionLocation_ = glGetUniformLocation(program_, "projection");
        colorLocation_ = glGetUniformLocation(program_, "objectColor");
        if (modelLocation_ < 0 || viewLocation_ < 0 || projectionLocation_ < 0 || colorLocation_ < 0) {
            throw std::runtime_error("Scene shader requires model/view/projection/objectColor uniforms");
        }
        // Cuatro vértices por cara permiten asignar colores constantes distintos por eje.
        // Cada fila contiene XYZ y factor de color; no hay normales ni iluminación.
        constexpr float front = demo::frontBackTint, side = demo::leftRightTint, top = demo::topBottomTint;
        constexpr float vertices[] = {
            -0.5f,-0.5f,-0.5f,front,   0.5f,-0.5f,-0.5f,front,
             0.5f, 0.5f,-0.5f,front,  -0.5f, 0.5f,-0.5f,front,
            -0.5f,-0.5f, 0.5f,front,   0.5f,-0.5f, 0.5f,front,
             0.5f, 0.5f, 0.5f,front,  -0.5f, 0.5f, 0.5f,front,
            -0.5f,-0.5f,-0.5f,side,  -0.5f,-0.5f, 0.5f,side,
            -0.5f, 0.5f, 0.5f,side,  -0.5f, 0.5f,-0.5f,side,
             0.5f,-0.5f,-0.5f,side,   0.5f,-0.5f, 0.5f,side,
             0.5f, 0.5f, 0.5f,side,   0.5f, 0.5f,-0.5f,side,
            -0.5f, 0.5f,-0.5f,top,    0.5f, 0.5f,-0.5f,top,
             0.5f, 0.5f, 0.5f,top,   -0.5f, 0.5f, 0.5f,top,
            -0.5f,-0.5f,-0.5f,top,    0.5f,-0.5f,-0.5f,top,
             0.5f,-0.5f, 0.5f,top,   -0.5f,-0.5f, 0.5f,top
        };
        constexpr unsigned int indices[] = {
             0, 1, 2,  2, 3, 0,    4, 5, 6,  6, 7, 4,
             8, 9,10, 10,11, 8,   12,13,14, 14,15,12,
            16,17,18, 18,19,16,   20,21,22, 22,23,20
        };
        glGenVertexArrays(1, &vertexArray_);
        glGenBuffers(1, &vertexBuffer_);
        glGenBuffers(1, &indexBuffer_);
        glBindVertexArray(vertexArray_);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot allocate cube VAO/VBO/EBO");
    } catch (...) {
        release();
        throw;
    }
}

SceneRenderer::~SceneRenderer() { release(); }
void SceneRenderer::release() noexcept {
    glDeleteBuffers(1, &indexBuffer_);
    glDeleteBuffers(1, &vertexBuffer_);
    glDeleteVertexArrays(1, &vertexArray_);
    glDeleteProgram(program_);
}

void SceneRenderer::draw(const Scene& scene, const glm::mat4& view, const glm::mat4& projection) const {
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glUniformMatrix4fv(viewLocation_, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
    scene.forEach([&](const SceneNode& node, const glm::mat4& world) {
        glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(world));
        glUniform3fv(colorLocation_, 1, glm::value_ptr(node.color));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    });
}
void SceneRenderer::drawLit(ModelRenderer& renderer, const Scene& scene, const glm::mat4& view, const glm::mat4& projection,
                            const LightingSettings& lighting, glm::vec3 camera, GLuint environment) {
    if (!litCubes_) {
        litCubes_ = std::make_shared<Model>();
        scene.forEach([&](const SceneNode&, const glm::mat4&) {
            auto mesh = makeUnitCube(); mesh.material.unlit = false;
            mesh.material.roughness = 0.8f; mesh.material.reflection = 0.1f;
            ModelNode node; node.meshes = {litCubes_->meshes.size()};
            litCubes_->meshes.push_back(std::move(mesh)); litCubes_->nodes.push_back(std::move(node));
        });
    }
    std::size_t index = 0;
    scene.forEach([&](const SceneNode& node, const glm::mat4& world) {
        litCubes_->nodes.at(index).local = world;
        litCubes_->meshes.at(index).color = glm::vec4(node.color,1);
        ++index;
    });
    renderer.draw(litCubes_,glm::mat4(1),view,projection,lighting,camera,environment);
}
}
