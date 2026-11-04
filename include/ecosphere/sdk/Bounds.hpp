#pragma once
#include <glm/vec3.hpp>

namespace ecosphere {
struct Aabb { glm::vec3 minimum{-0.5f}, maximum{0.5f}; };
}
