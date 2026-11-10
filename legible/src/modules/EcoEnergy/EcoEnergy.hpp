#pragma once
#include "EcoEnergySettings.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ecosphere {
std::string ecoEnergyModelId(ecoenergy::Model model);
std::vector<EntityDescriptor> makeEcoEnergyInstances(
    const ecoenergy::Settings& settings = ecoenergy::settings);
std::unique_ptr<IModule> makeEcoEnergy(const ecoenergy::Settings& settings);
}
