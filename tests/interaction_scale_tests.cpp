#include "app/InteractionDemoInput.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/EcoCycle/EcoCycle.hpp"
#include "assets/AssetManager.hpp"
#include "world/ZoneNavigator.hpp"
#include "world/LightingDemo.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <map>
using namespace ecosphere;
namespace {
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
struct Host:ModuleServices {
    ContentRegistry& registry;
    explicit Host(ContentRegistry& r):registry(r) {}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override { return registry.open(owner); }
};
void validateRender(const Model& model,const InteractionSystem& system,const std::string& id) {
    const auto world=system.world(id);
    model.forEach([&](std::size_t,const glm::mat4& matrix) { (void)normalMatrix(matrix); },world);
    const auto& box=system.object(id).bounds;
    const auto center=(box.minimum+box.maximum)*.5f,size=box.maximum-box.minimum;
    // Exercise the real thin selection bars, which fail before the body does.
    for(int axis=0;axis<3;++axis) for(int a:{-1,1}) for(int b:{-1,1}) {
        Pose bar;bar.position=center;bar.scale=glm::vec3(.025f);
        bar.scale[axis]=std::max(size[axis],.025f);
        bar.position[(axis+1)%3]+=a*size[(axis+1)%3]*.5f;
        bar.position[(axis+2)%3]+=b*size[(axis+2)%3]*.5f;
        (void)normalMatrix(world*bar.matrix());
    }
}
float exercise(ContentRegistry& registry,const std::string& owner,const std::string& id,
    const Model& model,int hz) {
    auto& system=registry.interactions(owner);
    system.object(id).manipulation=Pose{};system.select(id);
    const auto camera=ZoneSettings{}.entries[2].make();const auto projection=*camera.projection(1280,720);
    const auto original=system.object(id).manipulation;const auto center=glm::vec3(system.world(id)[3]);
    const auto input=[&](const std::vector<int>& pressed,const std::function<float(int)>& held,double dt) {
        updateContentInput(registry,owner,camera,projection,pressed,held,dt);
    };
    float atTen=0;
    for(int frame=1;frame<=20*hz;++frame) {
        input({},[](int k){return k==GLFW_KEY_C?1.f:0.f;},1./hz);
        validateRender(model,system,id);
        if(frame==5*hz || frame==10*hz || frame==20*hz) {
            require(system.object(id).manipulation.scale.x>0,"Non-positive scale");
            std::cout<<owner<<'/'<<id<<" hz="<<hz<<" seconds="<<frame/hz
                     <<" scale="<<system.object(id).manipulation.scale.x<<'\n';
        }
        if(frame==10*hz) atTen=system.object(id).manipulation.scale.x;
    }
    const auto minimum=system.object(id).manipulation.scale.x;
    require(minimum==atTen,"Holding C at minimum must not accumulate error");
    require(system.object(id).manipulation.position==original.position &&
        system.object(id).manipulation.orientation==original.orientation,"Shrink changed position/orientation");
    require(glm::length(glm::vec3(system.world(id)[3])-center)<1e-5f,"Shrink moved selected center");
    for(int frame=0;frame<hz;++frame) input({},[](int k){return k==GLFW_KEY_V?1.f:0.f;},1./hz);
    require(system.object(id).manipulation.scale.x>minimum*1.9f,"V cannot grow after reaching floor");
    input({},[](int k){return k==GLFW_KEY_L || k==GLFW_KEY_X?1.f:0.f;},.1);
    require(system.object(id).manipulation.position.x>original.position.x &&
        system.object(id).manipulation.orientation!=original.orientation,"TR after minimum stopped working");
    validateRender(model,system,id);
    const auto saved=system.object(id).manipulation;
    input({GLFW_KEY_BACKSPACE},[](int k){return k==GLFW_KEY_C?1.f:0.f;},.1);
    require(!system.selected() && system.object(id).manipulation.scale==saved.scale,"Deselect did not stop scaling");
    system.select(id);bool rejected=false;
    try { system.manipulate({},glm::quat(1,0,0,0),0); } catch(const std::invalid_argument&) {rejected=true;}
    require(rejected && system.object(id).manipulation.scale==saved.scale,"Strict API rejection changed");
    return minimum;
}
void hierarchy() {
    Model cube;cube.meshes.push_back(makeUnitCube());ModelNode node;node.meshes={0};cube.nodes.push_back(node);
    float reference=0;
    for(int hz:{30,144}) {
        InteractionSystem system;InteractiveObject parent;parent.id="parent";
        parent.base={{2,3,4},glm::angleAxis(.4f,glm::normalize(glm::vec3(1,2,3))),{.6f,.4f,.8f}};
        parent.bounds={{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};system.add(parent);
        InteractiveObject child;child.id="child";child.parent="parent";child.bounds=parent.bounds;
        child.base={{1,0,0},glm::angleAxis(.3f,glm::vec3(0,1,0)),{.15f,.17f,.19f}};system.add(child);
        Camera camera;const auto projection=*camera.projection(1280,720);
        for(const char* id:{"parent","child"}) {
            system.select(id);const auto center=glm::vec3(system.world(id)[3]);
            for(int frame=0;frame<20*hz;++frame) {
                updateObjectInput(system,camera,projection,{},[](int k){return k==GLFW_KEY_C?1.f:0.f;},1./hz);
                validateRender(cube,system,"parent");validateRender(cube,system,"child");
            }
            require(system.object(id).manipulation.scale.x<.99f,"Hierarchy case did not exercise shrinking");
            require(glm::length(glm::vec3(system.world(id)[3])-center)<1e-5f,"Scaled hierarchy changed selected center");
            if(std::string(id)=="parent") for(int frame=0;frame<hz/2;++frame) {
                updateObjectInput(system,camera,projection,{},[](int k){return k==GLFW_KEY_V?1.f:0.f;},1./hz);
                validateRender(cube,system,"parent");validateRender(cube,system,"child");
            }
        }
        const auto scale=system.object("parent").manipulation.scale.x;
        if(reference) require(std::abs(scale-reference)<reference*1e-4f,"Hierarchical floor depends on FPS");
        reference=scale;
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected runtime assets");
        using Factory=std::unique_ptr<IModule>(*)();
        for(const auto factory:{Factory(makeAquaBio),Factory(makeEcoEnergy),Factory(makeEcoCycle)}) {
            ContentRegistry registry(argv[1]);Host host(registry);auto module=factory();const std::string owner(module->id());
            ModuleSession session(std::move(module),host);registry.setPlaying(owner,false);
            std::map<std::string,std::shared_ptr<const Model>> models;
            for(const auto& p:registry.placements()) if(p.owner==owner) models[p.id]=p.model;
            std::vector<std::string> ids;
            for(const auto& object:registry.interactions(owner).objects()) ids.push_back(object.id);
            if(owner=="aquabio") {
                const auto camera=ZoneSettings{}.entries[2].make();
                updateContentInput(registry,owner,camera,*camera.projection(1280,720),{GLFW_KEY_TAB},[](int){return 0.f;},0);
                require(registry.interactions(owner).selected()==std::optional<std::string>("AQUA-009-demo"),"Original selection sequence changed");
            }
            for(const auto& id:ids) {
                const float baseline=exercise(registry,owner,id,*models.at(id),60);
                if(id=="AQUA-009-demo") for(int hz:{30,144}) {
                    const float scale=exercise(registry,owner,id,*models.at(id),hz);
                    require(std::abs(scale-baseline)<baseline*1e-4f,"AquaBio floor depends on FPS");
                }
            }
        }
        {
            ContentRegistry registry(argv[1]);auto sdk=registry.open("probe");EntityDescriptor e;
            e.id="nested";e.model="models/blender_probe_unlit.glb";e.transform.scale={.1f,.4f,.3f};
            e.interaction=InteractionDescriptor{Aabb{{-1000,-1000,-1000},{1000,1000,1000}}};sdk->add(e);
            const auto model=registry.placements().front().model;
            exercise(registry,"probe",e.id,*model,60);
        }
        hierarchy();
        {
            AssetManager assets(argv[1]);auto imported=assets.loadModel("models/blender_probe_unlit.glb");
            InteractionAnimationDemo demo(*imported);demo.togglePause();Camera camera;const auto projection=*camera.projection(1280,720);
            for(const auto& object:demo.interaction.objects()) {
                demo.interaction.select(object.id);
                for(int frame=0;frame<20*60;++frame) {
                    updateInteractionInput(demo,camera,projection,{},[](int k){return k==GLFW_KEY_C?1.f:0.f;},1./60);
                    demo.cubes->forEach([&](std::size_t,const glm::mat4& m){(void)normalMatrix(m);});
                    demo.outline->forEach([&](std::size_t,const glm::mat4& m){(void)normalMatrix(m);},demo.interaction.world(object.id));
                    if(object.id=="imported") validateRender(*imported,demo.interaction,object.id);
                }
            }
        }
        {
            ZoneNavigator nav(argv[1]);ZoneSettings settings;const auto portal=settings.portals[1];
            nav.input({},false,false);require(nav.input(portal.center,true,false),"AquaBio entry failed");
            auto& system=nav.registry().interactions("aquabio");system.select("AQUA-009-demo");
            const auto camera=nav.entryCamera();
            for(int frame=0;frame<20*60;++frame)
                updateContentInput(nav.registry(),"aquabio",camera,*camera.projection(1280,720),{},[](int k){return k==GLFW_KEY_C?1.f:0.f;},1./60);
            nav.input({},false,false);require(nav.input({},false,true),"Return to Plaza failed");
            nav.input({},false,false);require(nav.input(settings.portals[0].center,true,false),"EcoEnergy entry failed");
            nav.input({},false,false);require(nav.input({},false,true),"EcoEnergy return failed");
            nav.input({},false,false);require(nav.input(portal.center,true,false),"AquaBio reentry failed");
            const auto& fresh=nav.registry().interactions("aquabio");
            require(!fresh.selected() && fresh.object("AQUA-009-demo").manipulation.scale==glm::vec3(1),"Reentry retained manipulation");
        }
        std::cout<<"SCALE LIMIT PASS: 20 educational objects, 5/10/20 seconds, 30/60/144 Hz, V/TR/deselect/reentry, GLB nodes, hierarchy, original demo and strict API rejection\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
