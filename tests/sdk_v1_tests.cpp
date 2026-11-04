#include "sdk_consumer/LessonModule.hpp"
#include "content/ContentRegistry.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
template<class F> void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
struct Host final:ecosphere::ModuleServices {
    ecosphere::ContentRegistry registry;
    explicit Host(const std::filesystem::path& root):registry(root){}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override {return registry.open(owner);}
};
bool close(const glm::mat4& a,const glm::mat4& b) {
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(std::abs(a[c][r]-b[c][r])>.0002f)return false;
    return true;
}
}
int main(int argc,char** argv) {
    try {
        using namespace ecosphere;
        require(argc==2,"Expected assets root");Host host(argv[1]);
        std::vector<ContentPlacement> snapshot;std::weak_ptr<const Model> shared;
        {
            ModuleSession a(lesson::makeModule(),host);
            lesson::Settings settings;settings.owner="other";
            ModuleSession b(lesson::makeModule(settings),host);
            snapshot=host.registry.placements();require(snapshot.size()==6,"Consumer entities/owner isolation");
            shared=snapshot.front().resource;
            for(const auto& p:snapshot) {
                require(p.resource==snapshot.front().resource,"Shared GLB resource");
                require(!p.model->meshes.front().material.unlit,"Public material configuration");
                require(glm::length(p.model->meshes.front().color-p.resource->meshes.front().color*glm::vec4(.6f,.85f,1,1))<.0001f,"Public tint configuration");
            }
            require(host.registry.interactions("lesson").objects().size()==3,"Three interactive animation families");
            require(host.registry.lighting("lesson")->point.color==glm::vec3(1,.8f,.45f),"Consumer lighting");
            a.update(.5);host.registry.update(.5);
            const auto current=host.registry.placements();
            bool edited=false;
            for(const auto& p:current)if(p.owner=="lesson" && p.id=="simple")edited=std::abs(p.world[3].y-1.05f)<.0001f;
            require(edited,"Public setTransform during module update");
            for(const auto& p:current)if(p.owner=="other" && p.id=="simple")require(p.world[3].y==1,"Editing must not affect another owner");
            require(current.front().resource->nodes.size()==snapshot.front().resource->nodes.size(),"glTF hierarchy preserved");
        }
        require(host.registry.placements().empty() && !shared.expired(),"Session closes; snapshot remains valid");
        snapshot.clear();require(shared.expired(),"Last snapshot releases resource");
        Host a(argv[1]),b(argv[1]);ModuleSession low(lesson::makeModule(),a),high(lesson::makeModule(),b);
        for(int i=0;i<60;++i){low.update(1./30);a.registry.update(1./30);}
        for(int i=0;i<288;++i){high.update(1./144);b.registry.update(1./144);}
        const auto p=a.registry.placements(),q=b.registry.placements();
        for(std::size_t i=0;i<p.size();++i)require(close(p[i].world,q[i].world),"Consumer animation must be FPS independent");
        auto content=host.registry.open("editing");EntityDescriptor entity;
        entity.id="object";entity.model="models/blender_probe_unlit.glb";entity.interaction=InteractionDescriptor{};
        entity.animation=AnimationDescriptor{SimpleAnimation{}};content->add(entity);
        content->manipulate(entity.id,{1,0,0},{1,0,0,0},1.2f);
        content->setPlaying(false);const auto before=host.registry.placements().front().world;
        host.registry.update(1);require(close(before,host.registry.placements().front().world),"Pause");
        content->restart();require(host.registry.interactions("editing").object("object").manipulation.position.x==1,"Restart preserves edits");
        rejects([&]{content->add(entity);});rejects([&]{content->setTransform("other-owner",{});});
        auto invalid=entity.transform;invalid.scale.x=0;rejects([&]{content->setTransform(entity.id,invalid);});
        require(close(host.registry.placements().front().world,before),"Invalid edit must be atomic");
        entity.id="missing";entity.model="models/missing.glb";rejects([&]{content->add(entity);});
        require(host.registry.placements().size()==1,"Failed resource load must not publish entity");
        std::cout<<"SDK V1 CPU PASS: public consumer on real registry; GLB/TRS/material/three animations/interaction/lights/editing; sharing/lifetime; 30/144 FPS; errors and atomic edits\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
