#pragma once
#include <ecosphere/sdk/IModule.hpp>
#include <ecosphere/sdk/Content.hpp>
#include <memory>

namespace lesson {
struct Settings {
    std::string owner="lesson";
    std::string model="models/blender_probe_unlit.glb";
    float rotationSpeed=35;
    glm::vec3 position{0,1,0};
    float scale=.8f;
};
std::unique_ptr<ecosphere::IModule> makeModule(Settings settings = {});
}
