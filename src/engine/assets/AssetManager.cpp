#include "AssetManager.hpp"
#include "scene/Lighting.hpp"
#include <cgltf.h>
#include <stb_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>

namespace ecosphere {
namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
constexpr std::size_t maxFileBytes = 32 * 1024 * 1024;
constexpr std::size_t maxElements = 1000000;
std::vector<unsigned char> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(bool(input), "Cannot open model: " + path.string());
    const auto size = input.tellg();
    require(size > 0 && size <= static_cast<std::streamoff>(maxFileBytes), "Empty/oversized GLB (limit 32 MiB)");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    require(bool(input.read(reinterpret_cast<char*>(bytes.data()), size)), "Cannot read GLB");
    return bytes;
}
void checkAccessor(const cgltf_accessor& a) {
    require(!a.is_sparse, "Unsupported sparse accessor");
    require(a.buffer_view && a.buffer_view->buffer, "Accessor requires embedded buffer view");
    const auto element = cgltf_calc_size(a.type, a.component_type);
    const auto& view = *a.buffer_view;
    require(a.count > 0 && a.count <= maxElements && element > 0 && a.stride >= element && a.stride <= 256,
            "Invalid/oversized accessor count, type or stride");
    require(a.offset <= view.size && element <= view.size - a.offset &&
            a.count - 1 <= (view.size - a.offset - element) / a.stride,
            "Accessor exceeds buffer view");
    const auto alignment = cgltf_component_size(a.component_type);
    require(alignment && (view.offset + a.offset) % alignment == 0 && a.stride % alignment == 0,
            "Misaligned accessor");
}
void readAttribute(const cgltf_accessor* a, cgltf_type type, std::size_t count, std::size_t i, float* out, std::size_t n) {
    require(a && a->type == type && a->count == count, "Attribute type/count mismatch");
    require(cgltf_accessor_read_float(a, i, out, n), "Cannot read vertex attribute");
    for (std::size_t c = 0; c < n; ++c) require(std::isfinite(out[c]), "Non-finite vertex attribute");
}
std::shared_ptr<const Model> importGlb(const std::filesystem::path& path) {
    require(path.extension() == ".glb", "Unsupported format: use self-contained .glb (glTF 2.0)");
    auto bytes = readFile(path);
    cgltf_options options{};
    options.type = cgltf_file_type_glb;
    cgltf_data* raw = nullptr;
    const auto parsed = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    require(parsed == cgltf_result_success, "Corrupt/incompatible GLB, cgltf parse code=" + std::to_string(parsed));
    require(data->asset.version && std::strcmp(data->asset.version, "2.0") == 0, "Only glTF 2.0 supported");
    require(data->buffers_count == 1 && !data->buffers[0].uri && data->bin && data->buffers[0].size <= data->bin_size,
            "GLB requires one embedded BIN buffer; external/data URIs unsupported");
    data->buffers[0].data = const_cast<void*>(data->bin); // Prestado; bytes sobrevive a cgltf_free.
    require(!data->animations_count && !data->skins_count, "Unsupported animations/skins");
    for (std::size_t i = 0; i < data->extensions_required_count; ++i)
        require(std::strcmp(data->extensions_required[i], "KHR_materials_unlit") == 0,
                "Unsupported required extension: " + std::string(data->extensions_required[i]));
    for (std::size_t i = 0; i < data->buffer_views_count; ++i) {
        const auto& v = data->buffer_views[i];
        require(v.buffer == &data->buffers[0] && !v.has_meshopt_compression && v.offset <= v.buffer->size &&
                v.size <= v.buffer->size - v.offset, "Invalid/compressed buffer view");
    }
    for (std::size_t i = 0; i < data->accessors_count; ++i) checkAccessor(data->accessors[i]);
    require(cgltf_validate(data.get()) == cgltf_result_success, "Invalid glTF accessors, indices or hierarchy");
    auto model = std::make_shared<Model>();
    model->warnings.emplace_back("Phase 3: metallic/roughness approximated by Blinn-Phong + static environment; not full glTF PBR");
    for (std::size_t i = 0; i < data->extensions_used_count; ++i)
        require(std::strcmp(data->extensions_used[i], "KHR_materials_unlit") == 0,
                "Unsupported used extension: " + std::string(data->extensions_used[i]));
    require(data->images_count <= 64 && data->nodes_count <= 4096, "Too many images/nodes");
    std::size_t decodedPixels = 0;
    for (std::size_t i = 0; i < data->images_count; ++i) {
        const auto& source = data->images[i];
        require(!source.uri && source.buffer_view && source.mime_type &&
                (std::strcmp(source.mime_type, "image/png") == 0 || std::strcmp(source.mime_type, "image/jpeg") == 0),
                "Only embedded PNG/JPEG images supported");
        const auto& v = *source.buffer_view;
        const auto* encoded = static_cast<const unsigned char*>(v.buffer->data) + v.offset;
        Image image;
        int channels = 0;
        require(stbi_info_from_memory(encoded, static_cast<int>(v.size), &image.width, &image.height, &channels) &&
                image.width > 0 && image.height > 0 && image.width <= 4096 && image.height <= 4096,
                "Invalid/oversized image (limit 4096x4096)");
        decodedPixels += static_cast<std::size_t>(image.width) * image.height;
        require(decodedPixels <= 16 * 1024 * 1024, "Model exceeds 16 million decoded image pixels");
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(encoded, static_cast<int>(v.size), &image.width, &image.height, &channels, 4), stbi_image_free);
        require(bool(pixels), "Cannot decode embedded PNG/JPEG");
        image.rgba.assign(pixels.get(), pixels.get() + static_cast<std::size_t>(image.width) * image.height * 4);
        model->images.push_back(std::move(image));
    }
    for (std::size_t i = 0; i < data->textures_count; ++i) {
        const auto& source = data->textures[i];
        require(source.image && !source.has_basisu && !source.has_webp, "Texture needs PNG/JPEG source");
        TextureData texture;
        texture.image = static_cast<std::size_t>(source.image - data->images);
        if (source.sampler) {
            const auto& s = *source.sampler;
            if (s.min_filter) texture.minFilter = s.min_filter;
            if (s.mag_filter) texture.magFilter = s.mag_filter;
            texture.wrapS = s.wrap_s; texture.wrapT = s.wrap_t;
        }
        require(texture.magFilter == 9728 || texture.magFilter == 9729, "Invalid magnification filter");
        require(texture.minFilter == 9728 || texture.minFilter == 9729 ||
                (texture.minFilter >= 9984 && texture.minFilter <= 9987), "Invalid minification filter");
        for (int wrap : {texture.wrapS, texture.wrapT})
            require(wrap == 10497 || wrap == 33071 || wrap == 33648, "Invalid texture wrapping");
        model->textures.push_back(texture);
    }
    std::vector<std::vector<std::size_t>> meshPrimitives(data->meshes_count);
    std::size_t totalVertices = 0, totalIndices = 0;
    for (std::size_t m = 0; m < data->meshes_count; ++m) {
        const auto& source = data->meshes[m];
        for (std::size_t p = 0; p < source.primitives_count; ++p) {
            const auto& primitive = source.primitives[p];
            require(primitive.type == cgltf_primitive_type_triangles && !primitive.has_draco_mesh_compression &&
                    !primitive.targets_count, "Unsupported primitive: need triangles without Draco/morph targets");
            const cgltf_accessor *positions = nullptr, *normals = nullptr, *uv = nullptr;
            for (std::size_t a = 0; a < primitive.attributes_count; ++a) {
                const auto& attribute = primitive.attributes[a];
                if (attribute.type == cgltf_attribute_type_position && attribute.index == 0) positions = attribute.data;
                else if (attribute.type == cgltf_attribute_type_normal && attribute.index == 0) normals = attribute.data;
                else if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0) uv = attribute.data;
                else require(false, "Unsupported vertex attribute: " + std::string(attribute.name ? attribute.name : "unknown"));
            }
            require(positions && positions->component_type == cgltf_component_type_r_32f && !positions->normalized,
                    "POSITION must be FLOAT VEC3");
            if (normals) require(normals->component_type == cgltf_component_type_r_32f && !normals->normalized, "NORMAL must be FLOAT VEC3");
            if (uv) require((uv->component_type == cgltf_component_type_r_32f && !uv->normalized) ||
                ((uv->component_type == cgltf_component_type_r_8u || uv->component_type == cgltf_component_type_r_16u) && uv->normalized),
                "TEXCOORD_0 must be FLOAT or normalized unsigned byte/short");
            totalVertices += positions->count;
            require(totalVertices <= maxElements, "Model exceeds one million vertices");
            Mesh mesh;
            mesh.material.unlit = false;
            mesh.material.metallic = 1; // Valores predeterminados glTF.
            mesh.material.roughness = 1;
            mesh.vertices.resize(positions->count);
            for (std::size_t i = 0; i < positions->count; ++i) {
                auto& vertex = mesh.vertices[i];
                readAttribute(positions, cgltf_type_vec3, positions->count, i, glm::value_ptr(vertex.position), 3);
                if (normals) readAttribute(normals, cgltf_type_vec3, positions->count, i, glm::value_ptr(vertex.normal), 3);
                if (uv) readAttribute(uv, cgltf_type_vec2, positions->count, i, glm::value_ptr(vertex.uv), 2);
            }
            const auto count = primitive.indices ? primitive.indices->count : positions->count;
            totalIndices += count;
            require(count % 3 == 0 && totalIndices <= maxElements * 3, "Invalid/oversized triangle indices");
            if (primitive.indices) require(!primitive.indices->normalized, "Normalized indices unsupported");
            mesh.indices.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const auto index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
                require(index < positions->count, "Index outside vertex array");
                mesh.indices.push_back(static_cast<std::uint32_t>(index));
            }
            if (primitive.material) {
                const auto& material = *primitive.material;
                require(material.alpha_mode == cgltf_alpha_mode_opaque || material.alpha_mode == cgltf_alpha_mode_mask ||
                        material.alpha_mode == cgltf_alpha_mode_blend, "Invalid alphaMode");
                mesh.material.alphaMode = material.alpha_mode == cgltf_alpha_mode_blend ? AlphaMode::blend :
                    (material.alpha_mode == cgltf_alpha_mode_mask ? AlphaMode::mask : AlphaMode::opaque);
                mesh.material.unlit = material.unlit;
                mesh.material.doubleSided = material.double_sided;
                mesh.material.alphaCutoff = material.alpha_cutoff;
                mesh.material.metallic = material.pbr_metallic_roughness.metallic_factor;
                mesh.material.roughness = material.pbr_metallic_roughness.roughness_factor;
                validateMaterial(mesh.material);
                require(!material.normal_texture.texture && !material.occlusion_texture.texture && !material.emissive_texture.texture &&
                        !material.pbr_metallic_roughness.metallic_roughness_texture.texture && !material.has_pbr_specular_glossiness &&
                        !material.has_clearcoat && !material.has_transmission && !material.has_volume && !material.has_ior &&
                        !material.has_specular && !material.has_sheen && !material.has_emissive_strength && !material.has_iridescence &&
                        !material.has_diffuse_transmission && !material.has_anisotropy && !material.has_dispersion && !material.extensions_count,
                        "Unsupported material texture/lobe; only base color supported");
                mesh.color = glm::make_vec4(material.pbr_metallic_roughness.base_color_factor);
                for (int c = 0; c < 4; ++c) require(std::isfinite(mesh.color[c]) && mesh.color[c] >= 0 && mesh.color[c] <= 1, "Invalid base color");
                const auto& view = material.pbr_metallic_roughness.base_color_texture;
                if (view.texture) {
                    require(uv && view.texcoord == 0 && !view.has_transform, "Base color texture requires TEXCOORD_0 without transform");
                    mesh.texture = static_cast<int>(view.texture - data->textures);
                }
                if (material.emissive_factor[0] || material.emissive_factor[1] || material.emissive_factor[2])
                    model->warnings.emplace_back("Emissive factor ignored; only base color, metallic/roughness factors supported");
            }
            meshPrimitives[m].push_back(model->meshes.size());
            model->meshes.push_back(std::move(mesh));
        }
    }
    require(!model->meshes.empty(), "GLB has no triangle meshes");
    std::vector<std::pair<const cgltf_node*, int>> pending;
    const auto* selected = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    if (selected) {
        for (std::size_t i = selected->nodes_count; i > 0; --i) pending.emplace_back(selected->nodes[i-1], -1);
    } else {
        for (std::size_t i = data->nodes_count; i > 0; --i) if (!data->nodes[i-1].parent) pending.emplace_back(&data->nodes[i-1], -1);
    }
    std::set<const cgltf_node*> visited;
    while (!pending.empty()) {
        const auto [source, parent] = pending.back(); pending.pop_back();
        require(visited.insert(source).second, "Duplicate/cyclic node hierarchy");
        require(!source->skin && !source->has_mesh_gpu_instancing, "Unsupported skin/GPU instancing");
        require(!(source->has_matrix && (source->has_translation || source->has_rotation || source->has_scale)), "Node mixes matrix and TRS");
        if (source->has_rotation) {
            float norm = 0;
            for (float q : source->rotation) norm += q*q;
            require(std::isfinite(norm) && std::abs(norm - 1) < 0.001f, "Invalid node quaternion");
        }
        ModelNode node;
        node.name = source->name ? source->name : "node";
        node.parent = parent;
        cgltf_node_transform_local(source, glm::value_ptr(node.local));
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
            require(std::isfinite(node.local[c][r]), "Non-finite node transform");
        require(node.local[0][3] == 0 && node.local[1][3] == 0 && node.local[2][3] == 0 && node.local[3][3] == 1,
                "Node matrix must be affine");
        (void)normalMatrix(node.local);
        if (source->mesh) node.meshes = meshPrimitives.at(static_cast<std::size_t>(source->mesh - data->meshes));
        const auto nodeIndex = static_cast<int>(model->nodes.size());
        model->nodes.push_back(std::move(node));
        if (source->camera || source->light) model->warnings.emplace_back("Imported camera/light ignored; using Phase 1 camera");
        for (std::size_t i = source->children_count; i > 0; --i) pending.emplace_back(source->children[i-1], nodeIndex);
    }
    require(!model->nodes.empty(), "GLB scene is empty");
    return model;
}
}
AssetManager::AssetManager(const std::filesystem::path& root) : root_(std::filesystem::canonical(root)) {
    require(std::filesystem::is_directory(root_), "Asset root must be a directory");
}
std::filesystem::path AssetManager::resolve(const std::filesystem::path& relative) const {
    require(!relative.empty() && !relative.is_absolute() && !relative.has_root_name() && !relative.has_root_directory(), "Asset path must be relative");
    const auto path = std::filesystem::weakly_canonical(root_ / relative);
    const auto suffix = path.lexically_relative(root_);
    require(!suffix.empty() && *suffix.begin() != ".." && !suffix.is_absolute(), "Asset path escapes root: " + relative.string());
    return path;
}
std::shared_ptr<const Model> AssetManager::loadModel(const std::filesystem::path& relative) {
    const auto path = resolve(relative);
    const auto found = models_.find(path);
    if (found != models_.end()) return found->second;
    try {
        auto model = importGlb(path);
        models_.emplace(path, model); // Solo publicar en caché una carga completa.
        return model;
    } catch (const std::exception& error) {
        throw std::runtime_error("Model " + path.string() + ": " + error.what());
    }
}
}
