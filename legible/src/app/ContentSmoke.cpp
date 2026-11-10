#include "ContentSmoke.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;

// lee color y profundidad y revisa que haya contenido dibujado
Pixels readBackBuffer(int width, int height, bool includeContent) {
    Pixels pixels(static_cast<std::size_t>(width) * height * 3);
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Content readback failed");
    std::size_t occupied = 0;
    for (float value : depth) {
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::runtime_error("Invalid content depth");
        occupied += value < 1;
    }
    if (includeContent && occupied < 100) throw std::runtime_error("Registered content missing depth");
    return pixels;
}

// imagen PPM con las filas de arriba hacia abajo
void writePpm(const std::filesystem::path& path, const Pixels& pixels, int width, int height) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        const auto offset = static_cast<std::size_t>(y) * width * 3;
        const auto* row = reinterpret_cast<const char*>(pixels.data() + offset);
        file.write(row, width * 3);
    }
}

void writeStates(std::ostream& states, const char* stage, ContentRegistry& registry) {
    for (const auto& entity : registry.placements()) {
        const auto& matrix = entity.world;
        states << stage << ',' << entity.owner << ',' << entity.id << ','
               << matrix[3].x << ',' << matrix[3].y << ',' << matrix[3].z << ','
               << matrix[0].x << ',' << matrix[0].y << ',' << matrix[0].z << '\n';
    }
}

void requireChange(const Pixels& before, const Pixels& after) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < before.size(); i += 3) {
        int delta = 0;
        for (int c = 0; c < 3; ++c) delta += std::abs(int(before[i + c]) - int(after[i + c]));
        count += delta > 12;
    }
    if (count < 100) throw std::runtime_error("Content smoke missing visible change");
    std::cout << "Content changed pixels=" << count << '\n';
}

// acerca la cámara si el primer objeto quedó fuera de cuadro
void frameFirstObject(ContentRegistry& registry, const std::string& owner, Camera& camera,
    const glm::mat4& projection) {
    if (registry.interactions(owner).objects().empty())
        throw std::runtime_error("Module did not register interaction");
    const auto& first = registry.interactions(owner).objects().front();
    const auto centerFirst = registry.interactions(owner).world(first.id)
                           * glm::vec4((first.bounds.minimum + first.bounds.maximum) * .5f, 1);
    const auto projected = projection * camera.view() * centerFirst;
    const bool outside = projected.w <= 0 || std::abs(projected.x / projected.w) > .9f
                      || std::abs(projected.y / projected.w) > .9f;
    if (!outside) return;
    const auto center = glm::vec3(centerFirst);
    const float distance = std::max(4.0f, glm::length(first.bounds.maximum - first.bounds.minimum) * 1.6f);
    camera.position = center + glm::vec3(0, 0, distance);
    camera.position.y = 1.7f;
    camera.yawDegrees = -90;
    const float rise = center.y - camera.position.y;
    camera.pitchDegrees = glm::degrees(std::atan2(rise, camera.position.z - center.z));
}
}

void checkContentFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, const std::string& owner, Camera& camera,
    const glm::mat4& projection, const std::function<void(bool)>& render) {
    std::filesystem::create_directories(directory);
    std::ofstream states(directory / "states.csv");
    states.exceptions(std::ios::badbit | std::ios::failbit);
    states << "stage,owner,id,x,y,z,m00,m01,m02\n";
    const auto capture = [&](const char* stage, bool includeContent) {
        render(includeContent);
        glFinish();
        auto pixels = readBackBuffer(width, height, includeContent);
        writePpm(directory / (std::string(stage) + ".ppm"), pixels, width, height);
        writeStates(states, stage, registry);
        return pixels;
    };

    frameFirstObject(registry, owner, camera, projection);
    const auto background = capture("background", false);
    const auto initial = capture("initial", true);
    requireChange(background, initial);

    // seleccionar con rayo el primer objeto
    auto& system = registry.interactions(owner);
    if (system.objects().empty()) throw std::runtime_error("Module did not register interaction");
    const auto id = system.objects().front().id;
    const auto& box = system.object(id).bounds;
    const auto center = system.world(id) * glm::vec4((box.minimum + box.maximum) * 0.5f, 1);
    const auto clip = projection * camera.view() * center;
    const auto ray = cameraRay(camera.view(), projection, glm::vec2(clip) / clip.w);
    if (system.pick(ray) != std::optional<std::string>(id))
        throw std::runtime_error("Registered object selection failed");

    system.manipulate({0.6f, 0.3f, 0}, glm::angleAxis(glm::radians(25.0f), glm::vec3(0, 1, 0)), 1.2f);
    const auto manipulated = capture("manipulated", true);
    requireChange(initial, manipulated);
    registry.update(0.5);
    requireChange(manipulated, capture("animated", true));
    states.close();
    std::cout << "CONTENT SMOKE PASS: " << owner
              << "; registered GLB/material, selection/TRS, animation and real GPU readback\n";
}
}
