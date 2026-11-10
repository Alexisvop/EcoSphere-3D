#include "AquaBioSmoke.hpp"
#include "modules/AquaBio/AquaBioSettings.hpp"
#include "world/ZoneSettings.hpp"
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
    std::ostream& views;
};

struct ZoneStats {
    std::map<std::string, ContentPlacement> unique;
    std::map<std::string, unsigned> counts;
    std::set<const Model*> resources;
    std::size_t sourceTriangles = 0;
    std::size_t sceneTriangles = 0;
    std::size_t geometries = 0;
    std::size_t cpuBytes = 0;
    std::size_t textureCount = 0;
};

struct Bounds {
    glm::vec3 center;
    float radius;
};

struct BenchmarkResult {
    double seconds = 0;
    std::vector<double> durations;
};

struct View {
    const char* name;
    glm::vec3 position, target;
};

const std::array<View, 11> stations{{
    {"overview", {35, 27, 39}, {0, 0, -2}},
    {"overview-equivalent", {23, 21, 26}, {0, 0, -3}},
    {"arrival", {0, 1.7f, 22}, {0, 1.8f, 9}},
    {"entry-equivalent", {0, 1.7f, 6}, {0, 1.5f, -7}},
    {"rain-capture", {-8, 1.7f, 13}, {-14, 1.6f, 6}},
    {"treatment", {-7.5f, 5, -6}, {-8.5f, 1.3f, -15}},
    {"treatment-visitor", {-7.5f, 1.7f, -10}, {-13.5f, 1.2f, -14}},
    {"sampling", {6, 1.7f, 1}, {6, 1.3f, -4}},
    {"water-cycle", {-7, 1.7f, 22}, {-9, 1.8f, 16.5f}},
    {"wetland", {8.3f, 1.7f, 9}, {14, 1, 4}},
    {"biodiversity", {12.3f, 1.7f, 11}, {18, 1, 14.5f}},
}};

// número de modelo y centro del panel relativo al modelo
struct PanelView {
    int model;
    glm::vec3 center;
};

const std::array<PanelView, 11> panels{{
    {4, {-3.6f, 1.5f, .5f}}, {6, {-3.2f, 1.5f, 0}}, {8, {-3, 1.5f, -.2f}},
    {10, {-4, 1.5f, 0}}, {12, {-2.5f, 1.5f, 0}}, {13, {-2.5f, 1.5f, 0}},
    {15, {2.8f, 1.5f, -.4f}}, {16, {-3.5f, 1.5f, 0}}, {20, {3.1f, 1.5f, 0}},
    {22, {0, 1.5f, .8f}}, {23, {-5.9f, 1.5f, -1.8f}},
}};

const std::array<const char*, 7> interactive{{
    "AQUA-006-cistern", "AQUA-009-demo", "AQUA-013-carbon", "AQUA-014-demo",
    "AQUA-018-manual", "AQUA-020-sampling", "AQUA-028-demo",
}};

const std::array<const char*, 18> animated{{
    "AQUA-009-pump", "AQUA-011-scraper", "AQUA-019-indicator", "AQUA-021-cycle",
    "AQUA-005-adjustable", "AQUA-014-extraction", "AQUA-024-floating", "AQUA-025-sway",
    "AQUA-024-static-0", "AQUA-024-static-1", "AQUA-024-static-2", "AQUA-024-static-3", "AQUA-024-static-4",
    "AQUA-025-static-0", "AQUA-025-static-1", "AQUA-025-static-2", "AQUA-025-static-3", "AQUA-025-static-4",
}};

// recorrido: posiciones de cámara y puntos a los que mira
const std::array<glm::vec3, 8> route{{
    {0, 1.7f, 13}, {-7.5f, 1.7f, 2.7f}, {-7.5f, 1.7f, -8}, {0, 1.7f, -10},
    {0, 1.7f, -14}, {0, 1.7f, 4}, {8.3f, 1.7f, 8}, {12.3f, 1.7f, 11},
}};
const std::array<glm::vec3, 8> targets{{
    {-5, 1, 9}, {-13, 1.5f, 7}, {-12, 1.2f, -7}, {-9, 1.5f, -15},
    {2, 1.8f, -19}, {11, 1, -4}, {15, 1, 4}, {18, 1, 14.5f},
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
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    for (float value : depth) {
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::runtime_error("Invalid AquaBio depth");
    }
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("AquaBio readback failed");
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        const auto offset = static_cast<std::size_t>(y) * width * 3;
        const auto* row = reinterpret_cast<const char*>(pixels.data() + offset);
        out.write(row, width * 3);
    }
    return pixels;
}

// captura con nombre y anota la cámara en views.csv
Pixels shot(const SmokeContext& smoke, const std::string& name, double time) {
    const auto& camera = smoke.camera;
    smoke.views << name << ',' << time << ',' << camera.position.x << ',' << camera.position.y << ','
                << camera.position.z << ',' << camera.yawDegrees << ',' << camera.pitchDegrees << ','
                << camera.fovDegrees << '\n';
    return capture(smoke.directory / (name + ".ppm"), smoke.width, smoke.height);
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

// caja del modelo ya colocado en el mundo
Bounds worldBounds(const ContentPlacement& placement) {
    glm::vec3 low(std::numeric_limits<float>::infinity());
    glm::vec3 high(-std::numeric_limits<float>::infinity());
    placement.model->forEach([&](std::size_t index, const glm::mat4& matrix) {
        for (const auto& vertex : placement.model->meshes.at(index).vertices) {
            const auto point = glm::vec3(matrix * glm::vec4(vertex.position, 1));
            low = glm::min(low, point);
            high = glm::max(high, point);
        }
    }, placement.world);
    return {(low + high) * .5f, std::max({high.x - low.x, high.y - low.y, high.z - low.z}) * .5f};
}

std::size_t triangles(const Model& model) {
    std::size_t count = 0;
    model.forEach([&](std::size_t index, const glm::mat4&) {
        const auto& mesh = model.meshes.at(index);
        count += (mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size()) / 3;
    });
    return count;
}

// número con ceros a la izquierda, por ejemplo AQUA-004
std::string numbered(const std::string& prefix, std::size_t number) {
    return prefix + (number < 10 ? "00" : "0") + std::to_string(number);
}

// una fila por instancia con su matriz completa
void writeStates(std::ostream& out, const std::string& stage, double time,
    const std::vector<ContentPlacement>& placements) {
    for (const auto& placement : placements) {
        out << stage << ',' << time << ',' << placement.id;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) out << ',' << placement.world[c][r];
        }
        out << '\n';
    }
}

void writeStatesHeader(std::ostream& out) {
    out << std::setprecision(9) << "stage,time_s,instance_id";
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) out << ",m" << c << r;
    }
    out << '\n';
}

std::string modelIdOf(const std::string& instanceId) {
    return instanceId.substr(0, instanceId.rfind("SHARED-", 0) == 0 ? 10 : 8);
}

ZoneStats collectStats(const std::vector<ContentPlacement>& placements) {
    ZoneStats stats;
    for (const auto& placement : placements) {
        stats.unique.emplace(modelIdOf(placement.id), placement);
        stats.counts[modelIdOf(placement.id)]++;
        stats.resources.insert(placement.resource.get());
        stats.sceneTriangles += triangles(*placement.model);
        if (placement.model != placement.resource)
            throw std::runtime_error("AquaBio unexpectedly duplicated visual resources");
    }
    if (placements.size() != 55 || stats.unique.size() != 31 || stats.resources.size() != 31)
        throw std::runtime_error("AquaBio count mismatch");
    for (const auto* model : stats.resources) {
        stats.sourceTriangles += triangles(*model);
        stats.geometries += model->meshes.size();
        stats.textureCount += model->textures.size();
        for (const auto& mesh : model->meshes)
            stats.cpuBytes += mesh.vertices.size() * sizeof(Vertex)
                            + mesh.indices.size() * sizeof(std::uint32_t);
    }
    return stats;
}

// ---- Cada modelo aislado debe pintar pixeles ----
void checkModels(const SmokeContext& smoke, const ZoneStats& stats) {
    std::ofstream usage(smoke.directory / "model-use.csv");
    usage.exceptions(std::ios::badbit | std::ios::failbit);
    usage << "model_id,instance_id,instances,triangles,meshes,textures,changed_pixels\n";
    for (const auto& item : stats.unique) {
        const auto& id = item.first;
        const auto& placement = item.second;
        const auto bounds = worldBounds(placement);
        const auto eye = bounds.center
                       + glm::normalize(glm::vec3(1.2f, 1.4f, 2.2f)) * std::max(bounds.radius, .3f) * 3.1f;
        lookAt(smoke.camera, eye, bounds.center);
        smoke.render(smoke.lighting, false, {});
        const auto background = capture(smoke.directory / "background.ppm", smoke.width, smoke.height);
        smoke.render(smoke.lighting, true, placement.id);
        const auto pixels = shot(smoke, "assets/" + id, 0);
        const auto count = changed(background, pixels);
        if (count < 30) throw std::runtime_error("AquaBio missing visible model: " + id);
        usage << id << ',' << placement.id << ',' << stats.counts.at(id) << ','
              << triangles(*placement.model) << ',' << placement.model->meshes.size() << ','
              << placement.model->textures.size() << ',' << count << '\n';
        std::cout << "AQUABIO MODEL PASS " << id << " pixels=" << count << '\n';
    }
    usage.close();
}

// ---- Estaciones, entrada y paneles ----
void captureStationsAndPanels(const SmokeContext& smoke) {
    for (const auto& station : stations) {
        lookAt(smoke.camera, station.position, station.target);
        smoke.render(smoke.lighting, true, {});
        shot(smoke, station.name, 0);
    }
    smoke.camera = ZoneSettings{}.entries[2].make();
    smoke.render(smoke.lighting, true, {});
    shot(smoke, "entry", 0);
    for (const auto& panel : panels) {
        const auto target = aquabio::settings.positions[panel.model - 1] + panel.center;
        auto observer = target + glm::vec3(0, 0, 4.5f);
        observer.y = 1.7f;
        lookAt(smoke.camera, observer, target);
        smoke.render(smoke.lighting, true, {});
        shot(smoke, numbered("panels/AQUA-", panel.model), 0);
    }
}

// luz directa contra solo reflejos desde la vista general
void compareLighting(const SmokeContext& smoke) {
    lookAt(smoke.camera, stations[0].position, stations[0].target);
    auto direct = smoke.lighting;
    direct.environment.reflectionEnabled = false;
    smoke.render(direct, true, {});
    const auto directPixels = shot(smoke, "direct-only", 0);
    auto environment = smoke.lighting;
    environment.directEnabled = false;
    smoke.render(environment, true, {});
    const auto environmentPixels = shot(smoke, "environment-only", 0);
    if (changed(directPixels, environmentPixels) < 200)
        throw std::runtime_error("AquaBio material/light comparison missing");
}

// ---- Interacciones: rayo, resaltado, mover y soltar ----
void checkInteractions(const SmokeContext& smoke) {
    auto& inter = smoke.registry.interactions("aquabio");
    if (inter.objects().size() != 7) throw std::runtime_error("AquaBio interaction count");
    std::ofstream interactionCsv(smoke.directory / "interaction-states.csv");
    interactionCsv.exceptions(std::ios::badbit | std::ios::failbit);
    writeStatesHeader(interactionCsv);
    for (std::size_t i = 0; i < interactive.size(); ++i) {
        const std::string id = interactive[i];
        const std::string code = numbered("INT-AQUA-", i + 1);
        inter.deselect();
        const auto& box = inter.object(id).bounds;
        const auto center = glm::vec3(inter.world(id) * glm::vec4((box.minimum + box.maximum) * .5f, 1));
        const float radius = glm::length(box.maximum - box.minimum) * .5f;
        auto observer = center + glm::vec3(.4f, 0, 1) * std::max(1.3f, radius * 2.5f);
        observer.y = 1.7f;
        lookAt(smoke.camera, observer, center);
        smoke.render(smoke.lighting, true, {});
        const auto before = shot(smoke, "interactions/" + code + "-before", 0);
        writeStates(interactionCsv, code + "_before", 0, smoke.registry.placements());

        // rayo por el centro de la pantalla
        const auto projection = *smoke.camera.projection(smoke.width, smoke.height);
        const auto ray = cameraRay(smoke.camera.view(), projection, {0, 0});
        if (inter.pick(ray) != std::optional<std::string>(id))
            throw std::runtime_error("AquaBio ray-pick failed: " + id);
        smoke.render(smoke.lighting, true, {});
        const auto selected = shot(smoke, "interactions/" + code + "-selected", 0);
        if (changed(before, selected) < 25) throw std::runtime_error("AquaBio highlight invisible: " + id);

        inter.manipulate({.35f, .24f, -.10f}, glm::angleAxis(glm::radians(23.0f), glm::vec3(0, 1, 0)), 1.12f);
        inter.take(ray, glm::length(center - ray.origin));
        inter.follow(Ray{ray.origin + glm::vec3(.25f, .15f, 0), ray.direction});
        inter.drop();
        smoke.render(smoke.lighting, true, {});
        const auto manipulated = shot(smoke, "interactions/" + code + "-manipulated", 0);
        if (changed(selected, manipulated) < 60)
            throw std::runtime_error("AquaBio manipulation invisible: " + id);
        writeStates(interactionCsv, code + "_manipulated", 0, smoke.registry.placements());

        inter.object(id).manipulation = Pose{};
        inter.deselect();
        std::cout << "AQUABIO INTERACTION PASS " << code << " select/TRS/take/follow/drop\n";
    }
    interactionCsv.close();
}

// ---- Animaciones: misma cámara en t=0 y t=2 ----
void checkAnimations(const SmokeContext& smoke, const std::vector<ContentPlacement>& placements) {
    std::ofstream animationCsv(smoke.directory / "animation-states.csv");
    animationCsv.exceptions(std::ios::badbit | std::ios::failbit);
    writeStatesHeader(animationCsv);
    writeStates(animationCsv, "t0", 0, smoke.registry.placements());
    std::array<Pixels, 18> initial;
    std::array<Camera, 18> cameras;
    for (std::size_t i = 0; i < animated.size(); ++i) {
        const auto found = std::find_if(placements.begin(), placements.end(),
            [&](const auto& placement) { return placement.id == animated[i]; });
        if (found == placements.end()) throw std::runtime_error("Animated instance missing");
        const auto bounds = worldBounds(*found);
        const auto eye = bounds.center
                       + glm::normalize(glm::vec3(.4f, .7f, 2)) * std::max(.4f, bounds.radius) * 3.6f;
        lookAt(smoke.camera, eye, bounds.center);
        cameras[i] = smoke.camera;
        smoke.render(smoke.lighting, true, found->id);
        initial[i] = shot(smoke, numbered("animations/ANIM-AQUA-", i + 1) + "-t0", 0);
    }
    smoke.registry.update(2);
    writeStates(animationCsv, "t2", 2, smoke.registry.placements());
    for (std::size_t i = 0; i < animated.size(); ++i) {
        smoke.camera = cameras[i];
        smoke.render(smoke.lighting, true, std::string(animated[i]));
        const auto pixels = shot(smoke, numbered("animations/ANIM-AQUA-", i + 1) + "-t1", 2);
        const auto count = changed(initial[i], pixels);
        if (count < 40) throw std::runtime_error("AquaBio motion invisible: " + std::string(animated[i]));
        std::cout << "AQUABIO ANIMATION PASS " << animated[i] << " pixels=" << count << '\n';
    }
    animationCsv.close();
}

// ---- Medición de cuadros y video del recorrido ----
void moveOnRoute(Camera& camera, int frame) {
    const float progress = static_cast<float>(frame) / 599 * 7;
    const auto segment = std::min(static_cast<std::size_t>(progress), std::size_t{6});
    const float t = progress - static_cast<float>(segment);
    lookAt(camera, glm::mix(route[segment], route[segment + 1], t),
        glm::mix(targets[segment], targets[segment + 1], t));
}

BenchmarkResult runBenchmark(const SmokeContext& smoke) {
    // calentamiento
    for (int i = 0; i < 60; ++i) {
        smoke.registry.update(1.0 / 60);
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("AquaBio warmup interrupted");
    }
    std::ofstream frames(smoke.directory / "frames.csv");
    frames.exceptions(std::ios::badbit | std::ios::failbit);
    frames << "frame,seconds,frame_seconds\n";
    BenchmarkResult result;
    glFinish();
    const double start = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        const double frameStart = glfwGetTime();
        moveOnRoute(smoke.camera, i);
        smoke.registry.update(1.0 / 60);
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("AquaBio benchmark interrupted");
        result.durations.push_back(glfwGetTime() - frameStart);
        frames << i << ',' << glfwGetTime() - start << ',' << result.durations.back() << '\n';
    }
    glFinish();
    result.seconds = glfwGetTime() - start;
    frames.close();
    return result;
}

// el video avanza a media velocidad y guarda un cuadro de cada diez
double recordTour(const SmokeContext& smoke) {
    std::ofstream tourCameras(smoke.directory / "tour-cameras.csv");
    tourCameras.exceptions(std::ios::badbit | std::ios::failbit);
    tourCameras << "frame,recording_step_s,camera_x,camera_y,camera_z,yaw_deg,pitch_deg\n";
    const double recordingStart = glfwGetTime();
    for (int i = 0; i < 1200; ++i) {
        moveOnRoute(smoke.camera, i / 2);
        smoke.registry.update(1.0 / 60);
        smoke.render(smoke.lighting, true, {});
        if (i % 10 == 0) {
            const auto path = smoke.directory / "tour" / (std::to_string(1000 + i / 10) + ".ppm");
            capture(path, smoke.width, smoke.height);
            const auto& camera = smoke.camera;
            tourCameras << i << ',' << (i + 1) / 60.0 << ','
                        << camera.position.x << ',' << camera.position.y << ',' << camera.position.z << ','
                        << camera.yawDegrees << ',' << camera.pitchDegrees << '\n';
        }
        if (smoke.present()) throw std::runtime_error("AquaBio recording interrupted");
    }
    const double recordingSeconds = glfwGetTime() - recordingStart;
    tourCameras.close();
    return recordingSeconds;
}

void writeMetrics(const SmokeContext& smoke, const ZoneStats& stats, double loadSeconds,
    const BenchmarkResult& benchmark, double recordingSeconds) {
    const auto& durations = benchmark.durations;
    std::ofstream metrics(smoke.directory / "metrics.csv");
    metrics.exceptions(std::ios::badbit | std::ios::failbit);
    metrics << std::setprecision(9) << "metric,value\n"
            << "unique_models,28\n"
            << "model_resources,28\n"
            << "instances,52\n"
            << "interactive_objects,7\n"
            << "animated_entities,18\n"
            << "source_triangles," << stats.sourceTriangles << '\n'
            << "scene_triangles," << stats.sceneTriangles << '\n'
            << "source_geometries," << stats.geometries << '\n'
            << "source_textures," << stats.textureCount << '\n'
            << "width," << smoke.width << '\n'
            << "height," << smoke.height << '\n'
            << "cpu_geometry_bytes_lower_bound," << stats.cpuBytes << '\n'
            << "cpu_load_seconds," << loadSeconds << '\n'
            << "frames,600\n"
            << "seconds," << benchmark.seconds << '\n'
            << "fps_mean," << 600 / benchmark.seconds << '\n'
            << "frame_p50_ms," << durations[299] * 1000 << '\n'
            << "frame_p95_ms," << durations[569] * 1000 << '\n'
            << "frame_p99_ms," << durations[593] * 1000 << '\n'
            << "recording_seconds," << recordingSeconds << '\n';
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

void checkAquaBioFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present) {
    for (const auto* name : {"assets", "interactions", "animations", "panels", "tour"})
        std::filesystem::create_directories(directory / name);
    const auto placements = registry.placements();
    const auto stats = collectStats(placements);

    std::ofstream views(directory / "views.csv");
    views.exceptions(std::ios::badbit | std::ios::failbit);
    views << "capture,time_s,camera_x,camera_y,camera_z,yaw_deg,pitch_deg,fov_deg\n";
    const SmokeContext smoke{directory, width, height, registry, camera, lighting, render, present, views};

    checkModels(smoke, stats);
    captureStationsAndPanels(smoke);
    compareLighting(smoke);
    checkInteractions(smoke);
    checkAnimations(smoke, placements);
    views.close();

    auto benchmark = runBenchmark(smoke);
    const double recordingSeconds = recordTour(smoke);
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("AquaBio GL error");
    std::sort(benchmark.durations.begin(), benchmark.durations.end());
    writeMetrics(smoke, stats, loadSeconds, benchmark, recordingSeconds);

    const double seconds = benchmark.seconds;
    std::cout << "AQUABIO SMOKE PASS: 28 models; 52 instances; 7 interactions; 18 animations; "
              << "frames=600 seconds=" << seconds << " fps_mean=" << 600 / seconds
              << " framebuffer=" << width << 'x' << height << '\n';
}
}
