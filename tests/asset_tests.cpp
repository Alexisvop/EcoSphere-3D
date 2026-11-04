#include "assets/AssetManager.hpp"
#include "AssetFixture.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
using namespace fixture_test;
void near(glm::vec4 actual, glm::vec4 expected) { check(glm::length(actual - expected) < 0.0001f, "Hierarchy transform mismatch"); }
}
int main(int argc, char** argv) {
    try {
        check(argc == 3, "Need scratch and Blender resource directory");
        const auto scratch = fs::absolute(argv[1]); fs::create_directories(scratch);
        ecosphere::AssetManager assets(scratch);
        fixture(scratch / "valid.glb", valid);
        auto model = assets.loadModel("valid.glb");
        check(model->meshes.size() == 2 && model->nodes.size() == 3, "Multiple primitives/nodes missing");
        check(model->meshes[0].vertices[1].uv == glm::vec2(1,0) && model->meshes[0].vertices[0].normal == glm::vec3(0,0,1), "UV/normal mismatch");
        check(model->meshes[0].indices == std::vector<std::uint32_t>({0,1,2}), "Index mismatch");
        check(model->nodes[1].meshes == model->nodes[2].meshes, "Instances must reference the same primitives");
        int draws = 0;
        model->forEach([&](std::size_t, const glm::mat4& world) {
            near(world * glm::vec4(1,0,0,1), draws < 2 ? glm::vec4(2,6,4,1) : glm::vec4(0,4,4,1)); ++draws;
        });
        check(draws == 4, "Expected 4 shared mesh instances");
        ecosphere::Model editable = *model;
        editable.nodes[0].local = glm::translate(glm::mat4(1), {5,0,0});
        editable.nodes[2].visible = false; editable.meshes[1].visible = false;
        draws = 0;
        editable.forEach([&](std::size_t, const glm::mat4& world) { near(world * glm::vec4(0,0,0,1), {6,0,0,1}); ++draws; });
        check(draws == 1, "Visibility/editable hierarchy failed");
        check(assets.loadModel("./valid.glb") == model && assets.loadModel("sub/../valid.glb") == model, "Canonical cache must reuse model");
        const auto oldCwd = fs::current_path();
        fs::current_path(scratch.parent_path());
        const bool sameFromOtherCwd = assets.loadModel("valid.glb") == model;
        fs::current_path(oldCwd);
        check(sameFromOtherCwd, "Asset root must survive cwd change");
        std::weak_ptr<const ecosphere::Model> lifetime = model;
        assets.clear(); check(!lifetime.expired(), "External owner must survive cache clear");
        model.reset(); check(lifetime.expired(), "CPU model must release after last owner");
        const auto fails = [&](const fs::path& path, const std::string& diagnostic) {
            try { (void)assets.loadModel(path); }
            catch (const std::exception& e) { check(std::string(e.what()).find(diagnostic) != std::string::npos, e.what()); return; }
            throw std::runtime_error("Invalid asset accepted: " + path.string());
        };
        fails("missing.glb", "Cannot open model");
        std::ofstream(scratch / "corrupt.glb") << "not a GLB";
        fails("corrupt.glb", "parse code");
        fails("../outside.glb", "escapes root"); fails(scratch / "valid.glb", "relative");
        const auto negative = [&](const std::string& json, const std::string& message) {
            std::cout << "Negative case: " << message << std::endl;
            assets.clear();
            fixture(scratch / "bad.glb", json); fails("bad.glb", message);
        };
        negative(replace(valid, "\"byteLength\":96", "\"byteLength\":999999999"), "buffer view");
        negative(replace(valid, "\"byteOffset\":24", "\"byteOffset\":999999999999999999"), "Accessor exceeds");
        negative(replace(valid, "\"count\":3", "\"count\":0"), "accessor count");
        negative(replace(valid, "\"count\":3", "\"count\":999999999999999999"), "accessor count");
        negative(replace(valid, "\"byteLength\":104", "\"byteLength\":104,\"uri\":\"../secret.bin\""), "URIs unsupported");
        negative(replace(valid, "\"indices\":3", "\"indices\":3,\"mode\":1"), "Unsupported primitive");
        negative(replace(valid, "\"children\":[1,2]", "\"children\":[0,1,2]"), "parse code");
        negative(replace(valid, "\"scene\":0", "\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"scene\":0"), "required extension");
        // Una carga fallida no contamina la caché: el mismo nombre corregido debe cargar.
        fixture(scratch / "bad.glb", valid); check(bool(assets.loadModel("bad.glb")), "Failed load poisoned cache");
        auto optional = valid;
        for (int i = 0; i < 2; ++i) optional = replace(optional, ",\"NORMAL\":1,\"TEXCOORD_0\":2", "");
        fixture(scratch / "optional.glb", optional);
        check(assets.loadModel("optional.glb")->meshes[0].vertices[0].normal == glm::vec3(0), "Missing normal default");
        auto unindexed = valid;
        for (int i = 0; i < 2; ++i) unindexed = replace(unindexed, ",\"indices\":3", "");
        fixture(scratch / "unindexed.glb", unindexed);
        check(assets.loadModel("unindexed.glb")->meshes[0].indices.size() == 3, "Nonindexed triangles");
#if !defined(_WIN32)
        const auto link = scratch / "escape";
        if (!fs::exists(link)) fs::create_directory_symlink(scratch.parent_path(), link);
        fails("escape/outside.glb", "escapes root");
#endif
        ecosphere::AssetManager blender(argv[2]);
        auto probe = blender.loadModel("blender_probe_unlit.glb");
        check(probe->meshes.size() == 2 && probe->nodes.size() == 4 && probe->textures.size() == 1 && probe->images.size() == 1, "Blender export content");
        check(probe->images[0].width == 2 && probe->images[0].height == 2 && probe->images[0].rgba.size() == 16, "Embedded PNG decode");
        check(probe->meshes[0].texture == 0 && probe->textures[0].magFilter == 9728, "Texture association/sampler");
        check(probe->images[0].rgba[0] > 240 && probe->images[0].rgba[1] > 240, "PNG orientation top-left should be white");
        const auto loadBytes = [](const fs::path& path) {
            std::ifstream in(path, std::ios::binary);
            return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), {});
        };
        // Segunda imagen corrupta después de decodificar la primera: rollback CPU parcial.
        const auto glb = loadBytes(fs::path(argv[2]) / "blender_probe_unlit.glb");
        const std::array<unsigned char,8> signature{137,80,78,71,13,10,26,10};
        const auto begin = std::search(glb.begin(), glb.end(), signature.begin(), signature.end());
        check(begin != glb.end(), "Blender PNG signature missing");
        const std::array<unsigned char,8> endMarker{73,69,78,68,174,66,96,130};
        const auto end = std::search(begin, glb.end(), endMarker.begin(), endMarker.end());
        check(end != glb.end(), "PNG IEND missing");
        std::vector<unsigned char> png(begin, end + 8);
        const auto imageSize = png.size();
        png.insert(png.end(), {0,0,0,0});
        auto brokenImage = replace(valid, "\"byteLength\":104", "\"byteLength\":" + std::to_string(104 + png.size()));
        brokenImage = replace(brokenImage, "\"accessors\":[", "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"},{\"bufferView\":3,\"mimeType\":\"image/png\"}],\"accessors\":[");
        brokenImage = replace(brokenImage, "\"byteLength\":6}]", "\"byteLength\":6},{\"buffer\":0,\"byteOffset\":104,\"byteLength\":" + std::to_string(imageSize) +
            "},{\"buffer\":0,\"byteOffset\":" + std::to_string(104 + imageSize) + ",\"byteLength\":4}]");
        fixture(scratch / "image-error.glb", brokenImage, png);
        fails("image-error.glb", "Invalid/oversized image");
        auto alpha = replace(valid, "\"meshes\":[", "\"materials\":[{\"alphaMode\":\"BLEND\"}],\"meshes\":[");
        alpha = replace(alpha, "\"indices\":3", "\"indices\":3,\"material\":0");
        // BLEND ahora se admite: preservar el fixture y exigir la nueva semántica.
        fixture(scratch / "alpha-blend.glb", alpha);
        check(assets.loadModel("alpha-blend.glb")->meshes[0].material.alphaMode == ecosphere::AlphaMode::blend, "BLEND alphaMode lost");
        negative(replace(alpha, "\"alphaMode\":\"BLEND\"", "\"alphaMode\":\"BLEND\",\"alphaCutoff\":-1"), "material factor");
        auto invalidIndex = loadBytes(scratch / "valid.glb");
        // Últimos 8 bytes son el bloque de índices; 99 excede los tres vértices.
        invalidIndex[invalidIndex.size() - 8] = 99;
        { std::ofstream out(scratch / "index-error.glb", std::ios::binary); out.write(reinterpret_cast<const char*>(invalidIndex.data()), static_cast<std::streamsize>(invalidIndex.size())); }
        fails("index-error.glb", "indices");
        for (int i = 0; i < 100; ++i) { fails("corrupt.glb", "parse code"); blender.clear(); probe = blender.loadModel("blender_probe_unlit.glb"); }
        std::cout << "Assets PASS: GLB, multi-primitives, indices/normals/UV, quaternion/matrix hierarchy, visibility, cache/lifetime, errors/retry, PNG/samplers, cwd and path confinement\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
