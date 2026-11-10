#include "SharedResources.hpp"
#include <array>
#include <stdexcept>

namespace ecosphere::shared {
namespace {
struct Placement {
    const char* owner;
    unsigned model;
    glm::vec3 position;
};

// modelos SHARED-00N que se repiten en cada zona
const std::array<Placement, 12> placements{{
    {"hub",       1, {-7, .01f, -2.8f}},
    {"hub",       4, {7, .01f, -2.8f}},
    {"ecoenergy", 1, {-3.4f, .01f, 8.5f}},
    {"ecoenergy", 3, {-15.6f, .01f, 9.8f}},
    {"ecoenergy", 5, {13.5f, .01f, 8.5f}},
    {"aquabio",   1, {5, .01f, 11}},
    {"aquabio",   2, {-7, .01f, 12}},
    {"aquabio",   3, {7, .01f, 17}},
    {"ecocycle",  1, {4.5f, .01f, -7.5f}},
    {"ecocycle",  2, {10, .01f, 8.5f}},
    {"ecocycle",  4, {-5, .01f, 9}},
    {"ecocycle",  5, {5, .01f, -15.5f}}
}};
}

std::vector<EntityDescriptor> instances(std::string_view owner) {
    if (owner != "hub" && owner != "ecoenergy" && owner != "aquabio" && owner != "ecocycle")
        throw std::invalid_argument("Unknown shared-resource owner");
    std::vector<EntityDescriptor> result;
    for (const auto& placement : placements) {
        if (owner != placement.owner) continue;
        const auto model = "SHARED-00" + std::to_string(placement.model);
        EntityDescriptor entity;
        entity.id = model + "-" + placement.owner;
        entity.model = "shared/models/" + model + ".glb";
        entity.transform.position = placement.position;
        validatePose(entity.transform);
        result.push_back(std::move(entity));
    }
    return result;
}
}
