#pragma once
#include "world/ZoneNavigator.hpp"

namespace ecosphere {
void checkNavigationFramebuffer(const std::filesystem::path& directory, int width, int height,
    ZoneNavigator& navigator, Camera& camera, const std::function<bool(glm::vec3, bool, bool)>& navigate,
    const std::function<void()>& render, const std::function<void()>& present,
    const std::function<void(bool, const std::string&)>& renderContent, bool integral = false,
    int swapInterval = 1);
}
