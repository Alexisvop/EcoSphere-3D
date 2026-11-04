#include "SharedResources.hpp"
#include <array>
#include <stdexcept>

namespace ecosphere::shared {
std::vector<EntityDescriptor> instances(std::string_view owner) {
    if(owner!="hub" && owner!="ecoenergy" && owner!="aquabio" && owner!="ecocycle")
        throw std::invalid_argument("Unknown shared-resource owner");
    struct Placement {const char* owner;unsigned model;glm::vec3 position;};
    const std::array<Placement,12> placements{{
        {"hub",1,{-7,.01f,-2.8f}}, {"hub",4,{7,.01f,-2.8f}},
        {"ecoenergy",1,{-3.4f,.01f,8.5f}}, {"ecoenergy",3,{-15.6f,.01f,9.8f}}, {"ecoenergy",5,{13.5f,.01f,8.5f}},
        {"aquabio",1,{5,.01f,11}}, {"aquabio",2,{-7,.01f,12}}, {"aquabio",3,{7,.01f,17}},
        {"ecocycle",1,{4.5f,.01f,-7.5f}}, {"ecocycle",2,{10,.01f,8.5f}},
        {"ecocycle",4,{-5,.01f,9}}, {"ecocycle",5,{5,.01f,-15.5f}}
    }};
    std::vector<EntityDescriptor> result;
    for(const auto& p:placements) if(owner==p.owner) {
        const auto model="SHARED-00"+std::to_string(p.model);
        EntityDescriptor e;e.id=model+"-"+p.owner;e.model="shared/models/"+model+".glb";
        e.transform.position=p.position;validatePose(e.transform);
        result.push_back(std::move(e));
    }
    return result;
}
}
