#include "LightingDemo.hpp"
#include "LightingDemoSettings.hpp"
#include <glm/gtc/constants.hpp>

namespace ecosphere {
namespace {
Mesh makePanel() {
    Mesh mesh;
    mesh.vertices = {{{-0.5f,-0.5f,0},{0,0,1},{0,1}}, {{0.5f,-0.5f,0},{0,0,1},{1,1}},
                     {{0.5f,0.5f,0},{0,0,1},{1,0}}, {{-0.5f,0.5f,0},{0,0,1},{0,0}}};
    mesh.indices = {0,1,2,2,3,0};
    return mesh;
}
Mesh makeSphere() {
    Mesh mesh;
    constexpr std::uint32_t rings = 16, segments = 32;
    for (std::uint32_t y = 0; y <= rings; ++y) for (std::uint32_t x = 0; x <= segments; ++x) {
        const float theta = glm::pi<float>() * y / rings, phi = glm::two_pi<float>() * x / segments;
        const glm::vec3 normal{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};
        mesh.vertices.push_back({normal,normal,{float(x)/segments,float(y)/rings}});
    }
    for (std::uint32_t y = 0; y < rings; ++y) for (std::uint32_t x = 0; x < segments; ++x) {
        const auto a = y * (segments+1) + x, b = a + segments+1;
        mesh.indices.insert(mesh.indices.end(), {a,a+1,b, a+1,b+1,b});
    }
    return mesh;
}
}
Mesh makeUnitCube() {
    Mesh mesh;
    const std::array<glm::vec3,6> normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    for (auto n : normals) {
        const auto right = glm::normalize(glm::cross(std::abs(n.y) > 0.5f ? glm::vec3(0,0,1) : glm::vec3(0,1,0), n));
        const auto up = glm::cross(n, right);
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        for (auto uv : {glm::vec2(0,0),glm::vec2(1,0),glm::vec2(1,1),glm::vec2(0,1)})
            mesh.vertices.push_back({n*0.5f+right*(uv.x-0.5f)+up*(uv.y-0.5f),n,uv});
        mesh.indices.insert(mesh.indices.end(), {start,start+1,start+2,start+2,start+3,start});
    }
    return mesh;
}
std::shared_ptr<Model> makeLightingDemo() {
    auto model = std::make_shared<Model>();
    const auto add = [&](Mesh mesh, const Transform& transform, glm::vec4 color, MaterialSettings material, const char* name) {
        mesh.color = color; mesh.material = material;
        ModelNode node; node.name = name; node.local = transform.matrix(); node.meshes = {model->meshes.size()};
        model->meshes.push_back(std::move(mesh)); model->nodes.push_back(std::move(node));
    };
    const auto sphere = makeSphere();
    add(sphere,demo::materialPositions[0],demo::matteColor,demo::matte,"matte");
    add(sphere,demo::materialPositions[1],demo::plasticColor,demo::plastic,"plastic");
    add(sphere,demo::materialPositions[2],demo::metalColor,demo::metal,"metal");
    auto marker = demo::matte; marker.unlit = true;
    add(makeUnitCube(),demo::glassBackdrop,{0.95f,0.15f,0.1f,1},marker,"glass-backdrop");
    add(makePanel(),demo::glassPlacement,demo::glassColor,demo::glass,"glass");
    // RGBA propio 2x2: el mismo recurso demuestra MASK y alfa de textura sin duplicarlo.
    model->images.push_back({2,2,{255,255,255,255, 255,255,255,0, 255,255,255,0, 255,255,255,255}});
    TextureData sampler; sampler.magFilter = sampler.minFilter = 9728;
    model->textures.push_back(sampler);
    auto mask = demo::matte; mask.alphaMode = AlphaMode::mask; mask.doubleSided = true;
    auto panel = makePanel(); panel.texture = 0;
    add(std::move(panel),demo::maskPlacement,{0.9f,0.5f,0.05f,1},mask,"alpha-mask");
    return model;
}
}
