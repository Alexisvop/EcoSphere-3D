#include "EcoCycle.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
namespace {
glm::quat rotate(float degrees,glm::vec3 axis={0,1,0}) {return glm::angleAxis(glm::radians(degrees),axis);}
void validate(const ecocycle::Settings& s) {
    validatePose({s.cameraPosition,rotate(s.cameraYaw),glm::vec3(1)});
    if(!finiteVector(s.clearColor)||glm::any(glm::lessThan(s.clearColor,glm::vec3(0)))||glm::any(glm::greaterThan(s.clearColor,glm::vec3(1))))throw std::invalid_argument("EcoCycle clear color");
    for(float speed:{s.rollerSpeedDeg,s.magnetSpeedDeg,s.rotorSpeedDeg,s.compostSpeedDeg})
        if(!std::isfinite(speed)||std::abs(speed)>1440)throw std::invalid_argument("EcoCycle rotation speed");
    for(double duration:{s.gateDurationSeconds,s.pressDurationSeconds})
        if(!std::isfinite(duration)||duration<=.1)throw std::invalid_argument("EcoCycle duration");
    for(float angle:{s.gateOpeningDeg,s.sensorAmplitudeDegrees})
        if(!std::isfinite(angle)||angle<0||angle>80)throw std::invalid_argument("EcoCycle angle");
    for(float travel:{s.pressTravelMeters,s.feederAmplitudeMeters})
        if(!std::isfinite(travel)||travel<0||travel>1)throw std::invalid_argument("EcoCycle travel");
    for(double frequency:{s.feederFrequencyHz,s.sensorFrequencyHz})
        if(!std::isfinite(frequency)||frequency<0||frequency>20)throw std::invalid_argument("EcoCycle frequency");
    for(float width:{s.mainPathWidth,s.secondaryPathWidth})
        if(!std::isfinite(width)||width<2||width>5)throw std::invalid_argument("EcoCycle path width");
    if(!std::isfinite(s.cameraPitch)||std::abs(s.cameraPitch)>89)throw std::invalid_argument("EcoCycle camera pitch");
    for(std::size_t i=0;i<s.positions.size();++i) {
        if(!std::isfinite(s.yawDegrees[i]))throw std::invalid_argument("EcoCycle yaw");
        validatePose({s.positions[i],rotate(s.yawDegrees[i]),s.scale[i]});
    }
    validateLighting(s.lighting,s.cameraPosition);
}
AnimationDescriptor simple(float speed,glm::vec3 axis) {return {SimpleAnimation{SimpleMotion::rotation,axis,speed,0}};}
AnimationDescriptor gate(const ecocycle::Settings& s) {
    const auto d=s.gateDurationSeconds;Pose open;open.orientation=rotate(s.gateOpeningDeg);
    return {KeyframeTrack({{0,Pose{}},{d*.25,open},{d*.55,open},{d*.85,Pose{}},{d,Pose{}}}),d};
}
AnimationDescriptor press(const ecocycle::Settings& s) {
    const auto d=s.pressDurationSeconds;Pose down;down.position.y=-s.pressTravelMeters;
    return {KeyframeTrack({{0,Pose{}},{d*.3,down},{d*.6,down},{d*.9,Pose{}},{d,Pose{}}}),d};
}
AnimationDescriptor feeder(const ecocycle::Settings& s) {
    return {ProceduralAnimation{[a=s.feederAmplitudeMeters,f=s.feederFrequencyHz](double t) {
        const auto p=std::fmod(t*f,1.0)*glm::two_pi<double>();Pose v;
        v.position.y=a*static_cast<float>(std::sin(p));return v;
    }}};
}
AnimationDescriptor sensor(const ecocycle::Settings& s) {
    return {ProceduralAnimation{[a=s.sensorAmplitudeDegrees,f=s.sensorFrequencyHz](double t) {
        const auto p=std::fmod(t*f,1.0)*glm::two_pi<double>();Pose v;
        v.orientation=rotate(a*static_cast<float>(std::sin(p)));return v;
    }}};
}
class EcoCycleModule final:public IModule {
public:
    explicit EcoCycleModule(ecocycle::Settings s):settings_(std::move(s)){}
    std::string_view id() const noexcept override {return "ecocycle";}
    void initialize(ModuleServices& services) override {
        services_=&services;services.log("ecocycle");services.log(services.content()?"initialize content":"initialize stub");
        services.setClearColor(settings_.clearColor);
        if(auto* content=services.content()) {
            const auto entities=makeEcoCycleInstances(settings_);content->configureLighting(settings_.lighting);
            for(const auto& e:entities)content->add(e);
            for(const auto& e:shared::instances("ecocycle"))content->add(e);
        }
    }
    void update(double) override {}
    void shutdown() noexcept override {
        if(services_) {services_->log("ecocycle");services_->log(services_->content()?"shutdown content":"shutdown stub");services_=nullptr;}
    }
private:
    ecocycle::Settings settings_;
    ModuleServices* services_=nullptr; // Prestado; ModuleSession posee capacidad y compensa fallos parciales.
};
}
std::string ecoCycleModelId(ecocycle::Model model) {
    const auto n=static_cast<int>(model);if(n<1||n>24)throw std::invalid_argument("EcoCycle model number");
    return std::string("CYCLE-")+(n<10?"00":"0")+std::to_string(n);
}
std::vector<EntityDescriptor> makeEcoCycleInstances(const ecocycle::Settings& s) {
    validate(s);std::vector<EntityDescriptor> result;
    const auto add=[&](int n,const std::string& suffix,glm::vec3 position,std::optional<AnimationDescriptor> anim=std::nullopt,bool interactive=false,float yaw=0,glm::vec3 scale=glm::vec3(1)) {
        EntityDescriptor e;e.id=ecoCycleModelId(static_cast<ecocycle::Model>(n))+"-"+suffix;
        e.model="ecocycle/models/"+e.id.substr(0,9)+".glb";
        e.transform={position,rotate(s.yawDegrees[n-1]+yaw),s.scale[n-1]*scale};validatePose(e.transform);
        e.animation=std::move(anim);if(interactive)e.interaction=InteractionDescriptor{};
        if(s.visible[n-1])result.push_back(std::move(e));
    };
    for(int n=1;n<=24;++n) {
        std::optional<AnimationDescriptor> anim;
        switch(n) {
            case 8:anim=simple(s.rollerSpeedDeg,{1,0,0});break;
            case 11:anim=simple(s.magnetSpeedDeg,{1,0,0});break;
            case 16:anim=simple(s.rotorSpeedDeg,{1,0,0});break;
            case 22:anim=simple(s.compostSpeedDeg,{0,1,0});break;
            case 10:anim=gate(s);break;case 13:anim=press(s);break;
            case 6:anim=feeder(s);break;case 9:anim=sensor(s);break;
            default:break;
        }
        const bool interactive=n==5||n==14||n==17||n==19||n==21||n==23||n==24;
        add(n,"main",s.positions[n-1],std::move(anim),interactive,0,n==3?glm::vec3(s.mainPathWidth/3.8f,1,1):glm::vec3(1));
    }
    for(std::size_t i=0;i<s.extraPaths.size();++i)
        add(3,"path-"+std::to_string(i),s.extraPaths[i],std::nullopt,false,i<3?0.0f:90.0f,{(i<3?s.mainPathWidth:s.secondaryPathWidth)/3.8f,1,i<3?1:1.8f});
    const auto extras=[&](int n,const auto& positions) {for(std::size_t i=0;i<positions.size();++i)add(n,"stock-"+std::to_string(i),positions[i]);};
    extras(4,s.extraKiosks);extras(14,s.extraBales);extras(19,s.extraSpools);extras(24,s.extraChairs);
    return result;
}
std::unique_ptr<IModule> makeEcoCycle(const ecocycle::Settings& s) {return std::make_unique<EcoCycleModule>(s);}
std::unique_ptr<IModule> makeEcoCycle() {return makeEcoCycle(ecocycle::settings);}
}
