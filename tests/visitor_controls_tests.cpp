#include "app/InteractionDemoInput.hpp"
#include "app/NavigationHudLayout.hpp"
#include "world/ZoneNavigator.hpp"
#include "world/Plaza.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "modules/EcoCycle/EcoCycle.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using namespace ecosphere;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
struct Host:ModuleServices {
    ContentRegistry& registry;explicit Host(ContentRegistry& r):registry(r){}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override {return registry.open(owner);}
};
using Poses=std::map<std::string,glm::mat4>;
Poses poses(const ContentRegistry& r,const std::string& owner) {
    Poses out;for(const auto& p:r.placements())if(p.owner==owner)out[p.id]=p.world;return out;
}
bool near(const Poses& a,const Poses& b) {
    if(a.size()!=b.size())return false;
    for(const auto& [id,m]:a)for(int c=0;c<4;++c)for(int r=0;r<4;++r)
        if(!b.count(id) || std::abs(m[c][r]-b.at(id)[c][r])>1e-5f)return false;
    return true;
}
void checkHud(const std::string& message,int width,int height,const std::vector<std::string>& required) {
    const auto l=layoutNavigationHud(message,width,height);
    require(!l.lines.empty() && l.pixel>=1.5f,"HUD strokes too small/missing");
    require(l.x>=0 && l.y>=0 && l.x+l.width<=width && l.y+l.height<=height && l.height<=height/3.f,"HUD outside framebuffer/height budget");
    std::string text;for(const auto& row:l.lines){if(!text.empty())text+=' ';text+=row;}
    for(const auto& word:required)require(text.find(word)!=std::string::npos,"Required navigation/error/paused text hidden by layout");
    const auto model=makeNavigationHudModel(l);require(model->meshes.size()==2 && !model->meshes[1].vertices.empty(),"HUD glyph mesh missing");
    for(const auto& mesh:model->meshes)for(const auto& v:mesh.vertices)
        require(finiteVector(v.position) && v.position.x>=0 && v.position.x<=l.width && v.position.y>=0 && v.position.y<=l.height,"HUD mesh clips outside its panel");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected runtime assets");
        const std::array<std::string,3> owners{{"ecoenergy","aquabio","ecocycle"}};
        using Factory=std::unique_ptr<IModule>(*)();
        const std::array<Factory,3> factories{{makeEcoEnergy,makeAquaBio,makeEcoCycle}};
        const std::array<std::size_t,3> counts{{6,7,7}};
        ContentRegistry registry(argv[1]);Host host(registry);
        std::array<std::unique_ptr<ModuleSession>,3> sessions;
        for(std::size_t i=0;i<3;++i)sessions[i]=std::make_unique<ModuleSession>(factories[i](),host);
        const auto camera=ZoneSettings{}.entries[2].make();const auto projection=*camera.projection(1280,720);
        const auto press=[&](const std::string& owner,const std::vector<int>& keys,double dt=0,const std::function<float(int)>& held=[](int){return 0.f;}) {
            updateContentInput(registry,owner,camera,projection,keys,held,dt);
        };
        for(std::size_t i=0;i<3;++i) {
            const auto& owner=owners[i];const auto other=owners[(i+1)%3];registry.restart(owner);const auto initial=poses(registry,owner);
            auto& objects=registry.interactions(owner);std::set<std::string> selected;
            for(std::size_t n=0;n<counts[i];++n){press(owner,{GLFW_KEY_TAB});require(bool(objects.selected()),"Tab did not select");selected.insert(*objects.selected());}
            require(selected.size()==counts[i],"Not all twenty educational objects reachable through actual input adapter");
            press(owner,{},.1,[](int key){return key==GLFW_KEY_L || key==GLFW_KEY_X || key==GLFW_KEY_V ? 1.f : 0.f;});
            require(!near(initial,poses(registry,owner)),"Visitor translation/rotation/scale did not change selected object");
            press(owner,{GLFW_KEY_G});require(objects.holding(),"G did not take selected object");
            press(owner,{GLFW_KEY_G});require(!objects.holding(),"G did not drop selected object");
            const auto manipulatedAtZero=poses(registry,owner);
            registry.update(.5);press(owner,{GLFW_KEY_P});require(!registry.playing(owner),"P did not pause owner");
            const auto paused=poses(registry,owner),otherBefore=poses(registry,other);registry.update(.5);
            require(near(paused,poses(registry,owner)),"Visitor P key did not pause content animations");
            require(!near(otherBefore,poses(registry,other)),"Pausing one owner paused another");
            const auto otherAtRestart=poses(registry,other);press(owner,{GLFW_KEY_R});
            require(registry.playing(owner) && near(manipulatedAtZero,poses(registry,owner)),"R did not reset animation/resume/preserve manipulation");
            require(near(otherAtRestart,poses(registry,other)) && registry.interactions(owner).selected().has_value(),"Restart changed other owner or selection");
            press(owner,{GLFW_KEY_P});press(owner,{GLFW_KEY_P});registry.update(.5);
            require(registry.playing(owner) && !near(manipulatedAtZero,poses(registry,owner)),"P resume failed");
            press(owner,{GLFW_KEY_P});sessions[i].reset();rejects([&]{registry.playing(owner);});
            sessions[i]=std::make_unique<ModuleSession>(factories[i](),host);
            require(registry.playing(owner) && near(initial,poses(registry,owner)) && !registry.interactions(owner).selected(),"Reentry retained paused/selected/manipulated state");
        }
        auto sdk=registry.open("sdk-proof");EntityDescriptor e;e.id="proof";e.model="shared/models/SHARED-001.glb";
        bool fail=false;e.animation=AnimationDescriptor{ProceduralAnimation{[&](double t){if(fail)throw std::runtime_error("restart fault");Pose p;p.position.y=float(t);return p;}}};sdk->add(e);
        sdk->setPlaying(false);require(!registry.playing("sdk-proof"),"SDK pause and host state differ");
        const auto checkpoint=poses(registry,"sdk-proof");fail=true;rejects([&]{registry.restart("sdk-proof");});
        require(!registry.playing("sdk-proof") && near(checkpoint,poses(registry,"sdk-proof")),"Failed restart partially committed playback/poses");
        fail=false;sdk->restart();require(registry.playing("sdk-proof"),"SDK restart and host state differ");
        rejects([&]{registry.setPlaying("missing",false);});rejects([&]{registry.restart("missing");});
        ZoneNavigator nav(argv[1]);ZoneSettings settings;
        for(const auto& size:std::array<std::pair<int,int>,5>{{{320,240},{640,360},{800,600},{1280,720},{1920,1080}}}) {
            checkHud(nav.message({0,1.7f,19}),size.first,size.second,{"PLAZA CENTRAL","ACERCATE"});
            for(const auto& portal:settings.portals) {
                checkHud(nav.message(portal.center),size.first,size.second,{std::string(zoneTitle(portal.destination)),"ENTER PARA ENTRAR"});
                nav.input({},false,false);require(nav.input(portal.center,true,false),"HUD test enter failed");
                require(nav.message({}).find("MUESTRA TECNICA")==std::string::npos,"Provisional label in definitive experience");
                checkHud(nav.message({}),size.first,size.second,{std::string(zoneTitle(nav.active())),"H VOLVER A LA PLAZA"});
                nav.registry().setPlaying(std::string(zoneId(nav.active())),false);
                checkHud(nav.message({}),size.first,size.second,{"H VOLVER A LA PLAZA","EN PAUSA","P CONTINUAR"});
                nav.input({},false,false);require(nav.input({},false,true),"HUD test return failed");nav.input({},false,false);
            }
        }
        nav.input({},false,false);require(!nav.input(settings.portals[0].center,true,false,[](const auto&,const auto&,const auto&){throw std::runtime_error("prepare fault");}),"HUD error fixture accepted failed load");
        checkHud(nav.message({}),320,240,{"PLAZA CENTRAL","ERROR DE CARGA","ZONA CONSERVADA"});
        require(layoutNavigationHud("text",0,0).lines.empty() && layoutNavigationHud("text",80,50).lines.empty(),"Minimized/tiny framebuffer not handled");
        for(const auto& digit:{"1","2"})require(!makeNavigationHudModel(layoutNavigationHud(digit,640,360))->meshes[1].vertices.empty(),"Camera hotkey digit missing");
        std::cout<<"VISITOR CPU PASS: 20 selections/TRS/take-drop; P/R owner isolation, atomic failure, SDK semantics and reentry; four-zone HUD titles/Enter/H/pause/errors at five framebuffer sizes; CPU model bounds, no GL context\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
