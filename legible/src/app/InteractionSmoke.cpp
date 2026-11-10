#include "InteractionSmoke.hpp"
#include "InteractionDemoInput.hpp"
#include <glad/gl.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;

Pixels readBackBuffer(int width, int height) {
    Pixels pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Interaction readback failed");
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

// posición y primera columna de cada objeto
void writeStates(std::ostream& states, const char* stage, InteractionAnimationDemo& demo) {
    auto& system = demo.interaction;
    for (const auto& object : system.objects()) {
        const auto matrix = system.world(object.id);
        states << stage << ',' << demo.time() << ',' << object.id << ','
               << matrix[3].x << ',' << matrix[3].y << ',' << matrix[3].z << ','
               << matrix[0].x << ',' << matrix[0].y << ',' << matrix[0].z << '\n';
    }
}

void requireChange(const Pixels& before, const Pixels& after) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < before.size(); i += 3) {
        int delta = 0;
        for (int c = 0; c < 3; ++c) delta += std::abs(int(before[i + c]) - int(after[i + c]));
        if (delta > 12) ++count;
    }
    if (count < 30) throw std::runtime_error("Interaction smoke: missing visible change");
    std::cout << "Interaction changed pixels=" << count << std::endl;
}

bool matrixMoved(const glm::mat4& now, const glm::mat4& before) {
    bool differs = false;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) differs |= std::abs(now[c][r] - before[c][r]) > 0.001f;
    }
    return differs;
}
}

void checkInteractionFramebuffer(const std::filesystem::path& directory, int width, int height,
    InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
    const std::function<void()>& render) {
    std::filesystem::create_directories(directory);
    auto& system = demo.interaction;
    std::ofstream states(directory / "states.csv");
    states.exceptions(std::ios::badbit | std::ios::failbit);
    states << "stage,time,id,x,y,z,m00,m01,m02\n";
    const auto capture = [&](const char* stage) {
        demo.synchronize();
        render();
        glFinish();
        auto pixels = readBackBuffer(width, height);
        writePpm(directory / (std::string(stage) + ".ppm"), pixels, width, height);
        writeStates(states, stage, demo);
        logInteractionState(demo);
        return pixels;
    };

    // ---- Selección con rayo ----
    demo.restart();
    system.deselect();
    const auto initial = capture("time-zero");
    const auto first = system.world("simple-rotation") * glm::vec4(0, 0, 0, 1);
    const auto clip = projection * camera.view() * first;
    const auto ray = cameraRay(camera.view(), projection, glm::vec2(clip) / clip.w);
    if (system.pick(ray) != std::optional<std::string>("simple-rotation"))
        throw std::runtime_error("Interaction smoke: wrong ray selection");
    const auto selected = capture("selected");
    requireChange(initial, selected);

    // ---- Mover y tomar ----
    system.manipulate({0.35f, 0.2f, 0}, glm::angleAxis(glm::radians(25.0f), glm::vec3(0, 1, 0)), 1.25f);
    requireChange(selected, capture("manipulated"));
    system.take(cameraRay(camera.view(), projection), 3);
    requireChange(selected, capture("taken"));
    system.drop();
    system.deselect();

    // ---- Animaciones ----
    const auto before = system.world("keyframes");
    std::vector<glm::mat4> beforeAnimation;
    for (const auto& object : system.objects()) beforeAnimation.push_back(system.world(object.id));
    demo.update(0.5);
    const auto half = capture("time-half");
    for (std::size_t i = 0; i + 1 < system.objects().size(); ++i) {
        const auto matrix = system.world(system.objects()[i].id);
        if (!matrixMoved(matrix, beforeAnimation[i]))
            throw std::runtime_error("A demo animation did not change its transform");
    }
    demo.update(0.5);
    const auto second = capture("time-one");
    requireChange(half, second);
    demo.update(1);
    requireChange(second, capture("time-two"));
    if (glm::length(glm::vec3(system.world("keyframes")[3] - before[3])) < 0.05f)
        throw std::runtime_error("Keyframes did not move");

    // pausa
    demo.togglePause();
    const double paused = demo.time();
    demo.update(2);
    if (demo.time() != paused) throw std::runtime_error("Pause failed");

    demo.restart();
    system.select("imported");
    system.manipulate({0.6f, 0, 0}, glm::quat(1, 0, 0, 0), 1.1f);
    capture("imported-manipulated");
    states.close();
    std::cout << "INTERACTION SMOKE PASS: scripted CPU actions, real OpenGL captures; no OS input injection"
              << std::endl;
}
}
