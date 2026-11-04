#include "EcoEnergySmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

namespace ecosphere {
namespace {
void lookAt(Camera& camera, glm::vec3 position, glm::vec3 target) {
    const auto d = target - position;
    camera.position = position;
    camera.yawDegrees = glm::degrees(std::atan2(d.z, d.x));
    camera.pitchDegrees = glm::degrees(std::atan2(d.y, glm::length(glm::vec2(d.x, d.z))));
}

std::vector<unsigned char> capture(const std::filesystem::path& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    for (float d : depth)
        if (!std::isfinite(d) || d < 0.0f || d > 1.0f) throw std::runtime_error("Invalid EcoEnergy depth");
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("EcoEnergy framebuffer readback failed");
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(y) * width * 3), width * 3);
    return pixels;
}

std::size_t changed(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.size(); i += 3)
        n += std::abs(int(a[i]) - int(b[i])) + std::abs(int(a[i + 1]) - int(b[i + 1])) + std::abs(int(a[i + 2]) - int(b[i + 2])) > 12;
    return n;
}

std::size_t triangles(const Model& model) {
    std::size_t n = 0;
    model.forEach([&](std::size_t index, const glm::mat4&) {
        const auto& m = model.meshes.at(index);
        n += (m.indices.empty() ? m.vertices.size() : m.indices.size()) / 3;
    });
    return n;
}

std::size_t payload(const Model& model) {
    std::size_t n = sizeof(Model) + model.nodes.size() * sizeof(ModelNode) + model.meshes.size() * sizeof(Mesh);
    for (const auto& m : model.meshes)
        n += m.vertices.size() * sizeof(Vertex) + m.indices.size() * sizeof(std::uint32_t);
    for (const auto& image : model.images) n += image.rgba.size();
    for (const auto& node : model.nodes) n += node.name.size() + node.meshes.size() * sizeof(std::size_t);
    return n;
}

void writePlacementRows(std::ostream& out, const std::string& label, double seconds,
                        const std::vector<ContentPlacement>& placements, const std::set<std::string>& filter) {
    for (const auto& p : placements) {
        if (!filter.empty() && !filter.count(p.id)) continue;
        const auto& m = p.world;
        out << label << ',' << seconds << ',' << p.id << ','
            << m[3].x << ',' << m[3].y << ',' << m[3].z << ','
            << m[0].x << ',' << m[0].y << ',' << m[0].z << ','
            << m[1].x << ',' << m[1].y << ',' << m[1].z << '\n';
    }
}
}

void checkEcoEnergyFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, Camera& camera, const LightingSettings& lighting, double loadSeconds,
    const PlazaRender& render, const std::function<bool()>& present) {
    std::filesystem::create_directories(directory / "assets");
    std::filesystem::create_directories(directory / "interactions");
    std::filesystem::create_directories(directory / "animations");
    std::filesystem::create_directories(directory / "tour");

    const auto placements = registry.placements();
    std::map<std::string, ContentPlacement> unique;
    std::map<std::string, unsigned> counts;
    std::set<const Model*> resources, renderModels, cpuModels;
    std::size_t sceneTriangles = 0, geometryCount = 0, renderGeometryCount = 0, sourceTriangles = 0, textureCount = 0, cpuBytes = 0;

    for (const auto& p : placements) {
        const auto id = p.id.substr(0, 10);
        unique.emplace(id, p);
        ++counts[id];
        sceneTriangles += triangles(*p.model);
        resources.insert(p.resource.get());
        renderModels.insert(p.model.get());
        cpuModels.insert(p.resource.get());
        cpuModels.insert(p.model.get());
    }
    if (unique.size() != 31 || resources.size() != 31)
        throw std::runtime_error("EcoEnergy requires 28 original and 3 shared resources");
    for (const auto* model : resources) {
        geometryCount += model->meshes.size();
        textureCount += model->textures.size();
        sourceTriangles += triangles(*model);
    }
    for (const auto* model : renderModels) renderGeometryCount += model->meshes.size();
    for (const auto* model : cpuModels) cpuBytes += payload(*model);

    // 1. Verificación real de píxeles visibles para los 28 modelos únicos.
    std::ofstream evidence(directory / "model-use.csv");
    evidence.exceptions(std::ios::badbit | std::ios::failbit);
    evidence << "model_id,instance_id,instances,triangles,meshes,textures,changed_pixels\n";
    for (const auto& item : unique) {
        const auto& p = item.second;
        glm::vec3 lo(std::numeric_limits<float>::infinity()), hi(-std::numeric_limits<float>::infinity());
        p.model->forEach([&](std::size_t mesh, const glm::mat4& world) {
            for (const auto& v : p.model->meshes.at(mesh).vertices) {
                const auto pos = glm::vec3(world * glm::vec4(v.position, 1.0f));
                lo = glm::min(lo, pos);
                hi = glm::max(hi, pos);
            }
        }, p.world);
        const auto center = (lo + hi) * 0.5f;
        const float radius = std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) * 0.5f;
        const auto front = glm::normalize(glm::vec3(p.world * glm::vec4(1.2f, 1.4f, 2.2f, 0.0f)));
        lookAt(camera, center + front * (3.0f * std::max(radius, 0.45f)), center);
        render(lighting, false, {});
        const auto background = capture(directory / "asset-background.ppm", width, height);
        render(lighting, true, p.id);
        const auto pixels = capture(directory / "assets" / (item.first + ".ppm"), width, height);
        const auto diff = changed(background, pixels);
        if (diff < 30) throw std::runtime_error("EcoEnergy model did not contribute visible pixels: " + item.first);
        evidence << item.first << ',' << p.id << ',' << counts.at(item.first) << ',' << triangles(*p.model) << ','
                 << p.model->meshes.size() << ',' << p.model->textures.size() << ',' << diff << '\n';
        std::cout << "ECOENERGY MODEL PASS " << item.first << " pixels=" << diff << '\n';
    }
    evidence.close();

    // 2. Vistas generales, temáticas y comparación de materiales/iluminación.
    struct View { const char* name; glm::vec3 position, target; };
    const std::array<View, 7> views{{
        {"overview",      {22.0f, 17.5f, 22.0f},  {0.0f, 2.2f, -3.5f}},
        {"entry",         {0.0f, 1.7f, 6.0f},     {0.0f, 1.35f, -8.0f}},
        {"solar-zone",    {-6.2f, 2.3f, -3.5f},   {-9.2f, 1.1f, -8.0f}},
        {"wind-zone",     {6.8f, 2.6f, -1.8f},    {6.8f, 4.2f, -11.5f}},
        {"grid-zone",     {5.5f, 2.3f, -1.2f},    {12.4f, 1.8f, -3.5f}},
        {"battery-zone",  {6.8f, 1.9f, 5.4f},     {8.0f, 1.0f, 1.3f}},
        {"mobility-zone", {-6.8f, 2.1f, 8.8f},    {-9.2f, 0.9f, 0.8f}},
    }};
    std::vector<unsigned char> overviewPixels;
    for (const auto& view : views) {
        lookAt(camera, view.position, view.target);
        render(lighting, true, {});
        const auto pixels = capture(directory / (std::string(view.name) + ".ppm"), width, height);
        if (std::string(view.name) == "overview") overviewPixels = pixels;
    }
    lookAt(camera, views[0].position, views[0].target);
    auto directOnly = lighting;
    directOnly.environment.reflectionEnabled = false;
    render(directOnly, true, {});
    const auto directPixels = capture(directory / "direct-only.ppm", width, height);
    auto envOnly = lighting;
    envOnly.directEnabled = false;
    render(envOnly, true, {});
    const auto envPixels = capture(directory / "environment-only.ppm", width, height);
    if (changed(overviewPixels, directPixels) < 30 || changed(directPixels, envPixels) < 100)
        throw std::runtime_error("EcoEnergy lighting/reflection comparison missing visible difference");

    // 3. Seis interacciones educativas verificadas por ray-pick, barras amarillas AABB y manipulación real.
    const auto proj = *camera.projection(width, height);
    auto& inter = registry.interactions("ecoenergy");
    if (inter.objects().size() != 6) throw std::runtime_error("EcoEnergy smoke expected 6 interactive objects");

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
    const std::array<InteractionCase, 6> cases{{
        {"INT-ENERGY-001", "ENERGY-005-panel-adjustable", {-1.1f, 1.85f, 4.0f},  false, {0.32f, 0.24f, 0.0f},   {1.0f, 0.0f, 0.0f}, 24.0f, 1.12f},
        {"INT-ENERGY-002", "ENERGY-019-blade-demo",       {5.3f, 1.55f, -3.7f},  false, {0.0f, 0.34f, 0.25f},   {0.0f, 1.0f, 0.0f}, 28.0f, 1.10f},
        {"INT-ENERGY-003", "ENERGY-011-battery-module",   {5.2f, 1.25f, 3.9f},   false, {0.32f, 0.28f, 0.15f},  {0.0f, 1.0f, 0.0f}, -26.0f, 1.14f},
        {"INT-ENERGY-004", "ENERGY-023-generator-coil",   {8.5f, 1.35f, -3.3f},  true,  {0.45f, 0.20f, -0.35f}, {0.0f, 1.0f, 0.0f}, 0.0f, 1.0f},
        {"INT-ENERGY-005", "ENERGY-027-charge-connector", {-5.0f, 1.15f, 3.7f},  true,  {-0.40f, 0.25f, -0.35f},{0.0f, 1.0f, 0.0f}, 0.0f, 1.0f},
        {"INT-ENERGY-006", "ENERGY-026-ev-interactive",   {-5.0f, 2.05f, 10.2f}, false, {0.48f, 0.0f, -0.70f},  {0.0f, 1.0f, 0.0f}, 18.0f, 1.05f},
    }};
    std::ofstream interCsv(directory / "interaction-states.csv");
    interCsv.exceptions(std::ios::badbit | std::ios::failbit);
    interCsv << std::setprecision(7) << "stage,time_s,instance_id,x,y,z,m00,m01,m02,m10,m11,m12\n";

    for (const auto& c : cases) {
        inter.deselect();
        const auto& box = inter.object(c.instanceId).bounds;
        const auto worldCenter = glm::vec3(inter.world(c.instanceId) * glm::vec4((box.minimum + box.maximum) * 0.5f, 1.0f));
        lookAt(camera, c.camPos, worldCenter);
        render(lighting, true, {});
        const auto beforeImg = capture(directory / "interactions" / (std::string(c.code) + "-before.ppm"), width, height);
        writePlacementRows(interCsv, std::string(c.code) + "_before", 0.0, registry.placements(), {c.instanceId});

        const auto clip = proj * camera.view() * glm::vec4(worldCenter, 1.0f);
        const auto ray = cameraRay(camera.view(), proj, glm::vec2(clip) / clip.w);
        const auto picked = inter.pick(ray);
        if (picked != std::optional<std::string>(c.instanceId))
            throw std::runtime_error(std::string("EcoEnergy ray-pick failed for ") + c.instanceId);

        render(lighting, true, {});
        const auto selectedImg = capture(directory / "interactions" / (std::string(c.code) + "-selected.ppm"), width, height);
        const auto selPixels = changed(beforeImg, selectedImg);
        if (selPixels < 25)
            throw std::runtime_error(std::string("Missing visible selection feedback for ") + c.instanceId);

        if (c.testCarry) {
            const float dist = glm::length(worldCenter - ray.origin);
            inter.take(ray, dist);
            inter.follow(Ray{ray.origin + c.deltaPos, ray.direction});
            inter.drop();
        } else {
            inter.manipulate(c.deltaPos, glm::angleAxis(glm::radians(c.rotDeg), c.rotAxis), c.scaleFactor);
        }
        render(lighting, true, {});
        const auto manipImg = capture(directory / "interactions" / (std::string(c.code) + "-manipulated.ppm"), width, height);
        const auto manipPixels = changed(selectedImg, manipImg);
        if (manipPixels < 60)
            throw std::runtime_error(std::string("Missing visible manipulation delta for ") + c.instanceId);
        writePlacementRows(interCsv, std::string(c.code) + "_manipulated", 0.0, registry.placements(), {c.instanceId});

        inter.object(c.instanceId).manipulation = Pose{};
        inter.deselect();
        std::cout << "ECOENERGY INTERACTION PASS " << c.code << " (" << c.instanceId
                  << ") select_px=" << selPixels << " manip_px=" << manipPixels << '\n';
    }
    interCsv.close();

    // 4. Ocho animaciones (4 simples, 2 keyframes, 2 procedurales) verificadas en múltiples tiempos.
    const std::set<std::string> animatedIds{
        "ENERGY-018-rotor-main", "ENERGY-018-rotor-secondary", "ENERGY-021-anemometer-main", "ENERGY-024-demo-fan",
        "ENERGY-005-tracker-wing", "ENERGY-026-ev-shuttle", "ENERGY-020-weathervane", "ENERGY-012-charge-indicator"
    };
    std::ofstream animCsv(directory / "animation-states.csv");
    animCsv.exceptions(std::ios::badbit | std::ios::failbit);
    animCsv << std::setprecision(7) << "stage,time_s,instance_id,x,y,z,m00,m01,m02,m10,m11,m12\n";

    struct AnimShot { const char* prefix; glm::vec3 pos, target; };
    const AnimShot shotSimple{"simple", {6.8f, 2.4f, -3.2f}, {6.8f, 4.5f, -11.5f}};
    const AnimShot shotTracker{"keyframe-tracker", {-2.6f, 2.0f, -5.2f}, {-4.2f, 1.65f, -8.2f}};
    const AnimShot shotEv{"keyframe-ev", {-7.8f, 2.2f, 5.2f}, {-11.2f, 0.8f, 0.4f}};
    const AnimShot shotVane{"procedural-vane", {7.6f, 1.85f, -6.9f}, {8.6f, 1.60f, -8.8f}};
    const AnimShot shotSoc{"procedural-soc", {8.5f, 1.35f, 3.3f}, {8.5f, 1.15f, 1.3f}};

    const auto takeShot = [&](const AnimShot& s, const char* suffix) {
        lookAt(camera, s.pos, s.target);
        render(lighting, true, {});
        return capture(directory / "animations" / (std::string(s.prefix) + "-" + suffix + ".ppm"), width, height);
    };

    writePlacementRows(animCsv, "t0", 0.0, registry.placements(), animatedIds);
    const auto sim0 = takeShot(shotSimple, "t0");
    const auto trk0 = takeShot(shotTracker, "t0");
    const auto ev0  = takeShot(shotEv, "t0");
    const auto van0 = takeShot(shotVane, "t0");
    const auto soc0 = takeShot(shotSoc, "t0");

    registry.update(1.5);
    writePlacementRows(animCsv, "t1_5", 1.5, registry.placements(), animatedIds);
    const auto soc1 = takeShot(shotSoc, "t1");

    registry.update(2.0); // t = 3.5 s
    writePlacementRows(animCsv, "t3_5", 3.5, registry.placements(), animatedIds);
    const auto sim1 = takeShot(shotSimple, "t1");
    const auto trk1 = takeShot(shotTracker, "t1");
    const auto ev1  = takeShot(shotEv, "t1");
    const auto van1 = takeShot(shotVane, "t1");
    animCsv.close();

    for (const auto& check : std::array<std::pair<const char*, std::size_t>, 5>{{
        {"simple", changed(sim0, sim1)},
        {"keyframe-tracker", changed(trk0, trk1)},
        {"keyframe-ev", changed(ev0, ev1)},
        {"procedural-vane", changed(van0, van1)},
        {"procedural-soc", changed(soc0, soc1)},
    }}) {
        if (check.second < 40)
            throw std::runtime_error(std::string("EcoEnergy animation capture missing motion: ") + check.first);
        std::cout << "ECOENERGY ANIMATION PASS " << check.first << " changed_pixels=" << check.second << '\n';
    }

    // 5. Benchmark de 600 fotogramas con animaciones activas + segunda pasada de vídeo de recorrido.
    const std::array<std::pair<glm::vec3, glm::vec3>, 8> route{{
        {{0.0f, 1.7f, 8.5f},    {0.0f, 1.6f, -6.0f}},
        {{0.0f, 1.7f, 2.5f},    {-4.5f, 1.6f, -6.5f}},
        {{-6.5f, 1.7f, -3.8f},  {-9.5f, 1.4f, -8.5f}},
        {{0.0f, 1.7f, -5.5f},   {5.5f, 5.0f, -13.5f}},
        {{6.8f, 1.7f, -4.0f},   {11.5f, 1.5f, -5.0f}},
        {{6.2f, 1.7f, 4.2f},    {8.8f, 1.2f, 1.2f}},
        {{-6.2f, 1.7f, 4.2f},   {-9.8f, 1.0f, 1.0f}},
        {{0.0f, 1.7f, 6.0f},    {0.0f, 1.5f, -8.0f}},
    }};
    const auto moveOnRoute = [&](int i) {
        const float progress = float(i) / 599.0f * float(route.size() - 1);
        const auto seg = std::min(static_cast<std::size_t>(progress), route.size() - 2);
        const float alpha = progress - float(seg);
        lookAt(camera, glm::mix(route[seg].first, route[seg + 1].first, alpha),
                       glm::mix(route[seg].second, route[seg + 1].second, alpha));
    };

    for (int i = 0; i < 60; ++i) {
        registry.update(1.0 / 60.0);
        render(lighting, true, {});
        if (present()) throw std::runtime_error("EcoEnergy warmup interrupted");
    }

    std::ofstream frames(directory / "frames.csv");
    frames.exceptions(std::ios::badbit | std::ios::failbit);
    frames << "frame,seconds,x,y,z,frame_seconds\n";
    std::vector<double> durations;
    durations.reserve(600);
    glFinish();
    const double start = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        const double frameStart = glfwGetTime();
        moveOnRoute(i);
        registry.update(1.0 / 60.0);
        render(lighting, true, {});
        if (present()) throw std::runtime_error("EcoEnergy benchmark interrupted");
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("EcoEnergy benchmark GL error");
        const double duration = glfwGetTime() - frameStart;
        durations.push_back(duration);
        frames << i << ',' << glfwGetTime() - start << ',' << camera.position.x << ',' << camera.position.y << ','
               << camera.position.z << ',' << duration << '\n';
    }
    glFinish();
    const double seconds = glfwGetTime() - start;
    frames.close();

    // Segunda pasada para vídeo: captura fuera de la medición de FPS.
    const double recordingStart = glfwGetTime();
    for (int i = 0; i < 600; ++i) {
        moveOnRoute(i);
        registry.update(1.0 / 60.0);
        render(lighting, true, {});
        if (i % 5 == 0)
            capture(directory / "tour" / (std::to_string(1000 + i / 5) + ".ppm"), width, height);
        if (present()) throw std::runtime_error("EcoEnergy tour recording interrupted");
    }
    const double recordingSeconds = glfwGetTime() - recordingStart;

    std::sort(durations.begin(), durations.end());
    std::ofstream metrics(directory / "metrics.csv");
    metrics.exceptions(std::ios::badbit | std::ios::failbit);
    metrics << "metric,value\n"
            << "unique_models," << unique.size() << '\n'
            << "model_resources," << resources.size() << '\n'
            << "instances," << placements.size() << '\n'
            << "interactive_objects," << inter.objects().size() << '\n'
            << "animated_entities," << animatedIds.size() << '\n'
            << "scene_triangles," << sceneTriangles << '\n'
            << "source_triangles," << sourceTriangles << '\n'
            << "source_geometries," << geometryCount << '\n'
            << "render_geometries," << renderGeometryCount << '\n'
            << "source_textures," << textureCount << '\n'
            << "render_models," << renderModels.size() << '\n'
            << "cpu_model_payload_bytes_lower_bound," << cpuBytes << '\n'
            << "width," << width << '\n'
            << "height," << height << '\n'
            << "cpu_load_seconds," << loadSeconds << '\n'
            << "frames,600\n"
            << "seconds," << seconds << '\n'
            << "fps_mean," << 600.0 / seconds << '\n'
            << "recording_seconds," << recordingSeconds << '\n'
            << "frame_p95_ms," << durations.at(569) * 1000.0 << '\n';
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
        if (line.rfind("VmRSS:", 0) == 0) metrics << "process_rss_kib," << std::stoull(line.substr(6)) << '\n';
#endif
    metrics.close();
    std::cout << "ECOENERGY SMOKE PASS: unique=" << unique.size() << " resources=" << resources.size()
              << " instances=" << placements.size() << " triangles=" << sceneTriangles
              << " interactions=6 animations=8 load_seconds=" << loadSeconds
              << " frames=600 seconds=" << seconds << " fps_mean=" << 600.0 / seconds
              << " framebuffer=" << width << 'x' << height << std::endl;
}
}
