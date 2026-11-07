#pragma once
#include "world/LightingDemo.hpp"
#include "world/LightingDemoSettings.hpp"

namespace ecosphere::chrome_experiment {
inline const glm::vec3 center = demo::materialPositions[2].position;
inline constexpr int probeSize = 512;
struct Scene {
    std::shared_ptr<Model> surroundings, original, chrome;
};
inline Scene makeScene() {
    Scene scene;
    scene.surroundings = makeLightingDemo();
    scene.original = std::make_shared<Model>();
    scene.original->meshes.push_back(scene.surroundings->meshes.at(2));
    auto node = scene.surroundings->nodes.at(2);
    node.meshes = {0};
    scene.original->nodes.push_back(node);
    scene.surroundings->nodes[2].visible = false; // Exclude the reflector from its own capture.
    scene.chrome = std::make_shared<Model>(*scene.original);
    scene.chrome->meshes[0].color = {.92f,.92f,.92f,1};
    scene.chrome->meshes[0].material.roughness = .04f;
    const auto box = [&](glm::vec3 position, glm::vec3 scale, glm::vec4 color, bool marker=false) {
        auto mesh = makeUnitCube(); mesh.color = color; mesh.material = demo::matte;
        mesh.material.unlit = marker;
        ModelNode part; part.name = "experimental-room";
        part.local = Transform{position,{0,0,0},scale}.matrix();
        part.meshes = {scene.surroundings->meshes.size()};
        scene.surroundings->meshes.push_back(std::move(mesh));
        scene.surroundings->nodes.push_back(std::move(part));
    };
    // Real room geometry, shared by A/B/C; the sphere has no painted reflection texture.
    for(int z=0;z<6;++z) for(int x=0;x<6;++x) {
        const float shade=(x+z)%2 ? .12f : .65f;
        box(center+glm::vec3((x-2.5f)*2,-2.70f,(z-2.5f)*2),{2,.1f,2},{shade,shade,shade,1});
    }
    box(center+glm::vec3(0,-.1f,-6),{12,5.4f,.16f},{.08f,.13f,.18f,1});
    box(center+glm::vec3(-2,0,-5.8f),{2,3.8f,.12f},{.04f,.53f,.56f,1});
    box(center+glm::vec3(2,0,-5.8f),{2,3.8f,.12f},{.95f,.65f,.1f,1});
    for(float x:{-3.5f,3.5f}) {
        box(center+glm::vec3(x,0,4),{1.4f,4.8f,.16f},{.9f,.92f,.9f,1},true);
        box(center+glm::vec3(x,0,4.12f),{.14f,4.8f,.10f},{.05f,.12f,.16f,1},true);
    }
    box(center+glm::vec3(0,3,0),{6,.12f,4},{.9f,.92f,.9f,1},true);
    return scene;
}
}
