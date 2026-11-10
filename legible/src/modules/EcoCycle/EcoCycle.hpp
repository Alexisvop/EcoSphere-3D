#pragma once
#include "EcoCycleSettings.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <string>
#include <vector>

namespace ecosphere {
std::string ecoCycleModelId(ecocycle::Model model);
std::vector<EntityDescriptor> makeEcoCycleInstances(const ecocycle::Settings& settings=ecocycle::settings);
std::unique_ptr<IModule> makeEcoCycle(const ecocycle::Settings& settings);
}
