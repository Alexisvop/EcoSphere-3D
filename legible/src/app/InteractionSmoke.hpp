#pragma once
#include "world/InteractionAnimationDemo.hpp"
#include "camera/Camera.hpp"
#include <filesystem>
#include <functional>

namespace ecosphere {
void checkInteractionFramebuffer(const std::filesystem::path& directory, int width, int height,
    InteractionAnimationDemo& demo, const Camera& camera, const glm::mat4& projection,
    const std::function<void()>& render);
}
