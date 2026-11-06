#include "modules/EcoCycle/EcoCycle.hpp"
#include "world/ZoneNavigator.hpp"
#include "world/Plaza.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include <ecosphere/modules/Stubs.hpp>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
struct Broken final : ecosphere::IModule {
    ecosphere::Zone zone;
    explicit Broken(ecosphere::Zone z):zone(z){}
    std::string_view id() const noexcept override { return ecosphere::zoneId(zone); }
    void initialize(ecosphere::ModuleServices& s) override {
        ecosphere::EntityDescriptor d;d.id="partial";d.model="models/blender_probe_unlit.glb";
        s.content()->add(d);throw std::runtime_error("deliberate partial initialization");
    }
    void update(double) override {}
    void shutdown() noexcept override {}
};
}
int main(int argc,char** argv) {
    try {
        using namespace ecosphere;
        require(argc==2,"Expected assets root");
        const auto ecoExpected=makeEcoEnergyInstances().size()+3;
        const auto aquaExpected=makeAquaBioInstances().size()+3;
        ZoneSettings settings;
        std::set<std::string_view> ids;
        for(auto z:{Zone::hub,Zone::ecoenergy,Zone::aquabio,Zone::ecocycle}) ids.insert(zoneId(z));
        require(ids.size()==4,"Zone identifiers");rejects([]{zoneId(static_cast<Zone>(9));});
        auto bad=settings;bad.portals[0].halfExtent.x=0;rejects([&]{ZoneNavigator n(argv[1],bad);});
        bad=settings;bad.entries[1].height=-1;rejects([&]{ZoneNavigator n(argv[1],bad);});
        ZoneNavigator nav(argv[1]);
        require(nav.active()==Zone::hub && nav.placements().size()==92,"Initial hub");
        require(!nav.input({0,1.7f,0},true,false),"Out-of-volume Enter must fail");
        for(const auto& p:settings.portals) {
            nav.input({},true,false);
            require(nav.nearby(p.center)==p.destination,"Correct portal detection");
            require(p.contains(p.center+p.halfExtent),"Inclusive portal boundary");
            require(!p.contains(p.center+glm::vec3(0,5,0)),"Free camera above portal");
            require(!p.contains({NAN,0,0}),"NaN proximity");
            require(!nav.input(p.center,true,false),"Holding Enter from outside must not enter");
            nav.input(p.center,false,false);
        }
        // La geometría real y el volumen deben compartir el centro horizontal del acceso.
        for(const auto& e:makePlazaInstances()) if(e.id.find("PLAZA-008-")==0) {
            bool match=false;
            for(const auto& p:settings.portals) match|=glm::length(glm::vec2(e.transform.position.x-p.center.x,e.transform.position.z-p.center.z))<.001f;
            require(match,"Portal/arch placement mismatch");
        }
        for(int cycle=0;cycle<34;++cycle) for(const auto& p:settings.portals) {
            const auto expectedCount=p.destination==Zone::ecoenergy ? ecoExpected : p.destination==Zone::aquabio ? aquaExpected : makeEcoCycleInstances().size()+4;
            nav.input(p.center,false,false);
            auto old=nav.placements();std::weak_ptr<const Model> released=old.front().resource;old.clear();
            require(nav.input(p.center,true,false,[&](const auto& candidate,const auto&,const auto&) {
                require(nav.state()==NavigationState::preparing && nav.active()==Zone::hub,"Prepare before commit");
                require(candidate.size()==expectedCount && candidate.front().owner==zoneId(p.destination),"Only candidate placements");
            }),"Enter destination");
            require(released.expired(),"Outgoing hub reference leaked");
            require(nav.active()==p.destination && nav.registry().placements().size()==expectedCount,"Only one active session");
            require(nav.entryCamera().mode()==CameraMode::firstPerson && nav.entryCamera().position.y==1.7f,"Entry camera");
            require(nav.message({}).find("H VOLVER")!=std::string::npos,"In-zone HUD");
            const auto initial=nav.placements();
            nav.update(500); // Primer paso tras carga descartado; siguientes limitados a 0.1.
            const auto paused=nav.placements();
            for(std::size_t idx=0;idx<initial.size();++idx)
                for(int x=0;x<4;++x) for(int y=0;y<4;++y)
                    require(std::abs(initial[idx].world[x][y]-paused[idx].world[x][y])<.0001f,"First transition delta must be zero");
            nav.update(500);
            const auto advancedAll=nav.placements();
            if(p.destination!=Zone::hub) {
                std::size_t moved=0;
                for(std::size_t idx=0;idx<initial.size();++idx) moved+=initial[idx].world!=advancedAll[idx].world;
                require(moved==(p.destination==Zone::aquabio ? 18u : 8u),"Installation animation count must match its mechanisms and plants");
            }
            const auto advanced=advancedAll.front().world;
            if(p.destination==Zone::aquabio) for(const auto& e:advancedAll) if(e.id=="AQUA-005-adjustable") {
                const auto& a=aquabio::settings;
                const float angle=-a.gutterSwingDegrees+a.gutterSwingDegrees*static_cast<float>(.1/(a.gutterDurationSeconds*.25));
                require(std::abs(e.world[0][1]-std::sin(glm::radians(angle)))<.0001f,"AquaBio keyframe delta limited to 0.1");
            }
            if(p.destination==Zone::ecocycle)for(const auto& e:advancedAll)if(e.id=="CYCLE-006-main") {
                const auto& c=ecocycle::settings;
                require(std::abs(e.world[3].y-c.positions[5].y-c.feederAmplitudeMeters*std::sin(glm::two_pi<double>()*c.feederFrequencyHz*.1))<.0001f,"Procedural delta limited to 0.1");
            }
            require(!nav.input(p.center,true,true),"Transition must wait for release of both keys");
            nav.input(p.center,false,false);
            auto sample=nav.placements();auto resource=sample.front().resource;
            auto extra=nav.registry().open("shared-test");EntityDescriptor d;d.id="sample";
            d.model=p.destination==Zone::ecoenergy ? "ecoenergy/models/ENERGY-001.glb" : p.destination==Zone::aquabio ? "aquabio/models/AQUA-001.glb" : "ecocycle/models/CYCLE-001.glb";
            extra->add(d);
            bool shares=false;
            for(const auto& entity:nav.registry().placements()) if(entity.owner=="shared-test") shares=entity.resource==resource;
            require(shares,"Shared canonical GLB must survive another owner");
            require(nav.input({},false,true),"H return");
            require(nav.active()==Zone::hub && nav.placements().size()==92,"Restored hub");
            require(nav.lighting().directional.intensity==plaza::settings.lighting.directional.intensity,"Hub illumination restored");
            const auto expected=p.returnCamera.make();
            require(glm::length(nav.entryCamera().position-expected.position)<.0001f && nav.entryCamera().yawDegrees==expected.yawDegrees,"Safe return camera");
            require(!nav.nearby(nav.entryCamera().position),"Return outside activation volume");
            require(!nav.input(p.center,true,true),"Held keys after return");
            sample.clear();resource.reset();extra.reset();
            require(nav.registry().placements().size()==92,"Shared owner cleanup");
            nav.input({},false,false);require(!nav.input({},false,true),"H in hub is no-op");
        }
        nav.input({},false,false);nav.cancelInput();
        require(!nav.input(settings.portals[0].center,true,false),"Focus change blocks held keys");
        nav.input({},false,false);
        require(!nav.input(settings.portals[0].center,true,false,[](const auto&,const auto&,const auto&) {throw std::runtime_error("GPU upload failure");}),"GPU failure must roll back");
        require(nav.active()==Zone::hub && nav.placements().size()==92 && !nav.error().empty(),"GPU rollback state");
        require(nav.registry().placements().size()==92,"GPU failure candidate cleanup");
        nav.input({},false,false);
        require(nav.input(settings.portals[0].center,true,false),"Retry after failure");
        require(nav.error().empty(),"Successful retry clears error");
        bool fail=false;
        ZoneNavigator failures(argv[1],settings,[&](Zone z)->std::unique_ptr<IModule> {
            if(fail)return std::make_unique<Broken>(z);
            switch(z){case Zone::hub:return makePlaza();case Zone::ecoenergy:return makeEcoEnergy();case Zone::aquabio:return makeAquaBio();case Zone::ecocycle:return makeEcoCycle();}
            throw std::runtime_error("bad zone");
        });
        fail=true;
        require(!failures.input(settings.portals[1].center,true,false),"Partial initialization must fail");
        require(failures.active()==Zone::hub && failures.registry().placements().size()==92,"Partial rollback");
        fail=false;failures.input({},false,false);require(failures.input(settings.portals[1].center,true,false),"Retry init");
        fail=true;failures.input({},false,false);require(!failures.input({},false,true),"Hub initialization failure");
        require(failures.active()==Zone::aquabio && failures.registry().placements().size()==aquaExpected,"Failed hub keeps previous installation valid");
        require(failures.lighting().ambient==aquabio::settings.lighting.ambient,"Previous lights preserved after failure");
        rejects([&]{nav.update(-1);});
        auto c=settings.entries[0];c.height=2.2f;auto camera=c.make();camera.update({},.1);require(camera.position.y==2.2f,"Configured observer height");
        std::cout<<"ZONE CPU PASS: four zones, proximity/edges, 102 round trips, cameras/lights, shared GLB, session release, CPU/GPU preparation rollback and retry\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
