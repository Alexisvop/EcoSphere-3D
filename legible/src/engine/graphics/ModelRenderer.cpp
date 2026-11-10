#include "ModelRenderer.hpp"
#include "Shader.hpp"
#include "RenderState.hpp"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace ecosphere {
namespace {
// uniforms que el shader model.frag/model.vert debe tener
constexpr const char* requiredUniforms[] = {
    "model", "view", "projection", "normalTransform", "baseColor", "textured", "baseTexture",
    "legacyUnlit", "unlit", "doubleSided", "alphaMode", "alphaCutoff", "metallic", "roughness",
    "specular", "reflection", "cameraPosition", "directEnabled", "ambient", "directionalEnabled",
    "directionalDirection", "directionalColor", "directionalIntensity", "pointEnabled", "pointPosition",
    "pointColor", "pointIntensity", "phongShading", "fresnelEnabled", "attenuation", "reflectionEnabled",
    "environmentIntensity", "fresnelPower", "environmentMap"};

// una malla de un modelo ya colocada en el mundo
struct Instance {
    std::shared_ptr<const Model> model;
    std::size_t mesh;
    glm::mat4 world;
    glm::mat3 normal;
    float depth;
};

// atajos para mandar uniforms por nombre
class Uniforms {
public:
    explicit Uniforms(const std::map<std::string, GLint>& locations) : locations_(locations) {}
    void setInt(const char* name, int value) const { glUniform1i(at(name), value); }
    void setFloat(const char* name, float value) const { glUniform1f(at(name), value); }
    void setVec3(const char* name, glm::vec3 value) const {
        glUniform3fv(at(name), 1, glm::value_ptr(value));
    }
    void setVec4(const char* name, const glm::vec4& value) const {
        glUniform4fv(at(name), 1, glm::value_ptr(value));
    }
    void setMat3(const char* name, const glm::mat3& value) const {
        glUniformMatrix3fv(at(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    void setMat4(const char* name, const glm::mat4& value) const {
        glUniformMatrix4fv(at(name), 1, GL_FALSE, glm::value_ptr(value));
    }
private:
    GLint at(const char* name) const { return locations_.at(name); }
    const std::map<std::string, GLint>& locations_;
};

void validateColor(const glm::vec4& color) {
    for (int c = 0; c < 4; ++c) {
        if (!std::isfinite(color[c]) || color[c] < 0 || color[c] > 1) {
            throw std::invalid_argument("Invalid base color");
        }
    }
}

// junta las mallas visibles y las separa en opacas y transparentes
void collectInstances(const std::vector<ModelPlacement>& models, const glm::mat4& view,
                      std::vector<Instance>& opaque, std::vector<Instance>& blended) {
    for (const auto& item : models) {
        const auto& model = item.model;
        if (!model) throw std::invalid_argument("Cannot draw null model");
        model->forEach([&](std::size_t index, const glm::mat4& world) {
            const auto& mesh = model->meshes.at(index);
            validateMaterial(mesh.material);
            validateColor(mesh.color);
            const float depth = (view * world * glm::vec4(0, 0, 0, 1)).z;
            // normalMatrix: inversa transpuesta de la parte 3x3 de world
            Instance instance{model, index, world, normalMatrix(world), depth};
            if (!std::isfinite(instance.depth)) throw std::invalid_argument("Non-finite instance depth");
            if (mesh.material.alphaMode == AlphaMode::blend) {
                blended.push_back(instance);
            } else {
                opaque.push_back(instance);
            }
        }, item.placement);
    }
}

// uniforms iguales para todo el cuadro: cámara, luces y reflejo
void setFrameUniforms(const Uniforms& uniforms, const glm::mat4& view, const glm::mat4& projection,
                      const LightingSettings& lighting, glm::vec3 camera, GLuint environment,
                      bool phong, bool fresnel) {
    uniforms.setMat4("view", view);
    uniforms.setMat4("projection", projection);
    uniforms.setInt("phongShading", phong);
    uniforms.setInt("fresnelEnabled", fresnel);
    uniforms.setInt("baseTexture", 0);
    uniforms.setInt("environmentMap", 1);
    uniforms.setInt("legacyUnlit", lighting.legacyUnlit);
    uniforms.setInt("directEnabled", lighting.directEnabled);
    uniforms.setFloat("ambient", lighting.ambient);
    uniforms.setVec3("cameraPosition", camera);
    const auto& directional = lighting.directional;
    const auto& point = lighting.point;
    uniforms.setInt("directionalEnabled", directional.enabled);
    uniforms.setInt("pointEnabled", point.enabled);
    uniforms.setVec3("directionalDirection", glm::normalize(directional.vector));
    uniforms.setVec3("directionalColor", directional.color);
    uniforms.setFloat("directionalIntensity", directional.intensity);
    uniforms.setVec3("pointPosition", point.vector);
    uniforms.setVec3("pointColor", point.color);
    uniforms.setFloat("pointIntensity", point.intensity);
    uniforms.setVec3("attenuation", lighting.attenuation);
    uniforms.setInt("reflectionEnabled", lighting.environment.reflectionEnabled && environment != 0);
    uniforms.setFloat("environmentIntensity", lighting.environment.intensity);
    uniforms.setFloat("fresnelPower", lighting.environment.fresnelPower);
}

// el cubemap del cielo va en la unidad de textura 1
void bindEnvironment(GLuint environment) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environment);
    glActiveTexture(GL_TEXTURE0);
}

void setDepthAndBlend() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void drawInstance(const Uniforms& uniforms, const Instance& instance, const GpuMesh& gpuMesh,
                  const std::vector<std::unique_ptr<Texture>>& textures, bool legacyUnlit) {
    const auto& mesh = instance.model->meshes.at(instance.mesh);
    const auto& material = mesh.material;
    if (material.doubleSided || legacyUnlit) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    // una escala negativa invierte el orden de los vértices
    glFrontFace(glm::determinant(glm::mat3(instance.world)) < 0 ? GL_CW : GL_CCW);
    uniforms.setMat4("model", instance.world);
    uniforms.setMat3("normalTransform", instance.normal);
    uniforms.setVec4("baseColor", mesh.color);
    uniforms.setInt("unlit", material.unlit);
    uniforms.setInt("doubleSided", material.doubleSided);
    uniforms.setInt("alphaMode", static_cast<int>(material.alphaMode));
    uniforms.setFloat("alphaCutoff", material.alphaCutoff);
    uniforms.setFloat("metallic", material.metallic);
    uniforms.setFloat("roughness", material.roughness);
    uniforms.setFloat("specular", material.specular);
    uniforms.setFloat("reflection", material.reflection);
    uniforms.setInt("textured", mesh.texture >= 0);
    if (mesh.texture < 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    } else {
        glBindTexture(GL_TEXTURE_2D, textures.at(static_cast<std::size_t>(mesh.texture))->handle());
    }
    gpuMesh.draw();
}
}

// ---- Malla en GPU (VAO + VBO + EBO) ----

GpuMesh::GpuMesh(const Mesh& mesh) {
    const auto maxCount = static_cast<std::size_t>(std::numeric_limits<GLsizei>::max());
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size() > maxCount) {
        throw std::runtime_error("Invalid GPU mesh size");
    }
    for (auto index : mesh.indices) {
        if (index >= mesh.vertices.size()) throw std::runtime_error("Invalid GPU mesh index");
    }
    for (const auto& vertex : mesh.vertices) {
        if (!finiteVector(vertex.position) || !finiteVector(vertex.normal) ||
            !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y)) {
            throw std::invalid_argument("Non-finite GPU vertex/UV");
        }
    }
    count_ = static_cast<GLsizei>(mesh.indices.size());
    const auto vertexBytes = static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex));
    const auto indexBytes = static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t));
    const GLsizei stride = sizeof(Vertex);
    try {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        // el VAO guarda la configuración, el VBO los vértices y el EBO los índices
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertexBytes, mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes, mesh.indices.data(), GL_STATIC_DRAW);
        // location 0 posición, 1 normal, 2 uv (igual que en model.vert)
        const auto* positionOffset = reinterpret_cast<void*>(offsetof(Vertex, position));
        const auto* normalOffset = reinterpret_cast<void*>(offsetof(Vertex, normal));
        const auto* uvOffset = reinterpret_cast<void*>(offsetof(Vertex, uv));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, positionOffset);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, normalOffset);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, uvOffset);
        glEnableVertexAttribArray(2);
        if (!vao_ || !vbo_ || !ebo_ || glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("Cannot upload model VAO/VBO/EBO");
        }
    } catch (...) {
        release();
        throw;
    }
}

void GpuMesh::release() noexcept {
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

GpuMesh::~GpuMesh() { release(); }

void GpuMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}

// ---- Textura en GPU ----

Texture::Texture(const Image& image, const TextureData& settings) {
    if (image.width <= 0 || image.height <= 0 || image.width > 4096 || image.height > 4096 ||
        image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4) {
        throw std::runtime_error("Invalid texture pixels");
    }
    glGenTextures(1, &handle_);
    glBindTexture(GL_TEXTURE_2D, handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, settings.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, settings.magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, settings.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, settings.wrapT);
    // el color base viene en sRGB
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    if (!handle_ || glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &handle_);
        throw std::runtime_error("Cannot upload texture");
    }
}

Texture::~Texture() { glDeleteTextures(1, &handle_); }

// ---- Renderer ----

ModelRenderer::ModelRenderer(const std::filesystem::path& shaders) {
    const auto vertexText = readShaderFile(shaders / "model.vert");
    const auto fragmentText = readShaderFile(shaders / "model.frag");
    Shader vertex(GL_VERTEX_SHADER, vertexText.c_str());
    Shader fragment(GL_FRAGMENT_SHADER, fragmentText.c_str());
    program_ = linkProgram(vertex.handle, fragment.handle);
    try {
        for (const auto* name : requiredUniforms) {
            const auto location = glGetUniformLocation(program_, name);
            if (location < 0) {
                throw std::runtime_error(std::string("Model shader missing required uniform: ") + name);
            }
            uniforms_.emplace(name, location);
        }
    } catch (...) {
        glDeleteProgram(program_);
        throw;
    }
}

ModelRenderer::~ModelRenderer() {
    clear();
    glDeleteProgram(program_);
}

void ModelRenderer::draw(const std::shared_ptr<const Model>& model, const glm::mat4& placement,
                         const glm::mat4& view, const glm::mat4& projection,
                         const LightingSettings& lighting, glm::vec3 camera, GLuint environment) {
    drawModels({{model, placement}}, view, projection, lighting, camera, environment);
}

void ModelRenderer::drawModels(const std::vector<ModelPlacement>& models, const glm::mat4& view,
                               const glm::mat4& projection, const LightingSettings& lighting,
                               glm::vec3 camera, GLuint environment) {
    validateLighting(lighting, camera);
    std::vector<Instance> opaque, blended;
    collectInstances(models, view, opaque, blended);
    // transparentes de la más lejana a la más cercana
    std::stable_sort(blended.begin(), blended.end(),
                     [](const Instance& a, const Instance& b) { return a.depth < b.depth; });
    RenderState state;
    glActiveTexture(GL_TEXTURE0);
    // subir a la GPU los modelos que todavía no están
    for (const auto& item : models) {
        const auto& model = item.model;
        if (uploads_.find(model) != uploads_.end()) continue;
        Upload upload;
        upload.model = model;
        for (const auto& mesh : model->meshes) upload.meshes.push_back(std::make_unique<GpuMesh>(mesh));
        for (const auto& texture : model->textures) {
            upload.textures.push_back(std::make_unique<Texture>(model->images.at(texture.image), texture));
        }
        uploads_.emplace(model, std::move(upload));
    }
    glUseProgram(program_);
    const Uniforms uniforms(uniforms_);
    setFrameUniforms(uniforms, view, projection, lighting, camera, environment,
                     shading_ == ShadingModel::Phong, fresnelEnabled_);
    bindEnvironment(environment);
    setDepthAndBlend();
    // primero opacos, después transparentes sin escribir profundidad
    for (const bool blend : {false, true}) {
        glDepthMask(blend ? GL_FALSE : GL_TRUE);
        if (blend) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        for (const auto& instance : blend ? blended : opaque) {
            const auto& upload = uploads_.at(instance.model);
            const auto& gpuMesh = *upload.meshes.at(instance.mesh);
            drawInstance(uniforms, instance, gpuMesh, upload.textures, lighting.legacyUnlit);
        }
    }
}
}
