#include "scene/Lighting.hpp"
#include "assets/Material.hpp"
#include "world/LightingDemo.hpp"
#include "world/LightingDemoSettings.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Action> void rejects(Action action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid parameters accepted");
}
}
int main() {
    try {
        using namespace ecosphere;
        validateLighting(demo::lighting,{0,1,5});
        for (int test=0; test<8; ++test) {
            auto s=demo::lighting;
            if(test==0) s.point.intensity=-1;
            if(test==1) s.directional.vector={0,0,0};
            if(test==2) s.attenuation={0,0,0};
            if(test==3) s.point.color.x=2;
            if(test==4) s.environment.fresnelPower=0;
            if(test==5) s.environment.intensity=std::numeric_limits<float>::infinity();
            if(test==6) s.ambient=std::numeric_limits<float>::quiet_NaN();
            if(test==7) s.point.vector.x=std::numeric_limits<float>::infinity();
            rejects([&] { validateLighting(s,{0,1,5}); });
        }
        const auto world=glm::rotate(glm::scale(glm::mat4(1),{2,3,0.5f}),0.5f,glm::vec3(0,1,0));
        const glm::vec3 tangent=glm::normalize(glm::vec3(1,2,3));
        const glm::vec3 normal=glm::normalize(glm::cross(tangent,glm::vec3(0,1,0)));
        check(std::abs(glm::dot(normalMatrix(world)*normal,glm::mat3(world)*tangent))<0.00001f,"Normal lost perpendicularity");
        rejects([] { normalMatrix(glm::scale(glm::mat4(1),{1,0,1})); });
        auto invalid=glm::mat4(1); invalid[1][1]=std::numeric_limits<float>::quiet_NaN();
        rejects([&] { normalMatrix(invalid); });
        const glm::vec3 f0(0.04f);
        check(glm::length(fresnelSchlick(1,f0)-f0)<0.000001f,"Fresnel normal incidence");
        check(glm::length(fresnelSchlick(0,f0)-glm::vec3(1))<0.000001f,"Fresnel grazing");
        check(fresnelSchlick(0.2f,f0).x>fresnelSchlick(0.8f,f0).x,"Fresnel monotonicity");
        check(fresnelSchlick(-1,f0)==glm::vec3(1) && fresnelSchlick(2,f0)==f0,"Fresnel clamp");
        rejects([&] { fresnelSchlick(std::numeric_limits<float>::quiet_NaN(),f0); });
        check(std::abs(blinnExponent(1)-2)<0.00001f && std::abs(blinnExponent(0)-256)<0.00001f,"Roughness/exponent mapping");
        auto m=demo::plastic; m.roughness=-1; rejects([&] { validateMaterial(m); });
        const auto model=makeLightingDemo();
        check(model->nodes.size()==6 && model->meshes.size()==6,"Showcase content");
        for(const auto& mesh:model->meshes) {
            validateMaterial(mesh.material);
            for(const auto& v:mesh.vertices) check(finiteVector(v.position) && std::abs(glm::length(v.normal)-1)<0.0001f,"Primitive normals");
            for(auto i:mesh.indices) check(i<mesh.vertices.size(),"Primitive index");
            for(std::size_t i=0;i<mesh.indices.size();i+=3) {
                const auto& a=mesh.vertices[mesh.indices[i]];
                const auto& b=mesh.vertices[mesh.indices[i+1]];
                const auto& c=mesh.vertices[mesh.indices[i+2]];
                check(glm::dot(glm::cross(b.position-a.position,c.position-a.position),a.normal)>-0.000001f,"Primitive winding");
            }
        }
        std::cout << "Lighting CPU PASS: ranges/NaN, normals nonuniform/singular, Fresnel limits, exponent, primitives/materials\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
