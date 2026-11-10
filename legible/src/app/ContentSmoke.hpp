#pragma once
#include "content/ContentRegistry.hpp"
#include "camera/Camera.hpp"
#include <functional>

namespace ecosphere {
void checkContentFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, const std::string& owner, Camera& camera,
    const glm::mat4& projection, const std::function<void(bool)>& render);
}
