#include "scene/Scene.hpp"
#include "camera/Camera.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a - b) < 0.0001f; }
template<class Function>
void rejects(Function function) {
    try { function(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected invalid_argument");
}

void transforms() {
    const ecosphere::Transform local{{3, 4, 5}, {0, 0, 90}, {2, 3, 4}};
    require(near(glm::vec3(local.matrix() * glm::vec4(1, 0, 0, 1)), {3, 6, 5}), "T*R*S numeric case");
    const ecosphere::Transform euler{{3, 4, 5}, {90, 90, 90}, {2, 3, 4}};
    require(near(glm::vec3(euler.matrix() * glm::vec4(1, 2, 3, 1)), {15, 10, 3}), "Rz*Ry*Rx order");

    ecosphere::SceneNode root;
    auto& parent = root.addChild({{10, 0, 0}, {0, 90, 0}, {1, 1, 1}}, {1, 0, 0});
    auto& child = parent.addChild({{2, 0, 0}, {0, 0, 0}, {1, 1, 1}}, {0, 1, 0});
    glm::vec3 childOrigin{};
    int visited = 0;
    const auto visit = [&](const ecosphere::SceneNode& node, const glm::mat4& world) {
        ++visited;
        if (&node == &child) childOrigin = glm::vec3(world * glm::vec4(0, 0, 0, 1));
    };
    root.forEach(visit);
    require(visited == 2 && near(childOrigin, {10, 0, -2}), "Parent translation/rotation must reach child");
    parent.transform.position.x = 20;
    parent.transform.rotationDegrees.y = 180;
    root.forEach(visit);
    require(near(childOrigin, {18, 0, 0}), "Changed parent must immediately affect world matrix");
    require(child.transform.position == glm::vec3(2, 0, 0), "World traversal must preserve local transform");
    static_assert(!std::is_copy_constructible_v<ecosphere::SceneNode>);
    static_assert(!std::is_move_constructible_v<ecosphere::SceneNode>);
    ecosphere::Scene demo;
    int objects = 0;
    demo.forEach([&](const ecosphere::SceneNode&, const glm::mat4&) { ++objects; });
    require(objects == 5, "Demo needs ground, three cubes and child");
    std::cout << "Local T*R*S, Euler order, hierarchy and demo traversal PASS\n";
}

void cameras() {
    ecosphere::Camera first;
    first.pitchDegrees = 45;
    ecosphere::Camera free = first;
    free.setMode(ecosphere::CameraMode::free);
    ecosphere::CameraInput input;
    input.forward = 1;
    first.update(input, 0.1);
    free.update(input, 0.1);
    require(near(first.position.y, ecosphere::demo::observerHeight), "First-person must stay at observer height");
    require(free.position.y > first.position.y, "Free forward must follow pitched view");
    input = {};
    input.vertical = 1;
    const auto before = first.position;
    first.update(input, 0.1);
    const float freeHeight = free.position.y;
    free.update(input, 0.1);
    require(near(first.position, before), "First-person must ignore vertical input");
    require(near(free.position.y - freeHeight, free.speed * 0.1f), "Free ascent speed");
    input.vertical = -1;
    free.update(input, 0.1);
    require(near(free.position.y, freeHeight), "Free descent");
    free.setMode(ecosphere::CameraMode::firstPerson);
    require(near(free.position.y, ecosphere::demo::observerHeight), "F1 restores observer height");

    ecosphere::Camera camera;
    camera.yawDegrees = -90;
    camera.pitchDegrees = 0;
    require(near(glm::vec3(camera.view() * glm::vec4(camera.position, 1)), {0, 0, 0}), "View maps eye to origin");
    require(near(glm::vec3(camera.view() * glm::vec4(camera.position + glm::vec3(0, 0, -1), 1)), {0, 0, -1}), "View points to -Z");
    const auto projection = camera.projection(1600, 900);
    require(projection.has_value(), "Positive framebuffer must produce projection");
    require(near((*projection)[1][1] / (*projection)[0][0], 1600.0f / 900), "Projection uses framebuffer aspect");
    require(near((*projection)[1][1], 1.0f / std::tan(glm::radians(camera.fovDegrees) / 2)), "Vertical FOV");
    for (const auto distance : {ecosphere::demo::nearPlane, ecosphere::demo::farPlane}) {
        const glm::vec4 clip = *projection * glm::vec4(0, 0, -distance, 1);
        require(near(clip.z / clip.w, distance == ecosphere::demo::nearPlane ? -1.0f : 1.0f), "OpenGL near/far clip mapping");
    }
    const auto tall = camera.projection(400, 900);
    require(near((*tall)[1][1] / (*tall)[0][0], 400.0f / 900), "Resized framebuffer aspect");
    require(!camera.projection(0, 900) && !camera.projection(900, 0) && !camera.projection(-1, 0), "Empty/minimized framebuffer");
    for (float invalid : {0.0f, 180.0f, std::numeric_limits<float>::quiet_NaN()}) {
        camera.fovDegrees = invalid;
        rejects([&] { camera.projection(800, 600); });
    }
    std::cout << "First-person/free movement, view, FOV, near/far, aspect/zero framebuffer PASS\n";
}

void inputAndTime() {
    ecosphere::Camera a, b;
    ecosphere::CameraInput input;
    input.forward = 1;
    for (int frame = 0; frame < 30; ++frame) a.update(input, 1.0 / 30);
    for (int frame = 0; frame < 144; ++frame) b.update(input, 1.0 / 144);
    require(near(a.position, b.position), "Movement must be independent of FPS");
    require(near(glm::length(a.position - ecosphere::demo::cameraStart), a.speed), "One second means speed meters");
    ecosphere::Camera diagonal;
    input.right = 1;
    diagonal.update(input, 0.1);
    require(near(glm::length(diagonal.position - ecosphere::demo::cameraStart), diagonal.speed * 0.1f), "Diagonal must not speed up");
    input = {};
    input.pitch = 1;
    input.yaw = 1;
    for (int i = 0; i < 100; ++i) a.update(input, 0.1);
    require(near(a.pitchDegrees, ecosphere::demo::pitchLimitDegrees), "Upper pitch clamp");
    require(std::abs(a.yawDegrees) <= 180, "Yaw wrap");
    input.pitch = -1;
    for (int i = 0; i < 100; ++i) a.update(input, 0.1);
    require(near(a.pitchDegrees, -ecosphere::demo::pitchLimitDegrees), "Lower pitch clamp");
    a = ecosphere::Camera{};
    b = a;
    input = {};
    input.forward = 1;
    a.update(input, 5.0);
    b.update(input, ecosphere::demo::maxDeltaSeconds);
    require(near(a.position, b.position), "Long pause must clamp movement");
    const auto before = a.position;
    a.update(input, ecosphere::cameraDeltaSeconds(5.0, false));
    a.update(input, 0.0);
    require(near(a.position, before), "Unfocused and zero delta must not move");
    for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        rejects([&] { a.update(input, invalid); });
    }
    std::cout << "30/144 FPS, diagonal speed, arrows/pitch, pause, focus, invalid delta PASS\n";
}
}

int main() {
    try {
        transforms();
        cameras();
        inputAndTime();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
