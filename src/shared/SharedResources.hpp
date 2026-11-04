#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <string_view>
#include <vector>

namespace ecosphere::shared {
// IDs locales distintos; la ruta canónica identifica el mismo recurso para todos los owners.
inline bool isShared(std::string_view id) { return id.rfind("SHARED-",0)==0; }
std::vector<EntityDescriptor> instances(std::string_view owner);
}
