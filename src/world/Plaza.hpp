#pragma once
#include "PlazaSettings.hpp"
#include <ecosphere/sdk/IModule.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ecosphere {
std::string plazaModelId(plaza::Model model);
std::vector<EntityDescriptor> makePlazaInstances(const plaza::Settings& settings = plaza::settings);
std::unique_ptr<IModule> makePlaza(const plaza::Settings& settings = plaza::settings);
}
