#include "ModelRenderer.hpp"
#include "Shader.hpp"
#include "RenderState.hpp"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ecosphere {
GpuMesh::GpuMesh(const Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("Invalid GPU mesh size");
    for (auto index : mesh.indices) if (index >= mesh.vertices.size()) throw std::runtime_error("Invalid GPU mesh index");
    for (const auto& vertex : mesh.vertices)
        if (!finiteVector(vertex.position) || !finiteVector(vertex.normal) || !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y))
            throw std::invalid_argument("Non-finite GPU vertex/UV");
    count_ = static_cast<GLsizei>(mesh.indices.size());
    try {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)), mesh.indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(2);
        if (!vao_ || !vbo_ || !ebo_ || glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot upload model VAO/VBO/EBO");
    } catch (...) { release(); throw; }
}
void GpuMesh::release() noexcept {
    glDeleteBuffers(1, &ebo_); glDeleteBuffers(1, &vbo_); glDeleteVertexArrays(1, &vao_);
}
GpuMesh::~GpuMesh() { release(); }
void GpuMesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}
Texture::Texture(const Image& image, const TextureData& settings) {
    if (image.width <= 0 || image.height <= 0 || image.width > 4096 || image.height > 4096 ||
        image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4)
        throw std::runtime_error("Invalid texture pixels");
    glGenTextures(1, &handle_);
    glBindTexture(GL_TEXTURE_2D, handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, settings.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, settings.magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, settings.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, settings.wrapT);
    // Base color glTF es sRGB; el shader vuelve a sRGB al escribir al framebuffer no sRGB.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    if (!handle_ || glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &handle_);
        throw std::runtime_error("Cannot upload texture");
    }
}
Texture::~Texture() { glDeleteTextures(1, &handle_); }
ModelRenderer::ModelRenderer(const std::filesystem::path& shaders) {
    const auto vertText = readShaderFile(shaders / "model.vert"), fragText = readShaderFile(shaders / "model.frag");
    Shader vertex(GL_VERTEX_SHADER, vertText.c_str()), fragment(GL_FRAGMENT_SHADER, fragText.c_str());
    program_ = linkProgram(vertex.handle, fragment.handle);
    try {
        for (const auto* name : {"model", "view", "projection", "normalTransform", "baseColor", "textured", "baseTexture",
             "legacyUnlit", "unlit", "doubleSided", "alphaMode", "alphaCutoff", "metallic", "roughness", "specular",
             "reflection", "cameraPosition", "directEnabled", "ambient", "directionalEnabled", "directionalDirection",
             "directionalColor", "directionalIntensity", "pointEnabled", "pointPosition", "pointColor", "pointIntensity",
             "phongShading", "fresnelEnabled", "attenuation", "reflectionEnabled", "environmentIntensity", "fresnelPower", "environmentMap"}) {
            const auto location = glGetUniformLocation(program_, name);
            if (location < 0) throw std::runtime_error(std::string("Model shader missing required uniform: ") + name);
            uniforms_.emplace(name, location);
        }
    } catch (...) { glDeleteProgram(program_); throw; }
}

ModelRenderer::~ModelRenderer() { clear(); glDeleteProgram(program_); }
void ModelRenderer::draw(const std::shared_ptr<const Model>& model, const glm::mat4& placement,
                         const glm::mat4& view, const glm::mat4& projection,
                         const LightingSettings& lighting, glm::vec3 camera, GLuint environment) {
    drawModels({{model,placement}},view,projection,lighting,camera,environment);
}
void ModelRenderer::drawModels(const std::vector<ModelPlacement>& models, const glm::mat4& view, const glm::mat4& projection,
                               const LightingSettings& lighting, glm::vec3 camera, GLuint environment) {
    validateLighting(lighting, camera);
    struct Instance { std::shared_ptr<const Model> model; std::size_t mesh; glm::mat4 world; glm::mat3 normal; float depth; };
    std::vector<Instance> opaque, blended;
    for (const auto& item : models) {
        const auto& model = item.model;
        if (!model) throw std::invalid_argument("Cannot draw null model");
        model->forEach([&](std::size_t index, const glm::mat4& world) {
            const auto& mesh = model->meshes.at(index);
            validateMaterial(mesh.material);
            for (int c = 0; c < 4; ++c)
                if (!std::isfinite(mesh.color[c]) || mesh.color[c] < 0 || mesh.color[c] > 1) throw std::invalid_argument("Invalid base color");
            Instance instance{model, index, world, normalMatrix(world), (view * world * glm::vec4(0,0,0,1)).z};
            if (!std::isfinite(instance.depth)) throw std::invalid_argument("Non-finite instance depth");
            (mesh.material.alphaMode == AlphaMode::blend ? blended : opaque).push_back(instance);
        }, item.placement);
    }
    // shortcut: orden por origen de instancia, usar partición geométrica si se entrecruzan transparencias.
    std::stable_sort(blended.begin(), blended.end(), [](const auto& a, const auto& b) { return a.depth < b.depth; });
    RenderState state;
    glActiveTexture(GL_TEXTURE0);
    for (const auto& item : models) {
        const auto& model = item.model;
        if (uploads_.find(model) == uploads_.end()) {
            Upload upload;
            upload.model = model;
            for (const auto& mesh : model->meshes) upload.meshes.push_back(std::make_unique<GpuMesh>(mesh));
            for (const auto& texture : model->textures)
                upload.textures.push_back(std::make_unique<Texture>(model->images.at(texture.image), texture));
            uploads_.emplace(model, std::move(upload));
        }
    }
    glUseProgram(program_);
    glUniformMatrix4fv(uniform("view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    const auto scalar = [&](const char* name, float value) { glUniform1f(uniform(name), value); };
    const auto flag = [&](const char* name, int value) { glUniform1i(uniform(name), value); };
    const auto vector = [&](const char* name, glm::vec3 value) { glUniform3fv(uniform(name), 1, glm::value_ptr(value)); };
    flag("phongShading", shading_==ShadingModel::Phong); flag("fresnelEnabled", fresnelEnabled_);
    flag("baseTexture", 0); flag("environmentMap", 1);
    flag("legacyUnlit", lighting.legacyUnlit); flag("directEnabled", lighting.directEnabled);
    scalar("ambient", lighting.ambient); vector("cameraPosition", camera);
    flag("directionalEnabled", lighting.directional.enabled); flag("pointEnabled", lighting.point.enabled);
    vector("directionalDirection", glm::normalize(lighting.directional.vector)); vector("directionalColor", lighting.directional.color);
    scalar("directionalIntensity", lighting.directional.intensity);
    vector("pointPosition", lighting.point.vector); vector("pointColor", lighting.point.color); scalar("pointIntensity", lighting.point.intensity);
    vector("attenuation", lighting.attenuation);
    flag("reflectionEnabled", lighting.environment.reflectionEnabled && environment != 0);
    scalar("environmentIntensity", lighting.environment.intensity); scalar("fresnelPower", lighting.environment.fresnelPower);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, environment); glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    const auto drawInstances = [&](const std::vector<Instance>& instances, bool blend) {
        glDepthMask(blend ? GL_FALSE : GL_TRUE);
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        for (const auto& instance : instances) {
            const auto& mesh = instance.model->meshes.at(instance.mesh);
            const auto& upload = uploads_.at(instance.model);
            const auto& material = mesh.material;
            if (material.doubleSided || lighting.legacyUnlit) glDisable(GL_CULL_FACE);
            else { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
            glFrontFace(glm::determinant(glm::mat3(instance.world)) < 0 ? GL_CW : GL_CCW);
            glUniformMatrix4fv(uniform("model"), 1, GL_FALSE, glm::value_ptr(instance.world));
            glUniformMatrix3fv(uniform("normalTransform"), 1, GL_FALSE, glm::value_ptr(instance.normal));
            glUniform4fv(uniform("baseColor"), 1, glm::value_ptr(mesh.color));
            flag("unlit", material.unlit); flag("doubleSided", material.doubleSided);
            flag("alphaMode", static_cast<int>(material.alphaMode)); scalar("alphaCutoff", material.alphaCutoff);
            scalar("metallic", material.metallic); scalar("roughness", material.roughness);
            scalar("specular", material.specular); scalar("reflection", material.reflection);
            flag("textured", mesh.texture >= 0);
            glBindTexture(GL_TEXTURE_2D, mesh.texture < 0 ? 0 : upload.textures.at(static_cast<std::size_t>(mesh.texture))->handle());
            upload.meshes.at(instance.mesh)->draw();
        }
    };
    drawInstances(opaque, false);
    drawInstances(blended, true);
}
}
