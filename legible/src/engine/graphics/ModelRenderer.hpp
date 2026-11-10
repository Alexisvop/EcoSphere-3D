#pragma once
#include "assets/Model.hpp"
#include "scene/Lighting.hpp"
#include "Shading.hpp"
#include <glad/gl.h>
#include <filesystem>
#include <map>
#include <memory>

namespace ecosphere {
// una malla subida a la GPU: VAO + VBO (vértices) + EBO (índices)
class GpuMesh final {
public:
    explicit GpuMesh(const Mesh& mesh);
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    void draw() const;
private:
    void release() noexcept;
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

class Texture final {
public:
    Texture(const Image& image, const TextureData& settings);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    GLuint handle() const noexcept { return handle_; }
private:
    GLuint handle_ = 0;
};

struct ModelPlacement {
    std::shared_ptr<const Model> model;
    glm::mat4 placement{1};
};

// dibuja modelos con el shader model.vert / model.frag
class ModelRenderer final {
public:
    explicit ModelRenderer(const std::filesystem::path& shaders);
    ~ModelRenderer();
    ModelRenderer(const ModelRenderer&) = delete;
    ModelRenderer& operator=(const ModelRenderer&) = delete;
    void draw(const std::shared_ptr<const Model>& model, const glm::mat4& placement,
              const glm::mat4& view, const glm::mat4& projection,
              const LightingSettings& lighting = {}, glm::vec3 camera = {}, GLuint environment = 0);
    void drawModels(const std::vector<ModelPlacement>& models, const glm::mat4& view,
                    const glm::mat4& projection, const LightingSettings& lighting, glm::vec3 camera,
                    GLuint environment);
    void setShadingModel(ShadingModel model) { shadingName(model); shading_ = model; }
    void setFresnelEnabled(bool enabled) noexcept { fresnelEnabled_ = enabled; }
    void clear() noexcept { uploads_.clear(); }
private:
    // lo que ya se subió a la GPU de cada modelo
    struct Upload {
        std::shared_ptr<const Model> model;
        std::vector<std::unique_ptr<GpuMesh>> meshes;
        std::vector<std::unique_ptr<Texture>> textures;
    };
    std::map<std::shared_ptr<const Model>, Upload, std::owner_less<std::shared_ptr<const Model>>> uploads_;
    ShadingModel shading_ = ShadingModel::BlinnPhong;
    bool fresnelEnabled_ = true;
    GLuint program_ = 0;
    std::map<std::string, GLint> uniforms_;
    GLint uniform(const char* name) const { return uniforms_.at(name); }
};
}
