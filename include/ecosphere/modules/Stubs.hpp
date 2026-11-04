#pragma once
#include <ecosphere/sdk/IModule.hpp>
#include <memory>

namespace ecosphere {
std::unique_ptr<IModule> makeEcoEnergy();
std::unique_ptr<IModule> makeAquaBio();
std::unique_ptr<IModule> makeEcoCycle();
}
