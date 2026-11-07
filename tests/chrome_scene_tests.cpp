#include "experiments/ChromeScene.hpp"
#include <iostream>
#include <stdexcept>
using namespace ecosphere;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        const auto original=makeLightingDemo();
        const auto scene=chrome_experiment::makeScene();
        require(scene.surroundings->meshes.size()>original->meshes.size(),"Missing real capture surroundings");
        require(!scene.surroundings->nodes.at(2).visible,"Reflector included in capture");
        require(scene.original->meshes[0].color==demo::metalColor,"Original metal changed");
        const auto& chrome=scene.chrome->meshes[0];
        require(chrome.texture==-1 && scene.chrome->images.empty(),"Fake reflection/base texture");
        require(chrome.color==glm::vec4(.92f,.92f,.92f,1),"Chrome must be neutral");
        require(chrome.material.metallic==1 && chrome.material.roughness==.04f && chrome.material.reflection==1,"Chrome parameters");
        for(std::size_t i=0;i<original->meshes.size();++i) {
            const auto& a=original->meshes[i]; const auto& b=scene.surroundings->meshes[i];
            require(a.indices==b.indices && a.color==b.color && a.texture==b.texture,"Original material/indices changed");
            require(a.vertices.size()==b.vertices.size(),"Geometry size changed");
            for(std::size_t j=0;j<a.vertices.size();++j)
                require(a.vertices[j].position==b.vertices[j].position && a.vertices[j].normal==b.vertices[j].normal && a.vertices[j].uv==b.vertices[j].uv,"Original geometry changed");
            require(a.material.metallic==b.material.metallic && a.material.roughness==b.material.roughness && a.material.specular==b.material.specular && a.material.reflection==b.material.reflection,"Original lighting properties changed");
        }
        require(scene.original->meshes[0].indices==chrome.indices,"Chrome geometry differs from A");
        for(std::size_t i=0;i<chrome.vertices.size();++i)
            require(chrome.vertices[i].position==scene.original->meshes[0].vertices[i].position && chrome.vertices[i].normal==scene.original->meshes[0].vertices[i].normal && chrome.vertices[i].uv==scene.original->meshes[0].vertices[i].uv,"Chrome geometry/normals/UV changed");
        require(scene.original->nodes[0].local==scene.chrome->nodes[0].local && scene.original->nodes[0].parent==scene.chrome->nodes[0].parent,"Chrome placement/hierarchy changed");
        std::cout<<"CHROME SCENE CPU PASS: original material preserved, neutral chrome, same geometry, reflector excluded\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
