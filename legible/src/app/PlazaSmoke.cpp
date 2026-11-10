#include "PlazaSmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
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
    Camera& camera;
    const LightingSettings& lighting;
    const PlazaRender& render;
    const std::function<bool()>& present;
};

struct PlazaStats {
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
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::runtime_error("Invalid plaza depth");
    }
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Plaza framebuffer readback failed");
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

PlazaStats collectStats(const std::vector<ContentPlacement>& placements) {
    PlazaStats stats;
    for (const auto& placement : placements) {
        const auto id = placement.id.substr(0, placement.id.rfind("SHARED-", 0) == 0 ? 10 : 9);
        stats.unique.emplace(id, placement);
        ++stats.counts[id];
        stats.sceneTriangles += triangles(*placement.model);
        if (placement.id == "PLAZA-008-energy") stats.unique[id] = placement;
        stats.resources.insert(placement.resource.get());
        stats.renderModels.insert(placement.model.get());
        stats.cpuModels.insert(placement.resource.get());
        stats.cpuModels.insert(placement.model.get());
    }
    if (stats.unique.size() != 27 || stats.resources.size() != 29)
        throw std::runtime_error(
            "Plaza requires 25 base models, two shared designs and two signage variants");
    for (const auto* model : stats.resources) {
        stats.geometryCount += model->meshes.size();
        stats.textureCount += model->textures.size();
        stats.sourceTriangles += triangles(*model);
    }
    for (const auto* model : stats.renderModels) stats.renderGeometryCount += model->meshes.size();
    for (const auto* model : stats.cpuModels) stats.cpuBytes += payload(*model);
    return stats;
}

// cada modelo aislado debe pintar pixeles sobre el fondo
void checkModels(const SmokeContext& smoke, const PlazaStats& stats) {
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
                const auto position = glm::vec3(world * glm::vec4(vertex.position, 1));
                low = glm::min(low, position);
                high = glm::max(high, position);
            }
        }, placement.world);
        const auto center = (low + high) * .5f;
        const float radius = std::max({high.x - low.x, high.y - low.y, high.z - low.z}) * .5f;
        const auto front = glm::normalize(glm::vec3(placement.world * glm::vec4(1.2f, 1.4f, 2.2f, 0)));
        lookAt(smoke.camera, center + front * (3 * std::max(radius, .5f)), center);
        smoke.render(smoke.lighting, false, {});
        const auto background = capture(smoke.directory / "asset-background.ppm", smoke.width, smoke.height);
        smoke.render(smoke.lighting, true, placement.id);
        const auto assetPath = smoke.directory / "assets" / (modelId + ".ppm");
        const auto pixels = capture(assetPath, smoke.width, smoke.height);
        const auto difference = changed(background, pixels);
        if (difference < 30)
            throw std::runtime_error("Plaza model did not contribute visible pixels: " + modelId);
        evidence << modelId << ',' << placement.id << ',' << stats.counts.at(modelId) << ','
                 << triangles(*placement.model) << ',' << placement.model->meshes.size() << ','
                 << placement.model->textures.size() << ',' << difference << '\n';
        std::cout << "PLAZA MODEL PASS " << modelId << " pixels=" << difference << '\n';
    }
    evidence.close();
}

// vistas fijas y comparación de luz directa contra reflejos
void captureViews(const SmokeContext& smoke) {
    struct View {
        const char* name;
        glm::vec3 position, target;
    };
    const std::array<View, 7> views{{
        {"overview", {24, 24, 29}, {0, 0, 0}},
        {"entry", {0, 1.7f, 19}, {0, 1.8f, 0}},
        {"center", {6, 4, 7}, {0, 1.9f, 0}},
        {"ecoenergy", {-6, 1.7f, 3}, {-18, 1.8f, 0}},
        {"aquabio", {4, 1.7f, -6}, {0, 1.8f, -18}},
        {"ecocycle", {6, 1.7f, 3}, {18, 1.8f, 0}},
        {"garden", {-9, 2, 12}, {-6, .9f, 7}},
    }};
    Pixels combined;
    for (const auto& view : views) {
        lookAt(smoke.camera, view.position, view.target);
        smoke.render(smoke.lighting, true, {});
        const auto viewPath = smoke.directory / (std::string(view.name) + ".ppm");
        const auto pixels = capture(viewPath, smoke.width, smoke.height);
        if (std::string(view.name) == "overview") combined = pixels;
    }
    lookAt(smoke.camera, views[0].position, views[0].target);
    auto direct = smoke.lighting;
    direct.environment.reflectionEnabled = false;
    smoke.render(direct, true, {});
    const auto directPixels = capture(smoke.directory / "direct-only.ppm", smoke.width, smoke.height);
    auto reflection = smoke.lighting;
    reflection.directEnabled = false;
    smoke.render(reflection, true, {});
    const auto reflected = capture(smoke.directory / "environment-only.ppm", smoke.width, smoke.height);
    if (changed(combined, directPixels) < 30 || changed(directPixels, reflected) < 100)
        throw std::runtime_error("Plaza lighting comparison missing visible difference");
}

// recorrido por la plaza mirando al centro
const std::array<glm::vec3, 8> route{{
    {0, 1.7f, 17}, {0, 1.7f, 10}, {5.5f, 1.7f, 7}, {7.5f, 1.7f, 0},
    {0, 1.7f, -7.5f}, {-7.5f, 1.7f, 0}, {-5.5f, 1.7f, 7}, {0, 1.7f, 10},
}};

void moveOnRoute(Camera& camera, int frame) {
    const float progress = float(frame) / 599 * (route.size() - 1);
    const auto segment = std::min(static_cast<std::size_t>(progress), route.size() - 2);
    lookAt(camera, glm::mix(route[segment], route[segment + 1], progress - segment), {0, 1.6f, 0});
}

BenchmarkResult runBenchmark(const SmokeContext& smoke) {
    std::ofstream frames(smoke.directory / "frames.csv");
    frames.exceptions(std::ios::badbit | std::ios::failbit);
    frames << "frame,seconds,x,y,z,frame_seconds\n";
    BenchmarkResult result;
    // calentamiento
    for (int i = 0; i < 60; ++i) {
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("Plaza smoke interrupted");
    }
    const double start = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        const double frameStart = glfwGetTime();
        moveOnRoute(smoke.camera, i);
        smoke.render(smoke.lighting, true, {});
        if (smoke.present()) throw std::runtime_error("Plaza smoke interrupted");
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Plaza benchmark GL error");
        const double duration = glfwGetTime() - frameStart;
        result.durations.push_back(duration);
        const auto& position = smoke.camera.position;
        frames << i << ',' << glfwGetTime() - start << ',' << position.x << ',' << position.y << ','
               << position.z << ',' << duration << '\n';
    }
    result.seconds = glfwGetTime() - start;
    frames.close();
    return result;
}

// mismo recorrido guardando un cuadro de cada diez
double recordTour(const SmokeContext& smoke) {
    const double recordingStart = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        moveOnRoute(smoke.camera, i);
        smoke.render(smoke.lighting, true, {});
        if (i % 10 == 0) {
            const auto path = smoke.directory / "tour" / (std::to_string(1000 + i / 10) + ".ppm");
            capture(path, smoke.width, smoke.height);
        }
        if (smoke.present()) throw std::runtime_error("Plaza recording interrupted");
    }
    return glfwGetTime() - recordingStart;
}

void writeMetrics(const SmokeContext& smoke, const PlazaStats& stats, std::size_t instances,
    double loadSeconds, double seconds, double recordingSeconds, const std::vector<double>& sortedDurations) {
    std::ofstream metrics(smoke.directory / "metrics.csv");
    metrics.exceptions(std::ios::badbit | std::ios::failbit);
    metrics << "metric,value\n"
            << "unique_models," << stats.unique.size() << '\n'
            << "model_resources," << stats.resources.size() << '\n'
            << "instances," << instances << '\n'
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
            << "seconds," << seconds << '\n'
            << "fps_mean," << 600 / seconds << '\n'
            << "recording_seconds," << recordingSeconds << '\n'
            << "frame_p95_ms," << sortedDurations.at(569) * 1000 << "\n";
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

void checkPlazaFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present) {
    std::filesystem::create_directories(directory / "assets");
    std::filesystem::create_directories(directory / "tour");
    const SmokeContext smoke{directory, width, height, camera, lighting, render, present};
    const auto placements = registry.placements();
    const auto stats = collectStats(placements);

    checkModels(smoke, stats);
    captureViews(smoke);
    auto benchmark = runBenchmark(smoke);
    const double recordingSeconds = recordTour(smoke);
    std::sort(benchmark.durations.begin(), benchmark.durations.end());
    writeMetrics(smoke, stats, placements.size(), loadSeconds, benchmark.seconds, recordingSeconds,
        benchmark.durations);

    const double seconds = benchmark.seconds;
    std::cout << "PLAZA SMOKE PASS: unique=" << stats.resources.size() << " instances=" << placements.size()
              << " triangles=" << stats.sceneTriangles << " geometries=" << stats.geometryCount
              << " textures=" << stats.textureCount << " load_seconds=" << loadSeconds
              << " frames=600 seconds=" << seconds << " fps_mean=" << 600 / seconds
              << " framebuffer=" << width << 'x' << height << std::endl;
}
}
