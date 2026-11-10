#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <string_view>
#include <vector>

namespace ecosphere::shared {
// los recursos compartidos usan ids que empiezan con SHARED-
inline bool isShared(std::string_view id) { return id.rfind("SHARED-", 0) == 0; }
std::vector<EntityDescriptor> instances(std::string_view owner);
}
