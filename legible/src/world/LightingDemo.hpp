#pragma once
#include "assets/Model.hpp"
#include <memory>

namespace ecosphere {
Mesh makeUnitCube();
std::shared_ptr<Model> makeLightingDemo();
}
