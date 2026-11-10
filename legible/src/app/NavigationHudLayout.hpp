#pragma once
#include "assets/Model.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ecosphere {
// posición y renglones del panel, en píxeles de pantalla
struct NavigationHudLayout {
    float x = 10, y = 0, width = 0, height = 0, pixel = 2;
    std::vector<std::string> lines;
    bool truncated = false;
};

NavigationHudLayout layoutNavigationHud(const std::string& message, int width, int height);
std::shared_ptr<const Model> makeNavigationHudModel(const NavigationHudLayout& layout);
}
