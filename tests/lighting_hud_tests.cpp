#include "app/LightingControls.hpp"
#include "app/FpsCounter.hpp"
#include "app/NavigationHudLayout.hpp"
#include "world/ZoneNavigator.hpp"
#include "world/LightingDemoSettings.hpp"
#include <iostream>
#include <stdexcept>
using namespace ecosphere;
void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
bool same(const LightingSettings& a,const LightingSettings& b) {
    const auto light=[](const LightSettings& x,const LightSettings& y) {
        return x.enabled==y.enabled && x.vector==y.vector && x.color==y.color && x.intensity==y.intensity;
    };
    return a.legacyUnlit==b.legacyUnlit && a.directEnabled==b.directEnabled && a.ambient==b.ambient &&
        light(a.directional,b.directional) && light(a.point,b.point) && a.attenuation==b.attenuation &&
        a.environment.reflectionEnabled==b.environment.reflectionEnabled && a.environment.skyboxEnabled==b.environment.skyboxEnabled &&
        a.environment.intensity==b.environment.intensity && a.environment.fresnelPower==b.environment.fresnelPower;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected staged assets");
        ZoneNavigator nav(argv[1]);ZoneSettings zones;LightingControls controls;FpsCounter fps;
        require(controls.visible,"HUD must start visible");
        for(int zone=0;zone<4;++zone) {
            if(zone) {nav.cancelInput();nav.input({},false,false);require(nav.input(zones.portals[zone-1].center,true,false),"Enter zone");}
            const auto original=nav.lighting();auto current=original;controls.defaults(original);
            require(controls.visible==(zone==0),"Zone change lost hidden preference");
            const auto key=[&](int key,int action) {fps.key(key,action);return controls.key(key,action,current);};
            for(int code=GLFW_KEY_1;code<=GLFW_KEY_5;++code) {
                require(!key(code,GLFW_REPEAT) && same(current,original),"Repeat changed lights");
                require(!key(code,GLFW_RELEASE) && same(current,original),"Release changed lights");
                require(key(code,GLFW_PRESS) && !same(current,original),"Toggle did not change actual settings");
                key(code,GLFW_PRESS);require(same(current,original),"Toggle did not restore original flag");
            }
            key(GLFW_KEY_F4,GLFW_REPEAT);require(controls.visible==(zone==0),"F4 repeated");
            key(GLFW_KEY_F4,GLFW_RELEASE);require(controls.visible==(zone==0),"F4 release toggled");
            if(controls.visible)key(GLFW_KEY_F4,GLFW_PRESS);
            key(GLFW_KEY_1,GLFW_PRESS);
            require(!controls.visible && !current.directEnabled,"Hidden panel blocked input");
            const auto text=lightingHudText(current,false,320);
            require(text.find("1:OFF")!=std::string::npos && text.find("2:ON-")!=std::string::npos && text.find("3:ON-")!=std::string::npos && text.find("SIN DIRECTA")!=std::string::npos,"Masked contribution not distinguished");
            key(GLFW_KEY_F4,GLFW_PRESS);require(controls.visible && !current.directEnabled,"F4 changed lighting");
            key(GLFW_KEY_F3,GLFW_PRESS);require(!fps.visible && controls.visible,"F3 changed lighting HUD");
            key(GLFW_KEY_F4,GLFW_PRESS);require(!controls.visible && !fps.visible,"F4 changed FPS");
            key(GLFW_KEY_F3,GLFW_PRESS);require(fps.visible,"FPS did not return");
            require(!key(GLFW_KEY_6,GLFW_PRESS) && !current.directEnabled,"Lighting controls captured regression key");
            require(lightingHudText(current,true,1280).find("6 REGRESION: ON")!=std::string::npos,"Regression not labelled separately");
            current.ambient=.91f;current.directional.vector={1,2,3};current.point.color={.1f,.2f,.3f};
            current.point.intensity=7;current.environment.intensity=.13f;current.environment.fresnelPower=9;current.attenuation={1,2,3};
            const auto modified=current;
            key(GLFW_KEY_0,GLFW_REPEAT);require(same(current,modified),"Reset repeated");
            nav.update(.1);nav.update(.1);const auto before=nav.placements();
            key(GLFW_KEY_0,GLFW_PRESS);require(same(current,original),"Reset must restore all active-zone fields");
            require(same(nav.lighting(),original),"Input mutated module defaults");
            const auto after=nav.placements();require(before.size()==after.size(),"Reset changed resources");
            for(std::size_t i=0;i<before.size();++i)require(before[i].world==after[i].world && before[i].resource==after[i].resource,"Reset changed animations/resources");
            for(int mask=0;mask<32;++mask)for(const auto& size:{std::pair<int,int>{1280,720},{640,360},{320,240}}) {
                current.directEnabled=mask&1;current.directional.enabled=mask&2;current.point.enabled=mask&4;
                current.environment.reflectionEnabled=mask&8;current.environment.skyboxEnabled=mask&16;
                auto layout=layoutNavigationHud(lightingHudText(current,false,size.first),size.first,size.second);
                require(!layout.truncated && !layout.lines.empty(),"Lighting text truncated");
                const auto y=10.f,x=size.first-10-layout.width;
                require(x>=0 && x+layout.width<=size.first && y+layout.height<size.second/2.f,"Panel outside its bottom region");
                auto navLayout=layoutNavigationHud(nav.message(nav.entryCamera().position),size.first,size.second);
                require(y+layout.height<=navLayout.y-40,"Lighting panel overlaps educational HUD");
                const auto mesh=makeNavigationHudModel(layout);require(!mesh->meshes[1].vertices.empty(),"Missing text geometry");
            }
            if(zone) {nav.cancelInput();nav.input({},false,false);require(nav.input({},false,true),"Return to plaza");}
        }
        // Historical modes retain the initial renderer settings rather than a universal zone default.
        auto legacy=demo::lighting;controls.defaults(legacy);controls.key(GLFW_KEY_5,GLFW_PRESS,legacy);
        controls.key(GLFW_KEY_0,GLFW_PRESS,legacy);require(same(legacy,demo::lighting),"Legacy reset");
        std::cout<<"LIGHTING HUD CPU PASS: four defaults; PRESS/repeat/release; hidden changes; F3/F4 isolation; 32 states x 3 sizes; reset preserves resources/animation; legacy reset\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
