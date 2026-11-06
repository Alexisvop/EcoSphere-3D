#include "AquaBio.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
namespace {
glm::quat rotation(float degrees, glm::vec3 axis = {0,1,0}) {
    return glm::angleAxis(glm::radians(degrees), axis);
}
void validate(const aquabio::Settings& s) {
    if (!finiteVector(s.clearColor) || glm::any(glm::lessThan(s.clearColor,glm::vec3(0))) ||
        glm::any(glm::greaterThan(s.clearColor,glm::vec3(1))))
        throw std::invalid_argument("Invalid AquaBio clear color");
    for (float v : {s.pumpSpeedDeg,s.scraperSpeedDeg,s.flowSpeedDeg,s.cycleSpeedDeg})
        if (!std::isfinite(v) || std::abs(v)>1440) throw std::invalid_argument("Invalid AquaBio rotation speed");
    for (double v : {s.gutterDurationSeconds,s.cartridgeDurationSeconds})
        if (!std::isfinite(v) || v<=.1) throw std::invalid_argument("Invalid AquaBio keyframe duration");
    for (float v : {s.gutterSwingDegrees,s.reedAmplitudeDegrees})
        if (!std::isfinite(v) || std::abs(v)>30) throw std::invalid_argument("Invalid AquaBio swing");
    for (float v : {s.cartridgeLiftMeters,s.cartridgeTravelMeters,s.floatingAmplitudeMeters})
        if (!std::isfinite(v) || v<0 || v>2) throw std::invalid_argument("Invalid AquaBio travel");
    for (double v : {s.floatingFrequencyHz,s.reedFrequencyHz})
        if (!std::isfinite(v) || v<0 || v>20) throw std::invalid_argument("Invalid AquaBio frequency");
    for (std::size_t i=0;i<s.positions.size();++i) {
        if (!std::isfinite(s.yawDegrees[i])) throw std::invalid_argument("Invalid AquaBio yaw");
        validatePose({s.positions[i], rotation(s.yawDegrees[i]), s.scale[i]});
    }
    validateLighting(s.lighting, {0,1.7f,6});
}
AnimationDescriptor simple(float speed, glm::vec3 axis) {
    return {SimpleAnimation{SimpleMotion::rotation,axis,speed,0}};
}
AnimationDescriptor gutter(const aquabio::Settings& s) {
    const double d=s.gutterDurationSeconds;
    KeyframeTrack track({
        {0, {{}, rotation(-s.gutterSwingDegrees,{0,0,1}), glm::vec3(1)}},
        {d*.25, Pose{}},
        {d*.5, {{}, rotation(s.gutterSwingDegrees,{0,0,1}), glm::vec3(1)}},
        {d*.75, Pose{}},
        {d, {{}, rotation(-s.gutterSwingDegrees,{0,0,1}), glm::vec3(1)}}});
    return {std::move(track),d};
}
AnimationDescriptor cartridge(const aquabio::Settings& s) {
    const double d=s.cartridgeDurationSeconds;
    Pose lifted, placed;
    lifted.position={0,s.cartridgeLiftMeters,0};
    placed.position={s.cartridgeTravelMeters,s.cartridgeLiftMeters,0};
    placed.orientation=rotation(24);
    return {KeyframeTrack({{0,Pose{}},{d*.2,lifted},{d*.4,placed},{d*.65,placed},{d*.85,lifted},{d,Pose{}}}),d};
}
AnimationDescriptor floating(const aquabio::Settings& s) {
    ProceduralAnimation fn=[a=s.floatingAmplitudeMeters,f=s.floatingFrequencyHz](double t) {
        const double p=std::fmod(t*f,1.0)*glm::two_pi<double>();
        Pose v; v.position.y=a*static_cast<float>(std::sin(p));
        v.orientation=rotation(2*static_cast<float>(std::sin(p+.4)),{0,0,1});
        return v;
    };
    return {std::move(fn)};
}
AnimationDescriptor reeds(const aquabio::Settings& s) {
    ProceduralAnimation fn=[a=s.reedAmplitudeDegrees,f=s.reedFrequencyHz](double t) {
        const double p=std::fmod(t*f,1.0)*glm::two_pi<double>();
        Pose v; v.orientation=rotation(a*static_cast<float>(.75*std::sin(p)+.25*std::sin(2*p+.3)),{0,0,1});
        return v;
    };
    return {std::move(fn)};
}
class AquaBioModule final : public IModule {
public:
    explicit AquaBioModule(aquabio::Settings s) : settings_(std::move(s)) {}
    std::string_view id() const noexcept override { return "aquabio"; }
    void initialize(ModuleServices& services) override {
        services_=&services;
        services.log("aquabio"); services.log(services.content() ? "initialize content" : "initialize stub");
        services.setClearColor(settings_.clearColor);
        if (auto* content=services.content()) {
            const auto entities=makeAquaBioInstances(settings_);
            content->configureLighting(settings_.lighting);
            for (const auto& e:entities) content->add(e);
            for (const auto& e:shared::instances("aquabio")) content->add(e);
        }
    }
    void update(double) override {}
    void shutdown() noexcept override {
        if (services_) {
            services_->log("aquabio"); services_->log(services_->content() ? "shutdown content" : "shutdown stub");
            services_=nullptr;
        }
    }
private:
    aquabio::Settings settings_;
    ModuleServices* services_=nullptr; // Prestado hasta shutdown; ModuleSession posee la capacidad.
};
}

std::string aquaBioModelId(aquabio::Model model) {
    const int n=static_cast<int>(model);
    if(n<1 || n>28)throw std::invalid_argument("Invalid AquaBio model number");
    return std::string("AQUA-")+(n<10 ? "00" : "0")+std::to_string(n);
}
std::vector<EntityDescriptor> makeAquaBioInstances(const aquabio::Settings& s) {
    validate(s);
    using aquabio::Model;
    std::vector<EntityDescriptor> result;
    const auto add=[&](Model model, const std::string& suffix, glm::vec3 position, bool interactive=false,
                       std::optional<AnimationDescriptor> animation=std::nullopt, float yaw=0, glm::vec3 instanceScale=glm::vec3(1)) {
        const auto i=static_cast<std::size_t>(model)-1;
        EntityDescriptor e;
        e.id=aquaBioModelId(model)+"-"+suffix;
        e.model="aquabio/models/"+aquaBioModelId(model)+".glb";
        e.transform={position,rotation(s.yawDegrees[i]+yaw),s.scale[i]*instanceScale};
        validatePose(e.transform);
        if(interactive)e.interaction=InteractionDescriptor{};
        e.animation=std::move(animation);
        if(s.visible[i])result.push_back(std::move(e));
    };
    // Muestra frontal manipulable; el impulsor de la bomba conserva su propio reloj.
    add(Model::impeller,"demo",s.impellerDemoPosition,true);
    const std::array<const char*,28> names{{"main","entry","entry","collector","adjustable","cistern",
        "inlet","housing","pump","tank","scraper","sand","carbon","extraction","uv","treated",
        "distribution","manual","indicator","sampling","cycle","precipitation","basin","floating",
        "sway","tree","shelter","demo"}};
    for(int n=1;n<=28;++n) {
        std::optional<AnimationDescriptor> animation;
        switch(n) {
            case 5: animation=gutter(s); break;
            case 9: animation=simple(s.pumpSpeedDeg,{0,0,1}); break;
            case 11: animation=simple(s.scraperSpeedDeg,{0,1,0}); break;
            case 14: animation=cartridge(s); break;
            case 19: animation=simple(s.flowSpeedDeg,{0,0,1}); break;
            case 21: animation=simple(s.cycleSpeedDeg,{0,0,1}); break;
            case 24: animation=floating(s); break;
            case 25: animation=reeds(s); break;
            default: break;
        }
        const bool interactive=n==6 || n==13 || n==18 || n==20 || n==28;
        add(static_cast<Model>(n),names[n-1],s.positions[n-1],interactive,std::move(animation),0,n==3 ? s.mainWalkwayScale : glm::vec3(1));
    }
    add(Model::cartridge,"demo",s.cartridgeDemoPosition,true);
    for(std::size_t i=0;i<s.extraWalkways.size();++i)
        add(Model::walkway,"path-"+std::to_string(i),s.extraWalkways[i],false,std::nullopt,s.extraWalkwayYaws[i],s.extraWalkwayScales[i]);
    add(Model::pipes,"branch",s.extraPipesPosition);
    // Conserva los IDs static-* para no romper referencias; ahora también se animan.
    for(std::size_t i=0;i<s.extraFloatingPlants.size();++i)
        add(Model::floatingPlant,"static-"+std::to_string(i),s.extraFloatingPlants[i],false,floating(s),static_cast<float>(i)*47);
    for(std::size_t i=0;i<s.extraReeds.size();++i)
        add(Model::reeds,"static-"+std::to_string(i),s.extraReeds[i],false,reeds(s),static_cast<float>(i)*31);
    for(std::size_t i=0;i<s.extraTrees.size();++i)
        add(Model::riparianTree,"bank-"+std::to_string(i),s.extraTrees[i]);
    return result;
}
std::unique_ptr<IModule> makeAquaBio(const aquabio::Settings& settings) {
    return std::make_unique<AquaBioModule>(settings);
}
std::unique_ptr<IModule> makeAquaBio() { return makeAquaBio(aquabio::settings); }
}
