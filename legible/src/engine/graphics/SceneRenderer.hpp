#pragma once
#include <glad/gl.h>
#include "scene/Scene.hpp"
#include "ModelRenderer.hpp"
#include <filesystem>

namespace ecosphere {
// dibuja la escena de cubos con un solo cubo en GPU
class SceneRenderer final {
public:
    explicit SceneRenderer(const std::filesystem::path& shaderDirectory);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void draw(const Scene& scene, const glm::mat4& view, const glm::mat4& projection) const;
    void drawLit(ModelRenderer& renderer, const Scene& scene, const glm::mat4& view,
                 const glm::mat4& projection, const LightingSettings& lighting, glm::vec3 camera,
                 GLuint environment);
private:
    void release() noexcept;
    GLuint program_ = 0, vertexArray_ = 0, vertexBuffer_ = 0, indexBuffer_ = 0;
    GLint modelLocation_ = -1, viewLocation_ = -1, projectionLocation_ = -1, colorLocation_ = -1;
    std::shared_ptr<Model> litCubes_;
};
}
