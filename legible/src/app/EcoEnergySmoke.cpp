#include "EcoEnergySmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;

struct SmokeContext {
    const std::filesystem::path& directory;
    int width;
    int height;
    ContentRegistry& registry;
    Camera& camera;
    const LightingSettings& lighting;
    const PlazaRender& render;
    const std::function<bool()>& present;
};

struct EnergyStats {
    std::map<std::string, ContentPlacement> unique;
    std::map<std::string, unsigned> counts;
    std::set<const Model*> resources;
    std::set<const Model*> renderModels;
    std::set<const Model*> cpuModels;
    std::size_t sceneTriangles = 0;
    std::size_t geometryCount = 0;
    std::size_t renderGeometryCount = 0;
    std::size_t sourceTriangles = 0;
    std::size_t textureCount = 0;
    std::size_t cpuBytes = 0;
};

struct BenchmarkResult {
    double seconds = 0;
    std::vector<double> durations;
};

// caso de interacción: objeto, cámara y cambio que se le aplica
struct InteractionCase {
    const char* code;
    const char* instanceId;
    glm::vec3 camPos;
    bool testCarry;
    glm::vec3 deltaPos;
    glm::vec3 rotAxis;
    float rotDeg;
    float scaleFactor;
};

const std::array<InteractionCase, 6> interactionCases{{
    {"INT-ENERGY-001", "ENERGY-005-panel-adjustable", {-1.1f, 1.85f, 4.0f}, false,
        {0.32f, 0.24f, 0.0f}, {1.0f, 0.0f, 0.0f}, 24.0f, 1.12f},
    {"INT-ENERGY-002", "ENERGY-019-blade-demo", {5.3f, 1.55f, -3.7f}, false,
        {0.0f, 0.34f, 0.25f}, {0.0f, 1.0f, 0.0f}, 28.0f, 1.10f},
    {"INT-ENERGY-003", "ENERGY-011-battery-module", {5.2f, 1.25f, 3.9f}, false,
        {0.32f, 0.28f, 0.15f}, {0.0f, 1.0f, 0.0f}, -26.0f, 1.14f},
    {"INT-ENERGY-004", "ENERGY-023-generator-coil", {8.5f, 1.35f, -3.3f}, true,
        {0.45f, 0.20f, -0.35f}, {0.0f, 1.0f, 0.0f}, 0.0f, 1.0f},
    {"INT-ENERGY-005", "ENERGY-027-charge-connector", {-5.0f, 1.15f, 3.7f}, true,
        {-0.40f, 0.25f, -0.35f}, {0.0f, 1.0f, 0.0f}, 0.0f, 1.0f},
    {"INT-ENERGY-006", "ENERGY-026-ev-interactive", {-5.0f, 2.05f, 10.2f}, false,
        {0.48f, 0.0f, -0.70f}, {0.0f, 1.0f, 0.0f}, 18.0f, 1.05f},
}};

const std::set<std::string> animatedIds{
    "ENERGY-018-rotor-main", "ENERGY-018-rotor-secondary", "ENERGY-021-anemometer-main",
    "ENERGY-024-demo-fan", "ENERGY-005-tracker-wing", "ENERGY-026-ev-shuttle",
    "ENERGY-020-weathervane", "ENERGY-012-charge-indicator",
};

// tramos del recorrido: posición y punto al que se mira
const std::array<std::pair<glm::vec3, glm::vec3>, 8> route{{
    {{0.0f, 1.7f, 8.5f}, {0.0f, 1.6f, -6.0f}},
    {{0.0f, 1.7f, 2.5f}, {-4.5f, 1.6f, -6.5f}},
    {{-6.5f, 1.7f, -3.8f}, {-9.5f, 1.4f, -8.5f}},
    {{0.0f, 1.7f, -5.5f}, {5.5f, 5.0f, -13.5f}},
    {{6.8f, 1.7f, -4.0f}, {11.5f, 1.5f, -5.0f}},
    {{6.2f, 1.7f, 4.2f}, {8.8f, 1.2f, 1.2f}},
    {{-6.2f, 1.7f, 4.2f}, {-9.8f, 1.0f, 1.0f}},
    {{0.0f, 1.7f, 6.0f}, {0.0f, 1.5f, -8.0f}},
}};

void lookAt(Camera& camera, glm::vec3 position, glm::vec3 target) {
    const auto direction = target - position;
    camera.position = position;
    camera.yawDegrees = glm::degrees(std::atan2(direction.z, direction.x));
    const float horizontal = glm::length(glm::vec2(direction.x, direction.z));
    camera.pitchDegrees = glm::degrees(std::atan2(direction.y, horizontal));
}

// lee color y profundidad y guarda la imagen PPM
Pixels capture(const std::filesystem::path& path, int width, int height) {
    Pixels pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    for (float value : depth) {
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            throw std::runtime_error("Invalid EcoEnergy depth");
    }
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("EcoEnergy framebuffer readback failed");
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        const auto offset = static_cast<std::size_t>(y) * width * 3;
        const auto* row = reinterpret_cast<const char*>(pixels.data() + offset);
        file.write(row, width * 3);
    }
    return pixels;
}

std::size_t changed(const Pixels& before, const Pixels& after) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < before.size(); i += 3) {
        int delta = 0;
        for (int c = 0; c < 3; ++c) delta += std::abs(int(before[i + c]) - int(after[i + c]));
        count += delta > 12;
    }
    return count;
}

std::size_t triangles(const Model& model) {
    std::size_t count = 0;
    model.forEach([&](std::size_t index, const glm::mat4&) {
        const auto& mesh = model.meshes.at(index);
        count += (mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size()) / 3;
    });
    return count;
}

// memoria aproximada del modelo en CPU
std::size_t payload(const Model& model) {
    std::size_t bytes = sizeof(Model) + model.nodes.size() * sizeof(ModelNode)
                      + model.meshes.size() * sizeof(Mesh);
    for (const auto& mesh : model.meshes)
        bytes += mesh.vertices.size() * sizeof(Vertex) + mesh.indices.size() * sizeof(std::uint32_t);
    for (const auto& image : model.images) bytes += image.rgba.size();
    for (const auto& node : model.nodes) bytes += node.name.size() + node.meshes.size() * sizeof(std::size_t);
    return bytes;
}

// filas del CSV con posición y dos columnas de la matriz
void writePlacementRows(std::ostream& out, const std::string& label, double seconds,
    const std::vector<ContentPlacement>& placements, const std::set<std::string>& filter) {
    for (const auto& placement : placements) {
        if (!filter.empty() && !filter.count(placement.id)) continue;
        const auto& matrix = placement.world;
        out << label << ',' << seconds << ',' << placement.id << ','
            << matrix[3].x << ',' << matrix[3].y << ',' << matrix[3].z << ','
            << matrix[0].x << ',' << matrix[0].y << ',' << matrix[0].z << ','
            << matrix[1].x << ',' << matrix[1].y << ',' << matrix[1].z << '\n';
    }
}

void writeStatesHeader(std::ostream& out) {
    out << std::setprecision(7) << "stage,time_s,instance_id,x,y,z,m00,m01,m02,m10,m11,m12\n";
}

EnergyStats collectStats(const std::vector<ContentPlacement>& placements) {
    EnergyStats stats;
    for (const auto& placement : placements) {
        const auto id = placement.id.substr(0, 10);
        stats.unique.emplace(id, placement);
        ++stats.counts[id];
        stats.sceneTriangles += triangles(*placement.model);
        stats.resources.insert(placement.resource.get());
        stats.renderModels.insert(placement.model.get());
        stats.cpuModels.insert(placement.resource.get());
        stats.cpuModels.insert(placement.model.get());
    }
    if (stats.unique.size() != 31 || stats.resources.size() != 31)
        throw std::runtime_error("EcoEnergy requires 28 original and 3 shared resources");
    for (const auto* model : stats.resources) {
        stats.geometryCount += model->meshes.size();
        stats.textureCount += model->textures.size();
        stats.sourceTriangles += triangles(*model);
    }
    for (const auto* model : stats.renderModels) stats.renderGeometryCount += model->meshes.size();
    for (const auto* model : stats.cpuModels) stats.cpuBytes += payload(*model);
    return stats;
}

// ---- 1. Cada modelo aislado debe pintar pixeles ----
void checkModels(const SmokeContext& smoke, const EnergyStats& stats) {
    std::ofstream evidence(smoke.directory / "model-use.csv");
    evidence.exceptions(std::ios::badbit | std::ios::failbit);
    evidence << "model_id,instance_id,instances,triangles,meshes,textures,changed_pixels\n";
    for (const auto& item : stats.unique) {
        const auto& modelId = item.first;
        const auto& placement = item.second;
        glm::vec3 low(std::numeric_limits<float>::infinity());
        glm::vec3 high(-std::numeric_limits<float>::infinity());
        placement.model->forEach([&](std::size_t mesh, const glm::mat4& world) {
            for (const auto& vertex : placement.model->meshes.at(mesh).vertices) {
                const auto position = glm::vec3(world * glm::vec4(vertex.position, 1.0f));
                low = glm::min(low, position);
                high = glm::max(high, position);
            }
        }, placement.world);
        const auto center = (low + high) * 0.5f;
        const float radius = std::max({high.x - low.x, high.y - low.y, high.z - low.z}) * 0.5f;
        const auto front = glm::normalize(glm::vec3(placement.world * glm::vec4(1.2f, 1.4f, 2.2f, 0.0f)));
        lookAt(smoke.camera, center + front * (3.0f * std::max(radius, 0.45f)), center);
        smoke.render(smoke.lighting, false, {});
        const auto background = capture(smoke.directory / "asset-background.ppm", smoke.width, smoke.height);
        smoke.render(smoke.lighting, true, placement.id);
        const auto assetPath = smoke.directory / "assets" / (modelId + ".ppm");
        const auto pixels = capture(assetPath, smoke.width, smoke.height);
        const auto difference = changed(background, pixels);
        if (difference < 30)
            throw std::runtime_error("EcoEnergy model did not contribute visible pixels: " + modelId);
        evidence << modelId << ',' << placement.id << ',' << stats.counts.at(modelId) << ','
                 << triangles(*placement.model) << ',' << placement.model->meshes.size() << ','
                 << placement.model->textures.size() << ',' << difference << '\n';
        std::cout << "ECOENERGY MODEL PASS " << modelId << " pixels=" << difference << '\n';
    }
    evidence.close();
}

// ---- 2. Vistas generales y comparación de luz ----
void captureViews(const SmokeContext& smoke) {
    struct View {
        const char* name;
        glm::vec3 position, target;
    };
    const std::array<View, 7> views{{
        {"overview", {22.0f, 17.5f, 22.0f}, {0.0f, 2.2f, -3.5f}},
        {"entry", {0.0f, 1.7f, 6.0f}, {0.0f, 1.35f, -8.0f}},
        {"solar-zone", {-6.2f, 2.3f, -3.5f}, {-9.2f, 1.1f, -8.0f}},
        {"wind-zone", {6.8f, 2.6f, -1.8f}, {6.8f, 4.2f, -11.5f}},
        {"grid-zone", {5.5f, 2.3f, -1.2f}, {12.4f, 1.8f, -3.5f}},
        {"battery-zone", {6.8f, 1.9f, 5.4f}, {8.0f, 1.0f, 1.3f}},
        {"mobility-zone", {-6.8f, 2.1f, 8.8f}, {-9.2f, 0.9f, 0.8f}},
    }};
    Pixels overviewPixels;
    for (const auto& view : views) {
        lookAt(smoke.camera, view.position, view.target);
        smoke.render(smoke.lighting, true, {});
        const auto viewPath = smoke.directory / (std::string(view.name) + ".ppm");
        const auto pixels = capture(viewPath, smoke.width, smoke.height);
        if (std::string(view.name) == "overview") overviewPixels = pixels;
    }
    lookAt(smoke.camera, views[0].position, views[0].target);
    auto directOnly = smoke.lighting;
    directOnly.environment.reflectionEnabled = false;
    smoke.render(directOnly, true, {});
    const auto directPixels = capture(smoke.directory / "direct-only.ppm", smoke.width, smoke.height);
    auto envOnly = smoke.lighting;
    envOnly.directEnabled = false;
    smoke.render(envOnly, true, {});
    const auto envPixels = capture(smoke.directory / "environment-only.ppm", smoke.width, smoke.height);
    if (changed(overviewPixels, directPixels) < 30 || changed(directPixels, envPixels) < 100)
        throw std::runtime_error("EcoEnergy lighting/reflection comparison missing visible difference");
}

// ---- 3. Interacciones: rayo, resaltado y manipulación ----
void checkInteraction(const SmokeContext& smoke, InteractionSystem& inter, const glm::mat4& proj,
    const InteractionCase& item, std::ostream& csv) {
    const auto interactionShot = [&](const char* suffix) {
        smoke.render(smoke.lighting, true, {});
        const auto name = std::string(item.code) + "-" + suffix + ".ppm";
        return capture(smoke.directory / "interactions" / name, smoke.width, smoke.height);
    };
    inter.deselect();
    const auto& box = inter.object(item.instanceId).bounds;
    const auto localCenter = glm::vec4((box.minimum + box.maximum) * 0.5f, 1.0f);
    const auto worldCenter = glm::vec3(inter.world(item.instanceId) * localCenter);
    lookAt(smoke.camera, item.camPos, worldCenter);
    const auto beforeImg = interactionShot("before");
    writePlacementRows(csv, std::string(item.code) + "_before", 0.0, smoke.registry.placements(),
        {item.instanceId});

    const auto clip = proj * smoke.camera.view() * glm::vec4(worldCenter, 1.0f);
    const auto ray = cameraRay(smoke.camera.view(), proj, glm::vec2(clip) / clip.w);
    if (inter.pick(ray) != std::optional<std::string>(item.instanceId))
        throw std::runtime_error(std::string("EcoEnergy ray-pick failed for ") + item.instanceId);

    const auto selectedImg = interactionShot("selected");
    const auto selPixels = changed(beforeImg, selectedImg);
    if (selPixels < 25)
        throw std::runtime_error(std::string("Missing visible selection feedback for ") + item.instanceId);

    if (item.testCarry) {
        // tomar, mover y soltar
        const float dist = glm::length(worldCenter - ray.origin);
        inter.take(ray, dist);
        inter.follow(Ray{ray.origin + item.deltaPos, ray.direction});
        inter.drop();
    } else {
        const auto rotation = glm::angleAxis(glm::radians(item.rotDeg), item.rotAxis);
        inter.manipulate(item.deltaPos, rotation, item.scaleFactor);
    }
    const auto manipImg = interactionShot("manipulated");
    const auto manipPixels = changed(selectedImg, manipImg);
    if (manipPixels < 60)
        throw std::runtime_error(std::string("Missing visible manipulation delta for ") + item.instanceId);
    writePlacementRows(csv, std::string(item.code) + "_manipulated", 0.0, smoke.registry.placements(),
        {item.instanceId});

    inter.object(item.instanceId).manipulation = Pose{};
    inter.deselect();
    std::cout << "ECOENERGY INTERACTION PASS " << item.code << " (" << item.instanceId
              << ") select_px=" << selPixels << " manip_px=" << manipPixels << '\n';
}

// ---- 4. Animaciones capturadas en dos tiempos ----
void checkAnimations(const SmokeContext& smoke) {
    std::ofstream animCsv(smoke.directory / "animation-states.csv");
    animCsv.exceptions(std::ios::badbit | std::ios::failbit);
    writeStatesHeader(animCsv);

    struct AnimShot {
        const char* prefix;
        glm::vec3 pos, target;
    };
    const AnimShot shotSimple{"simple", {6.8f, 2.4f, -3.2f}, {6.8f, 4.5f, -11.5f}};
    const AnimShot shotTracker{"keyframe-tracker", {-2.6f, 2.0f, -5.2f}, {-4.2f, 1.65f, -8.2f}};
    const AnimShot shotEv{"keyframe-ev", {-7.8f, 2.2f, 5.2f}, {-11.2f, 0.8f, 0.4f}};
    const AnimShot shotVane{"procedural-vane", {7.6f, 1.85f, -6.9f}, {8.6f, 1.60f, -8.8f}};
    const AnimShot shotSoc{"procedural-soc", {8.5f, 1.35f, 3.3f}, {8.5f, 1.15f, 1.3f}};
    const auto takeShot = [&](const AnimShot& shot, const char* suffix) {
        lookAt(smoke.camera, shot.pos, shot.target);
        smoke.render(smoke.lighting, true, {});
        const auto name = std::string(shot.prefix) + "-" + suffix + ".ppm";
        return capture(smoke.directory / "animations" / name, smoke.width, smoke.height);
    };

    writePlacementRows(animCsv, "t0", 0.0, smoke.registry.placements(), animatedIds);
    const auto sim0 = takeShot(shotSimple, "t0");
    const auto trk0 = takeShot(shotTracker, "t0");
    const auto ev0 = takeShot(shotEv, "t0");
    const auto van0 = takeShot(shotVane, "t0");
    const auto soc0 = takeShot(shotSoc, "t0");

    smoke.registry.update(1.5);
    writePlacementRows(animCsv, "t1_5", 1.5, smoke.registry.placements(), animatedIds);
    const auto soc1 = takeShot(shotSoc, "t1");

    // tiempo total 3.5 s
    smoke.registry.update(2.0);
    writePlacementRows(animCsv, "t3_5", 3.5, smoke.registry.placements(), animatedIds);
    const auto sim1 = takeShot(shotSimple, "t1");
    const auto trk1 = takeShot(shotTracker, "t1");
    const auto ev1 = takeShot(shotEv, "t1");
    const auto van1 = takeShot(shotVane, "t1");
    animCsv.close();

    const std::array<std::pair<const char*, std::size_t>, 5> results{{
        {"simple", changed(sim0, sim1)},
        {"keyframe-tracker", changed(trk0, trk1)},
        {"keyframe-ev", changed(ev0, ev1)},
        {"procedural-vane", changed(van0, van1)},
        {"procedural-soc", changed(soc0, soc1)},
    }};
    for (const auto& [name, pixels] : results) {
        if (pixels < 40)
            throw std::runtime_error(std::string("EcoEnergy animation capture missing motion: ") + name);
        std::cout << "ECOENERGY ANIMATION PASS " << name << " changed_pixels=" << pixels << '\n';
    }
}

// ---- 5. Recorrido de 600 cuadros ----
void moveOnRoute(Camera& camera, int frame) {
    const float progress = float(frame) / 599.0f * float(route.size() - 1);
    const auto segment = std::min(static_cast<std::size_t>(progress), route.size() - 2);
    const float alpha = progress - float(segment);
    lookAt(camera, glm::mix(route[segment].first, route[segment + 1].first, alpha),
        glm::mix(route[segment].second, route[segment + 1].second, alpha));
}

BenchmarkResult runBenchmark(const SmokeContext& smoke) {
    // calentamiento
    for (int i = 0; i < 60; ++i) {
        smoke.registry.update(1.0 / 60.0);
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("EcoEnergy warmup interrupted");
    }

    std::ofstream frames(smoke.directory / "frames.csv");
    frames.exceptions(std::ios::badbit | std::ios::failbit);
    frames << "frame,seconds,x,y,z,frame_seconds\n";
    BenchmarkResult result;
    result.durations.reserve(600);
    glFinish();
    const double start = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        const double frameStart = glfwGetTime();
        moveOnRoute(smoke.camera, i);
        smoke.registry.update(1.0 / 60.0);
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("EcoEnergy benchmark interrupted");
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("EcoEnergy benchmark GL error");
        const double duration = glfwGetTime() - frameStart;
        result.durations.push_back(duration);
        const auto& position = smoke.camera.position;
        frames << i << ',' << glfwGetTime() - start << ',' << position.x << ',' << position.y << ','
               << position.z << ',' << duration << '\n';
    }
    glFinish();
    result.seconds = glfwGetTime() - start;
    frames.close();
    return result;
}

// segunda pasada guardando un cuadro de cada cinco
double recordTour(const SmokeContext& smoke) {
    const double recordingStart = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        moveOnRoute(smoke.camera, i);
        smoke.registry.update(1.0 / 60.0);
        smoke.render(smoke.lighting, true, {});
        if (i % 5 == 0) {
            const auto path = smoke.directory / "tour" / (std::to_string(1000 + i / 5) + ".ppm");
            capture(path, smoke.width, smoke.height);
        }
        if (smoke.present()) throw std::runtime_error("EcoEnergy tour recording interrupted");
    }
    return glfwGetTime() - recordingStart;
}

void writeMetrics(const SmokeContext& smoke, const EnergyStats& stats, std::size_t instances,
    std::size_t interactiveObjects, double loadSeconds, const BenchmarkResult& benchmark,
    double recordingSeconds) {
    std::ofstream metrics(smoke.directory / "metrics.csv");
    metrics.exceptions(std::ios::badbit | std::ios::failbit);
    metrics << "metric,value\n"
            << "unique_models," << stats.unique.size() << '\n'
            << "model_resources," << stats.resources.size() << '\n'
            << "instances," << instances << '\n'
            << "interactive_objects," << interactiveObjects << '\n'
            << "animated_entities," << animatedIds.size() << '\n'
            << "scene_triangles," << stats.sceneTriangles << '\n'
            << "source_triangles," << stats.sourceTriangles << '\n'
            << "source_geometries," << stats.geometryCount << '\n'
            << "render_geometries," << stats.renderGeometryCount << '\n'
            << "source_textures," << stats.textureCount << '\n'
            << "render_models," << stats.renderModels.size() << '\n'
            << "cpu_model_payload_bytes_lower_bound," << stats.cpuBytes << '\n'
            << "width," << smoke.width << '\n'
            << "height," << smoke.height << '\n'
            << "cpu_load_seconds," << loadSeconds << '\n'
            << "frames,600\n"
            << "seconds," << benchmark.seconds << '\n'
            << "fps_mean," << 600.0 / benchmark.seconds << '\n'
            << "recording_seconds," << recordingSeconds << '\n'
            << "frame_p95_ms," << benchmark.durations.at(569) * 1000.0 << '\n';
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) == 0)
            metrics << "process_rss_kib," << std::stoull(line.substr(6)) << '\n';
    }
#endif
    metrics.close();
}
}

void checkEcoEnergyFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present) {
    std::filesystem::create_directories(directory / "assets");
    std::filesystem::create_directories(directory / "interactions");
    std::filesystem::create_directories(directory / "animations");
    std::filesystem::create_directories(directory / "tour");
    const SmokeContext smoke{directory, width, height, registry, camera, lighting, render, present};
    const auto placements = registry.placements();
    const auto stats = collectStats(placements);

    checkModels(smoke, stats);
    captureViews(smoke);

    const auto proj = *camera.projection(width, height);
    auto& inter = registry.interactions("ecoenergy");
    if (inter.objects().size() != 6)
        throw std::runtime_error("EcoEnergy smoke expected 6 interactive objects");
    std::ofstream interCsv(directory / "interaction-states.csv");
    interCsv.exceptions(std::ios::badbit | std::ios::failbit);
    writeStatesHeader(interCsv);
    for (const auto& item : interactionCases) checkInteraction(smoke, inter, proj, item, interCsv);
    interCsv.close();

    checkAnimations(smoke);
    auto benchmark = runBenchmark(smoke);
    const double recordingSeconds = recordTour(smoke);
    std::sort(benchmark.durations.begin(), benchmark.durations.end());
    writeMetrics(smoke, stats, placements.size(), inter.objects().size(), loadSeconds, benchmark,
        recordingSeconds);

    const double seconds = benchmark.seconds;
    std::cout << "ECOENERGY SMOKE PASS: unique=" << stats.unique.size()
              << " resources=" << stats.resources.size()
              << " instances=" << placements.size() << " triangles=" << stats.sceneTriangles
              << " interactions=6 animations=8 load_seconds=" << loadSeconds
              << " frames=600 seconds=" << seconds << " fps_mean=" << 600.0 / seconds
              << " framebuffer=" << width << 'x' << height << std::endl;
}
}
