#include "AssetManager.hpp"
#include "scene/Lighting.hpp"
#include <cgltf.h>
#include <stb_image.h>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>

namespace ecosphere {
namespace {
constexpr std::size_t maxFileBytes = 32 * 1024 * 1024;
constexpr std::size_t maxElements = 1000000;

using GltfData = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;
using MeshList = std::vector<std::vector<std::size_t>>;

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

bool isUnlitExtension(const char* name) {
    return std::strcmp(name, "KHR_materials_unlit") == 0;
}

// ---- Lectura del archivo ----

std::vector<unsigned char> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(bool(input), "Cannot open model: " + path.string());
    const auto size = input.tellg();
    require(size > 0 && size <= static_cast<std::streamoff>(maxFileBytes),
            "Empty/oversized GLB (limit 32 MiB)");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    require(bool(input.read(reinterpret_cast<char*>(bytes.data()), size)), "Cannot read GLB");
    return bytes;
}

GltfData parseGlb(const std::vector<unsigned char>& bytes) {
    cgltf_options options{};
    options.type = cgltf_file_type_glb;
    cgltf_data* raw = nullptr;
    const auto parsed = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
    GltfData data(raw, cgltf_free);
    require(parsed == cgltf_result_success,
            "Corrupt/incompatible GLB, cgltf parse code=" + std::to_string(parsed));
    return data;
}

void checkAccessor(const cgltf_accessor& accessor) {
    require(!accessor.is_sparse, "Unsupported sparse accessor");
    require(accessor.buffer_view && accessor.buffer_view->buffer, "Accessor requires embedded buffer view");
    const auto elementSize = cgltf_calc_size(accessor.type, accessor.component_type);
    const auto& view = *accessor.buffer_view;
    require(accessor.count > 0 && accessor.count <= maxElements && elementSize > 0 &&
            accessor.stride >= elementSize && accessor.stride <= 256,
            "Invalid/oversized accessor count, type or stride");
    require(accessor.offset <= view.size && elementSize <= view.size - accessor.offset &&
            accessor.count - 1 <= (view.size - accessor.offset - elementSize) / accessor.stride,
            "Accessor exceeds buffer view");
    const auto alignment = cgltf_component_size(accessor.component_type);
    require(alignment && (view.offset + accessor.offset) % alignment == 0 && accessor.stride % alignment == 0,
            "Misaligned accessor");
}

// revisa lo que el cargador soporta antes de leer datos
void checkDocument(cgltf_data& data) {
    require(data.asset.version && std::strcmp(data.asset.version, "2.0") == 0, "Only glTF 2.0 supported");
    require(data.buffers_count == 1 && !data.buffers[0].uri && data.bin &&
            data.buffers[0].size <= data.bin_size,
            "GLB requires one embedded BIN buffer; external/data URIs unsupported");
    // el buffer apunta al bloque BIN que ya está en memoria
    data.buffers[0].data = const_cast<void*>(data.bin);
    require(!data.animations_count && !data.skins_count, "Unsupported animations/skins");
    for (std::size_t i = 0; i < data.extensions_required_count; ++i) {
        require(isUnlitExtension(data.extensions_required[i]),
                "Unsupported required extension: " + std::string(data.extensions_required[i]));
    }
    for (std::size_t i = 0; i < data.buffer_views_count; ++i) {
        const auto& view = data.buffer_views[i];
        require(view.buffer == &data.buffers[0] && !view.has_meshopt_compression &&
                view.offset <= view.buffer->size && view.size <= view.buffer->size - view.offset,
                "Invalid/compressed buffer view");
    }
    for (std::size_t i = 0; i < data.accessors_count; ++i) checkAccessor(data.accessors[i]);
    require(cgltf_validate(&data) == cgltf_result_success, "Invalid glTF accessors, indices or hierarchy");
    for (std::size_t i = 0; i < data.extensions_used_count; ++i) {
        require(isUnlitExtension(data.extensions_used[i]),
                "Unsupported used extension: " + std::string(data.extensions_used[i]));
    }
    require(data.images_count <= 64 && data.nodes_count <= 4096, "Too many images/nodes");
}

// ---- Imágenes y texturas ----

bool isPngOrJpeg(const char* mimeType) {
    return std::strcmp(mimeType, "image/png") == 0 || std::strcmp(mimeType, "image/jpeg") == 0;
}

void loadImages(const cgltf_data& data, Model& model) {
    std::size_t decodedPixels = 0;
    for (std::size_t i = 0; i < data.images_count; ++i) {
        const auto& source = data.images[i];
        require(!source.uri && source.buffer_view && source.mime_type && isPngOrJpeg(source.mime_type),
                "Only embedded PNG/JPEG images supported");
        const auto& view = *source.buffer_view;
        const auto* encoded = static_cast<const unsigned char*>(view.buffer->data) + view.offset;
        const auto encodedSize = static_cast<int>(view.size);
        Image image;
        int channels = 0;
        require(stbi_info_from_memory(encoded, encodedSize, &image.width, &image.height, &channels) &&
                image.width > 0 && image.height > 0 && image.width <= 4096 && image.height <= 4096,
                "Invalid/oversized image (limit 4096x4096)");
        decodedPixels += static_cast<std::size_t>(image.width) * image.height;
        require(decodedPixels <= 16 * 1024 * 1024, "Model exceeds 16 million decoded image pixels");
        // decodificar siempre a RGBA (4 canales)
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(encoded, encodedSize, &image.width, &image.height, &channels, 4),
            stbi_image_free);
        require(bool(pixels), "Cannot decode embedded PNG/JPEG");
        const auto byteCount = static_cast<std::size_t>(image.width) * image.height * 4;
        image.rgba.assign(pixels.get(), pixels.get() + byteCount);
        model.images.push_back(std::move(image));
    }
}

void loadTextures(const cgltf_data& data, Model& model) {
    for (std::size_t i = 0; i < data.textures_count; ++i) {
        const auto& source = data.textures[i];
        require(source.image && !source.has_basisu && !source.has_webp, "Texture needs PNG/JPEG source");
        TextureData texture;
        texture.image = static_cast<std::size_t>(source.image - data.images);
        if (source.sampler) {
            const auto& sampler = *source.sampler;
            if (sampler.min_filter) texture.minFilter = sampler.min_filter;
            if (sampler.mag_filter) texture.magFilter = sampler.mag_filter;
            texture.wrapS = sampler.wrap_s;
            texture.wrapT = sampler.wrap_t;
        }
        // 9728 NEAREST, 9729 LINEAR, 9984..9987 con mipmaps
        require(texture.magFilter == 9728 || texture.magFilter == 9729, "Invalid magnification filter");
        require(texture.minFilter == 9728 || texture.minFilter == 9729 ||
                (texture.minFilter >= 9984 && texture.minFilter <= 9987),
                "Invalid minification filter");
        // 10497 REPEAT, 33071 CLAMP_TO_EDGE, 33648 MIRRORED_REPEAT
        for (int wrap : {texture.wrapS, texture.wrapT}) {
            require(wrap == 10497 || wrap == 33071 || wrap == 33648, "Invalid texture wrapping");
        }
        model.textures.push_back(texture);
    }
}

// ---- Mallas (una por primitiva) ----

struct PrimitiveAttributes {
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* uv = nullptr;
};

struct MeshTotals {
    std::size_t vertices = 0;
    std::size_t indices = 0;
};

bool isFloat(const cgltf_accessor& accessor) {
    return accessor.component_type == cgltf_component_type_r_32f && !accessor.normalized;
}

bool isNormalizedByteOrShort(const cgltf_accessor& accessor) {
    return (accessor.component_type == cgltf_component_type_r_8u ||
            accessor.component_type == cgltf_component_type_r_16u) && accessor.normalized;
}

PrimitiveAttributes findAttributes(const cgltf_primitive& primitive) {
    PrimitiveAttributes found;
    for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
        const auto& attribute = primitive.attributes[i];
        const bool firstSet = attribute.index == 0;
        if (attribute.type == cgltf_attribute_type_position && firstSet) {
            found.positions = attribute.data;
        } else if (attribute.type == cgltf_attribute_type_normal && firstSet) {
            found.normals = attribute.data;
        } else if (attribute.type == cgltf_attribute_type_texcoord && firstSet) {
            found.uv = attribute.data;
        } else {
            const std::string name = attribute.name ? attribute.name : "unknown";
            require(false, "Unsupported vertex attribute: " + name);
        }
    }
    require(found.positions && isFloat(*found.positions), "POSITION must be FLOAT VEC3");
    if (found.normals) require(isFloat(*found.normals), "NORMAL must be FLOAT VEC3");
    if (found.uv) {
        require(isFloat(*found.uv) || isNormalizedByteOrShort(*found.uv),
                "TEXCOORD_0 must be FLOAT or normalized unsigned byte/short");
    }
    return found;
}

void readAttribute(const cgltf_accessor* accessor, cgltf_type type, std::size_t vertexCount,
                   std::size_t vertexIndex, float* out, std::size_t components) {
    require(accessor && accessor->type == type && accessor->count == vertexCount,
            "Attribute type/count mismatch");
    require(cgltf_accessor_read_float(accessor, vertexIndex, out, components),
            "Cannot read vertex attribute");
    for (std::size_t c = 0; c < components; ++c) {
        require(std::isfinite(out[c]), "Non-finite vertex attribute");
    }
}

void readVertices(const PrimitiveAttributes& attributes, Mesh& mesh) {
    const auto count = attributes.positions->count;
    mesh.vertices.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto& vertex = mesh.vertices[i];
        readAttribute(attributes.positions, cgltf_type_vec3, count, i, glm::value_ptr(vertex.position), 3);
        if (attributes.normals) {
            readAttribute(attributes.normals, cgltf_type_vec3, count, i, glm::value_ptr(vertex.normal), 3);
        }
        if (attributes.uv) {
            readAttribute(attributes.uv, cgltf_type_vec2, count, i, glm::value_ptr(vertex.uv), 2);
        }
    }
}

// sin índices en el GLB se usan 0, 1, 2, ...
void readIndices(const cgltf_primitive& primitive, std::size_t vertexCount, MeshTotals& totals, Mesh& mesh) {
    const auto count = primitive.indices ? primitive.indices->count : vertexCount;
    totals.indices += count;
    require(count % 3 == 0 && totals.indices <= maxElements * 3, "Invalid/oversized triangle indices");
    if (primitive.indices) require(!primitive.indices->normalized, "Normalized indices unsupported");
    mesh.indices.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
        require(index < vertexCount, "Index outside vertex array");
        mesh.indices.push_back(static_cast<std::uint32_t>(index));
    }
}

AlphaMode toAlphaMode(cgltf_alpha_mode mode) {
    if (mode == cgltf_alpha_mode_blend) return AlphaMode::blend;
    if (mode == cgltf_alpha_mode_mask) return AlphaMode::mask;
    return AlphaMode::opaque;
}

bool usesOnlyBaseColor(const cgltf_material& material) {
    return !material.normal_texture.texture && !material.occlusion_texture.texture &&
           !material.emissive_texture.texture &&
           !material.pbr_metallic_roughness.metallic_roughness_texture.texture &&
           !material.has_pbr_specular_glossiness && !material.has_clearcoat && !material.has_transmission &&
           !material.has_volume && !material.has_ior && !material.has_specular && !material.has_sheen &&
           !material.has_emissive_strength && !material.has_iridescence &&
           !material.has_diffuse_transmission && !material.has_anisotropy && !material.has_dispersion && !material.extensions_count;
}

void readMaterial(const cgltf_data& data, const cgltf_material& material, bool hasUv, Mesh& mesh,
                  Model& model) {
    const auto mode = material.alpha_mode;
    require(mode == cgltf_alpha_mode_opaque || mode == cgltf_alpha_mode_mask ||
            mode == cgltf_alpha_mode_blend,
            "Invalid alphaMode");
    const auto& pbr = material.pbr_metallic_roughness;
    mesh.material.alphaMode = toAlphaMode(mode);
    mesh.material.unlit = material.unlit;
    mesh.material.doubleSided = material.double_sided;
    mesh.material.alphaCutoff = material.alpha_cutoff;
    mesh.material.metallic = pbr.metallic_factor;
    mesh.material.roughness = pbr.roughness_factor;
    validateMaterial(mesh.material);
    require(usesOnlyBaseColor(material), "Unsupported material texture/lobe; only base color supported");
    mesh.color = glm::make_vec4(pbr.base_color_factor);
    for (int c = 0; c < 4; ++c) {
        const float channel = mesh.color[c];
        require(std::isfinite(channel) && channel >= 0 && channel <= 1, "Invalid base color");
    }
    const auto& baseTexture = pbr.base_color_texture;
    if (baseTexture.texture) {
        require(hasUv && baseTexture.texcoord == 0 && !baseTexture.has_transform,
                "Base color texture requires TEXCOORD_0 without transform");
        mesh.texture = static_cast<int>(baseTexture.texture - data.textures);
    }
    const auto* emissive = material.emissive_factor;
    if (emissive[0] || emissive[1] || emissive[2]) {
        model.warnings.emplace_back(
            "Emissive factor ignored; only base color, metallic/roughness factors supported");
    }
}

Mesh loadPrimitive(const cgltf_data& data, const cgltf_primitive& primitive, MeshTotals& totals,
                   Model& model) {
    require(primitive.type == cgltf_primitive_type_triangles && !primitive.has_draco_mesh_compression &&
            !primitive.targets_count,
            "Unsupported primitive: need triangles without Draco/morph targets");
    const auto attributes = findAttributes(primitive);
    const auto vertexCount = attributes.positions->count;
    totals.vertices += vertexCount;
    require(totals.vertices <= maxElements, "Model exceeds one million vertices");
    // valores por defecto de glTF
    Mesh mesh;
    mesh.material.unlit = false;
    mesh.material.metallic = 1;
    mesh.material.roughness = 1;
    readVertices(attributes, mesh);
    readIndices(primitive, vertexCount, totals, mesh);
    if (primitive.material) readMaterial(data, *primitive.material, attributes.uv != nullptr, mesh, model);
    return mesh;
}

// devuelve, por cada malla glTF, los índices de sus primitivas en model.meshes
MeshList loadMeshes(const cgltf_data& data, Model& model) {
    MeshList meshPrimitives(data.meshes_count);
    MeshTotals totals;
    for (std::size_t m = 0; m < data.meshes_count; ++m) {
        const auto& source = data.meshes[m];
        for (std::size_t p = 0; p < source.primitives_count; ++p) {
            auto mesh = loadPrimitive(data, source.primitives[p], totals, model);
            meshPrimitives[m].push_back(model.meshes.size());
            model.meshes.push_back(std::move(mesh));
        }
    }
    require(!model.meshes.empty(), "GLB has no triangle meshes");
    return meshPrimitives;
}

// ---- Jerarquía de nodos ----

using PendingNode = std::pair<const cgltf_node*, int>;

// raíces en orden inverso para sacarlas de la pila en orden
std::vector<PendingNode> rootNodes(const cgltf_data& data) {
    std::vector<PendingNode> pending;
    const cgltf_scene* selected = data.scene ? data.scene : (data.scenes_count ? &data.scenes[0] : nullptr);
    if (selected) {
        for (std::size_t i = selected->nodes_count; i > 0; --i) {
            pending.emplace_back(selected->nodes[i - 1], -1);
        }
        return pending;
    }
    for (std::size_t i = data.nodes_count; i > 0; --i) {
        if (!data.nodes[i - 1].parent) pending.emplace_back(&data.nodes[i - 1], -1);
    }
    return pending;
}

void checkNode(const cgltf_node& source) {
    require(!source.skin && !source.has_mesh_gpu_instancing, "Unsupported skin/GPU instancing");
    require(!(source.has_matrix && (source.has_translation || source.has_rotation || source.has_scale)),
            "Node mixes matrix and TRS");
    if (source.has_rotation) {
        float norm = 0;
        for (float q : source.rotation) norm += q * q;
        require(std::isfinite(norm) && std::abs(norm - 1) < 0.001f, "Invalid node quaternion");
    }
}

// matriz local del nodo, debe ser finita y afín
glm::mat4 localMatrix(const cgltf_node& source) {
    glm::mat4 local{1};
    cgltf_node_transform_local(&source, glm::value_ptr(local));
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) require(std::isfinite(local[c][r]), "Non-finite node transform");
    }
    require(local[0][3] == 0 && local[1][3] == 0 && local[2][3] == 0 && local[3][3] == 1,
            "Node matrix must be affine");
    (void)normalMatrix(local);
    return local;
}

void loadNodes(const cgltf_data& data, const MeshList& meshPrimitives, Model& model) {
    auto pending = rootNodes(data);
    std::set<const cgltf_node*> visited;
    while (!pending.empty()) {
        const auto [source, parent] = pending.back();
        pending.pop_back();
        require(visited.insert(source).second, "Duplicate/cyclic node hierarchy");
        checkNode(*source);
        ModelNode node;
        node.name = source->name ? source->name : "node";
        node.parent = parent;
        node.local = localMatrix(*source);
        if (source->mesh) {
            node.meshes = meshPrimitives.at(static_cast<std::size_t>(source->mesh - data.meshes));
        }
        const auto nodeIndex = static_cast<int>(model.nodes.size());
        model.nodes.push_back(std::move(node));
        if (source->camera || source->light) {
            model.warnings.emplace_back("Imported camera/light ignored; using Phase 1 camera");
        }
        for (std::size_t i = source->children_count; i > 0; --i) {
            pending.emplace_back(source->children[i - 1], nodeIndex);
        }
    }
    require(!model.nodes.empty(), "GLB scene is empty");
}

// GLB -> Model en CPU: imágenes, texturas, mallas y nodos
std::shared_ptr<const Model> importGlb(const std::filesystem::path& path) {
    require(path.extension() == ".glb", "Unsupported format: use self-contained .glb (glTF 2.0)");
    const auto bytes = readFile(path);
    const auto data = parseGlb(bytes);
    checkDocument(*data);
    auto model = std::make_shared<Model>();
    model->warnings.emplace_back(
        "Phase 3: metallic/roughness approximated by Blinn-Phong + static environment; not full glTF PBR");
    loadImages(*data, *model);
    loadTextures(*data, *model);
    const auto meshPrimitives = loadMeshes(*data, *model);
    loadNodes(*data, meshPrimitives, *model);
    return model;
}
}

AssetManager::AssetManager(const std::filesystem::path& root) : root_(std::filesystem::canonical(root)) {
    require(std::filesystem::is_directory(root_), "Asset root must be a directory");
}

std::filesystem::path AssetManager::resolve(const std::filesystem::path& relative) const {
    require(!relative.empty() && !relative.is_absolute() && !relative.has_root_name() &&
            !relative.has_root_directory(),
            "Asset path must be relative");
    const auto path = std::filesystem::weakly_canonical(root_ / relative);
    const auto suffix = path.lexically_relative(root_);
    require(!suffix.empty() && *suffix.begin() != ".." && !suffix.is_absolute(),
            "Asset path escapes root: " + relative.string());
    return path;
}

std::shared_ptr<const Model> AssetManager::loadModel(const std::filesystem::path& relative) {
    const auto path = resolve(relative);
    const auto found = models_.find(path);
    if (found != models_.end()) return found->second;
    try {
        auto model = importGlb(path);
        // solo se guarda en caché si cargó completo
        models_.emplace(path, model);
        return model;
    } catch (const std::exception& error) {
        throw std::runtime_error("Model " + path.string() + ": " + error.what());
    }
}
}
