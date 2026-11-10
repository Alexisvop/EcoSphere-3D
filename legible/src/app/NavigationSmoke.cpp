#include "NavigationSmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;

void require(bool ok, const char* text) {
    if (!ok) throw std::runtime_error(text);
}

Pixels readRgb(int width, int height) {
    Pixels rgb(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    require(glGetError() == GL_NO_ERROR, "Navigation framebuffer capture failed");
    return rgb;
}

// guarda el framebuffer como imagen PPM
void capture(const std::filesystem::path& path, int width, int height) {
    const auto rgb = readRgb(width, height);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        const auto offset = static_cast<std::size_t>(y) * width * 3;
        const auto* row = reinterpret_cast<const char*>(rgb.data() + offset);
        out.write(row, width * 3);
    }
}

// memoria residente en KiB, solo existe en Linux
std::size_t rss() {
    std::ifstream in("/proc/self/status");
    std::string key;
    std::string line;
    std::size_t value = 0;
    while (in >> key) {
        if (key == "VmRSS:") {
            in >> value;
            return value;
        }
        std::getline(in, line);
    }
    return 0;
}

void look(Camera& camera, glm::vec3 position, glm::vec3 target) {
    const auto direction = target - position;
    camera.position = position;
    camera.yawDegrees = glm::degrees(std::atan2(direction.z, direction.x));
    const float horizontal = glm::length(glm::vec2(direction.x, direction.z));
    camera.pitchDegrees = glm::degrees(std::atan2(direction.y, horizontal));
}

std::size_t countChanged(const Pixels& after, const Pixels& before) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < after.size(); i += 3) {
        int delta = 0;
        for (int c = 0; c < 3; ++c) delta += std::abs(int(after[i + c]) - int(before[i + c]));
        count += delta > 12;
    }
    return count;
}

glm::vec3 worldCenter(const ContentPlacement& placement) {
    glm::vec3 low(std::numeric_limits<float>::infinity());
    glm::vec3 high = -low;
    placement.model->forEach([&](std::size_t index, const glm::mat4& world) {
        for (const auto& vertex : placement.model->meshes.at(index).vertices) {
            const auto point = glm::vec3(world * glm::vec4(vertex.position, 1));
            low = glm::min(low, point);
            high = glm::max(high, point);
        }
    }, placement.world);
    return (low + high) * .5f;
}

// estado de la prueba: archivos CSV abiertos y funciones de la app
class NavigationRun {
public:
    NavigationRun(const std::filesystem::path& directory, int width, int height, ZoneNavigator& navigator,
        Camera& camera, const std::function<bool(glm::vec3, bool, bool)>& navigate,
        const std::function<void()>& render, const std::function<void()>& present,
        const std::function<void(bool, const std::string&)>& renderContent, int swapInterval)
        : directory_(directory), width_(width), height_(height), navigator_(navigator), camera_(camera),
          navigate_(navigate), render_(render), present_(present), renderContent_(renderContent),
          swapInterval_(swapInterval) {
        transitions_.open(directory / "transitions.csv");
        transitions_.exceptions(std::ios::badbit | std::ios::failbit);
        transitions_ << "index,from,to,seconds,entities,resources,meshes,textures,rss_kib,"
                        "expired_outgoing_resources,reused_shared_references\n";
        fps_.open(directory / "fps.csv");
        fps_.exceptions(std::ios::badbit | std::ios::failbit);
        fps_ << "zone,width,height,swap_interval,frames,seconds,"
                "fps_mean,frame_mean_ms,frame_p95_ms,rss_kib\n";
        sharedViews_.open(directory / "shared-views.csv");
        sharedViews_.exceptions(std::ios::badbit | std::ios::failbit);
        sharedViews_ << "owner,instance_id,model_id,capture,camera_x,camera_y,camera_z,"
                        "target_x,target_y,target_z,contribution_pixels\n";
    }

    unsigned transitionCount() const { return index_; }

    void frame() {
        navigator_.update(1.0 / 60);
        render_();
        present_();
        require(glGetError() == GL_NO_ERROR, "Navigation GL error");
    }

    void shot(const std::string& name) {
        render_();
        capture(directory_ / (name + ".ppm"), width_, height_);
        present_();
    }

    void saveTourFrame() {
        capture(directory_ / "tour" / (std::to_string(1000 + tourSample_) + ".ppm"), width_, height_);
    }

    unsigned nextTourSample() { return tourSample_++; }

    // cruza un portal (o regresa a la plaza) y anota qué recursos se liberaron
    void change(const Portal& portal, bool home) {
        navigate_(camera_.position, false, false);
        const auto from = navigator_.active();
        std::vector<std::weak_ptr<const Model>> outgoing;
        for (const auto& placement : navigator_.placements()) outgoing.emplace_back(placement.resource);
        glFinish();
        const auto start = glfwGetTime();
        const auto destination = home ? camera_.position : portal.center;
        require(navigate_(destination, !home, home), "Navigation transition failed");
        glFinish();
        const auto seconds = glfwGetTime() - start;
        for (int i = 0; i < 3; ++i) frame();

        std::set<const Model*> resources;
        std::set<const Model*> incoming;
        std::size_t meshes = 0;
        std::size_t textures = 0;
        std::size_t expired = 0;
        std::size_t reused = 0;
        for (const auto& placement : navigator_.placements()) incoming.insert(placement.resource.get());
        for (const auto& placement : navigator_.placements()) {
            if (!resources.insert(placement.model.get()).second) continue;
            meshes += placement.model->meshes.size();
            textures += placement.model->textures.size();
        }
        // lo que sigue vivo debe ser compartido con la zona nueva
        for (const auto& resource : outgoing) {
            if (resource.expired()) {
                ++expired;
            } else {
                require(incoming.count(resource.lock().get()) == 1,
                    "Outgoing resource leaked without an incoming owner");
                ++reused;
            }
        }
        require(expired + reused == outgoing.size(), "Outgoing/shared reference accounting mismatch");
        transitions_ << index_++ << ',' << zoneId(from) << ',' << zoneId(navigator_.active()) << ','
                     << seconds << ',' << navigator_.placements().size() << ',' << resources.size() << ','
                     << meshes << ','
                     << textures << ',' << rss() << ',' << expired << ',' << reused << '\n';
        navigate_(camera_.position, false, false);
    }

    // 30 cuadros de calentamiento y 300 medidos
    void benchmark() {
        for (int i = 0; i < 30; ++i) frame();
        std::vector<double> times;
        glFinish();
        const auto start = glfwGetTime();
        for (int i = 0; i < 300; ++i) {
            const auto frameStart = glfwGetTime();
            frame();
            times.push_back(glfwGetTime() - frameStart);
        }
        glFinish();
        const auto duration = glfwGetTime() - start;
        std::sort(times.begin(), times.end());
        const auto meanMs = std::accumulate(times.begin(), times.end(), 0.0) / times.size() * 1000;
        fps_ << zoneId(navigator_.active()) << ',' << width_ << ',' << height_ << ','
             << swapInterval_ << ",300,"
             << duration << ',' << 300 / duration << ',' << meanMs << ',' << times.at(284) * 1000 << ','
             << rss() << '\n';
    }

    // cada modelo compartido de la zona, visto de frente y aislado
    void sharedShots() {
        const auto saved = camera_;
        for (const auto& placement : navigator_.placements()) {
            if (placement.id.rfind("SHARED-", 0) != 0) continue;
            const auto center = worldCenter(placement);
            const glm::vec3 observer{center.x, 1.7f, center.z + 3};
            look(camera_, observer, center);
            renderContent_(false, {});
            const auto background = readRgb(width_, height_);
            renderContent_(true, placement.id);
            const auto isolated = readRgb(width_, height_);
            const auto changed = countChanged(isolated, background);
            require(changed > 100, "Shared model made no framebuffer contribution");
            const auto name = "shared/" + placement.owner + "-" + placement.id.substr(0, 10);
            shot(name);
            sharedViews_ << placement.owner << ',' << placement.id << ',' << placement.id.substr(0, 10) << ','
                         << name << ',' << observer.x << ',' << observer.y << ',' << observer.z << ','
                         << center.x << ',' << center.y << ',' << center.z << ',' << changed << '\n';
        }
        camera_ = saved;
    }

private:
    const std::filesystem::path& directory_;
    int width_;
    int height_;
    ZoneNavigator& navigator_;
    Camera& camera_;
    const std::function<bool(glm::vec3, bool, bool)>& navigate_;
    const std::function<void()>& render_;
    const std::function<void()>& present_;
    const std::function<void(bool, const std::string&)>& renderContent_;
    int swapInterval_;
    std::ofstream transitions_;
    std::ofstream fps_;
    std::ofstream sharedViews_;
    unsigned index_ = 0;
    unsigned tourSample_ = 0;
};
}

void checkNavigationFramebuffer(const std::filesystem::path& directory, int width, int height,
    ZoneNavigator& navigator, Camera& camera, const std::function<bool(glm::vec3, bool, bool)>& navigate,
    const std::function<void()>& render, const std::function<void()>& present,
    const std::function<void(bool, const std::string&)>& renderContent, bool integral, int swapInterval) {
    std::filesystem::create_directories(directory / "tour");
    std::filesystem::create_directories(directory / "shared");
    const ZoneSettings settings;
    NavigationRun run(directory, width, height, navigator, camera, navigate, render, present, renderContent,
        swapInterval);

    // ---- Plaza central ----
    run.shot("hub-entry");
    run.benchmark();
    run.sharedShots();
    look(camera, {24, 24, 29}, {0, 0, 0});
    run.shot("hub-overview");
    look(camera, {-2.8f, 1.7f, 14.7f}, {-2.8f, 2.1f, 11.2f});
    run.shot("signpost-first-person");
    if (integral) {
        const std::vector<std::pair<std::string, glm::vec3>> signpostViews{
            {"left", {-5.1f, 1.7f, 14.5f}}, {"right", {-.5f, 1.7f, 14.5f}}, {"path", {0, 1.7f, 16}}};
        for (const auto& view : signpostViews) {
            look(camera, view.second, {-2.8f, 2.05f, 11.2f});
            run.shot("signpost-" + view.first);
        }
    }
    const auto panel = plaza::settings.panels[0];
    look(camera, panel + glm::vec3(0, 1.7f, 3.5f), panel + glm::vec3(0, 1.65f, 0));
    run.shot("panel-first-person");
    look(camera, {3.8f, 1.7f, 13}, {3.8f, 1.95f, 9.5f});
    run.shot("kiosk-first-person");

    // ---- Visita a cada zona ----
    for (const auto& portal : settings.portals) {
        const auto name = std::string(zoneId(portal.destination));
        look(camera, portal.returnCamera.make().position, {portal.center.x, 2.5f, portal.center.z});
        run.shot(name + "-access");
        camera.position = portal.center;
        run.shot(name + "-prompt");
        run.change(portal, false);
        run.shot(name + "-inside");
        run.benchmark();
        run.sharedShots();
        run.change(portal, true);
        run.shot(name + "-return");
    }

    // ---- Muchas idas y vueltas para revisar memoria ----
    std::ofstream endurance(directory / "endurance.csv");
    endurance.exceptions(std::ios::badbit | std::ios::failbit);
    endurance << "cycle,transitions,rss_kib\n0," << run.transitionCount() << ',' << rss() << '\n';
    const int cycles = integral ? 100 : 30;
    for (int cycle = 0; cycle < cycles; ++cycle) {
        const auto& portal = settings.portals.at(static_cast<std::size_t>(cycle) % 3);
        run.change(portal, false);
        run.change(portal, true);
        endurance << cycle + 1 << ',' << run.transitionCount() << ',' << rss() << '\n';
    }
    run.shot(integral ? "hub-after-103-visits" : "hub-after-33-visits");

    // ---- Video: caminar a cada portal, entrar y volver ----
    std::ofstream recording(directory / "recording.csv");
    recording << "frame,seconds,zone\n";
    const auto start = glfwGetTime();
    const auto recordSample = [&] {
        run.saveTourFrame();
        recording << run.nextTourSample() << ',' << glfwGetTime() - start << ','
                  << zoneId(navigator.active()) << '\n';
    };
    for (const auto& portal : settings.portals) {
        const auto entry = portal.returnCamera.make().position;
        for (int i = 0; i < 60; ++i) {
            const auto position = glm::mix(entry, portal.center, float(i) / 59);
            look(camera, position, {portal.center.x, 2.4f, portal.center.z});
            render();
            if (i % 5 == 0) recordSample();
            present();
        }
        run.change(portal, false);
        for (int i = 0; i < 60; ++i) {
            run.frame();
            if (i % 5 == 0) {
                render();
                recordSample();
            }
        }
        run.change(portal, true);
    }
    std::cout << "NAVIGATION GPU PASS: 4 zones, " << run.transitionCount()
              << " transitions, exclusive resources released/shared incoming references retained, "
                 "12 shared-model framebuffer views, 1280x720 requested; actual "
              << width << 'x' << height << std::endl;
}
}
