#pragma once
#include "AquaBioSettings.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <vector>
#include <string>

namespace ecosphere {
std::string aquaBioModelId(aquabio::Model model);
std::vector<EntityDescriptor> makeAquaBioInstances(const aquabio::Settings& settings = aquabio::settings);
std::unique_ptr<IModule> makeAquaBio(const aquabio::Settings& settings);
}
