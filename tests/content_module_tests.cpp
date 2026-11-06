#include "modules/EcoCycle/EcoCycle.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/modules/Stubs.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "content/ContentRegistry.hpp"
#include "app/InteractionDemoInput.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <algorithm>
#include <limits>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
struct Host final : ecosphere::ModuleServices {
    ecosphere::ContentRegistry& registry;
    explicit Host(ecosphere::ContentRegistry& value) : registry(value) {}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override { return registry.open(owner); }
};
}
int main(int argc,char** argv) {
    try {
        using namespace ecosphere;
        require(argc==2,"Expected asset root"); ContentRegistry registry(argv[1]); Host host(registry);
        const auto ecoExpected = makeEcoEnergyInstances().size()+3;
        const auto aquaExpected = makeAquaBioInstances().size()+3;
        const auto cycleExpected = makeEcoCycleInstances().size()+4;
        using Factory=std::unique_ptr<IModule>(*)();
        const std::array<Factory,3> factories{makeEcoEnergy,makeAquaBio,makeEcoCycle};
        std::vector<std::unique_ptr<ModuleSession>> sessions;
        for(auto factory:factories) sessions.push_back(std::make_unique<ModuleSession>(factory(),host));
        auto before=registry.placements();
        require(before.size()==ecoExpected+aquaExpected+cycleExpected,"Three definitive modules not registered");
        require(before.front().owner=="aquabio","Unexpected owner order");
        const auto cycleIndex=aquaExpected;
        require(before[cycleIndex].owner=="ecocycle" && before[cycleIndex].id=="CYCLE-001-main","EcoCycle first model identity");
        registry.update(0.5); const auto after=registry.placements();
        std::size_t ecoMoved=0, aquaMoved=0,cycleMoved=0;
        for(std::size_t i=0;i<before.size();++i) {
            // Eight float epsilons allow recomposition rounding, not mechanism motion.
            bool moved=false;
            for(int c=0;c<4;++c)for(int r=0;r<4;++r)
                moved=moved || std::abs(before[i].world[c][r]-after[i].world[c][r])>8*std::numeric_limits<float>::epsilon()*
                    std::max({1.f,std::abs(before[i].world[c][r]),std::abs(after[i].world[c][r])});
            if(moved) {
                if(before[i].owner=="ecoenergy")++ecoMoved;
                else if(before[i].owner=="aquabio")++aquaMoved;else ++cycleMoved;
            }
            if(before[i].owner=="ecocycle")require(after[i].model==after[i].resource,"Unnecessary EcoCycle material variant");
            require(registry.lighting(after[i].owner).has_value(),"Lighting not registered");
            std::size_t draws=0;
            after[i].model->forEach([&](std::size_t,const glm::mat4& world){ (void)normalMatrix(world); ++draws; },after[i].world);
            require(draws>=1,"Bridge lost glTF hierarchy/primitives");
        }
        require(cycleMoved==8 && registry.interactions("ecocycle").objects().size()==7,"EcoCycle definitive mechanisms/interactions");
        require(aquaMoved==18,"AquaBio must animate 6 mechanisms and 12 pond plants");
        require(registry.interactions("aquabio").objects().size()==7,"AquaBio must expose 7 interactive objects");
        require(ecoMoved==8,"EcoEnergy must animate 8 definitive mechanisms");
        require(registry.interactions("ecoenergy").objects().size()==6,"EcoEnergy must expose 6 interactive objects");
        Camera camera;
        std::vector<std::pair<bool,bool>> observed;
        auto& interaction=registry.interactions("ecoenergy");
        updateObjectInput(interaction,camera,*camera.projection(800,600),
            {GLFW_KEY_BACKSPACE,GLFW_KEY_TAB,GLFW_KEY_G},[](int){return 0.0f;},0,
            [&](int){observed.push_back({interaction.selected().has_value(),interaction.holding()});});
        require(observed==std::vector<std::pair<bool,bool>>{{false,false},{true,false},{true,true}},
            "Input actions/callbacks lost event order");
        sessions[0].reset(); require(registry.placements().size()==aquaExpected+cycleExpected,"Independent EcoEnergy close failed");
        sessions.clear(); require(registry.placements().empty(),"Module cleanup failed");
        require(!before[cycleIndex].resource->nodes.empty(),"Snapshot invalid after close");
        {
            auto proof=registry.open("override-proof");EntityDescriptor e;e.id="sample";e.model="models/blender_probe_unlit.glb";
            e.visual=VisualProperties{{.6f,1,.4f,1},MaterialSettings{false}};e.interaction=InteractionDescriptor{};proof->add(e);
            const auto p=registry.placements().front();
            require(!p.model->meshes.at(0).material.unlit,"Public material not applied");
            require(p.resource->meshes.at(0).material.unlit,"Visual variant mutated shared source");
            require(registry.interactions("override-proof").select(e.id),"Public interaction not applied");
        }
        require(registry.placements().empty(),"Override proof cleanup");
        std::cout<<"Three-module integration PASS: three definitive installations, shared GLBs, three animation families, materials, interactions, lighting and independent close; CPU bridge, no GL context\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
