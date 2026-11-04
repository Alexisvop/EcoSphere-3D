#include "Plaza.hpp"
#include "shared/SharedResources.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
void validate(const plaza::Settings& s) {
    for (float v : {s.radius,s.pathWidth,s.pathLength,s.treeRingOffset})
        if (!std::isfinite(v) || v <= 0) throw std::invalid_argument("Plaza dimensions must be finite and positive");
    if (s.benchCount>s.benches.size() || s.smallTreeCount>s.smallTrees.size() ||
        s.broadTreeCount>32 || s.lampCount>32 || s.rockCount>64 || s.grassCount>64)
        throw std::invalid_argument("Plaza instance count exceeds configured capacity");
    if (!finiteVector(s.cameraStart) || !std::isfinite(s.cameraYaw) || !std::isfinite(s.cameraPitch) ||
        std::abs(s.cameraPitch)>=89 || s.windowWidth<=0 || s.windowHeight<=0)
        throw std::invalid_argument("Invalid plaza camera/window");
    validateLighting(s.lighting,s.cameraStart);
}
float towardCenter(glm::vec3 p) { return glm::degrees(std::atan2(-p.x,-p.z)); }
class PlazaModule final : public IModule {
public:
    explicit PlazaModule(plaza::Settings settings) : settings_(std::move(settings)) {}
    std::string_view id() const noexcept override { return "hub"; }
    void initialize(ModuleServices& services) override {
        services_=&services;
        auto* content=services.content();
        if (!content) throw std::runtime_error("Plaza requires the candidate content service");
        const auto entities=makePlazaInstances(settings_);
        content->configureLighting(settings_.lighting);
        for (const auto& entity : entities) content->add(entity);
        for (const auto& entity : shared::instances("hub")) content->add(entity);
        services.log("hub initialize: plaza GLB content registered");
    }
    void update(double) override {}
    void shutdown() noexcept override {
        if (services_) services_->log("hub shutdown: session releases plaza content");
        services_=nullptr;
    }
private:
    plaza::Settings settings_;
    ModuleServices* services_=nullptr;
};
}
std::string plazaModelId(plaza::Model model) {
    const int n=static_cast<int>(model);
    if (n<1 || n>25) throw std::invalid_argument("Invalid plaza model number");
    return std::string("PLAZA-")+(n<10 ? "00" : "0")+std::to_string(n);
}
std::vector<EntityDescriptor> makePlazaInstances(const plaza::Settings& s) {
    validate(s);
    using plaza::Model;
    std::vector<EntityDescriptor> result;
    const auto add=[&](Model model,const std::string& name,glm::vec3 position,float yaw=0,glm::vec3 scale=glm::vec3(1),
                       std::optional<glm::vec4> tint=std::nullopt) {
        const auto index=static_cast<std::size_t>(model)-1;
        if (!s.visible.at(index)) return;
        yaw+=s.modelYawDegrees.at(index);
        if (!std::isfinite(yaw)) throw std::invalid_argument("Invalid plaza orientation");
        EntityDescriptor e;
        e.id=plazaModelId(model)+"-"+name;
        e.model="plaza/models/"+plazaModelId(model)+".glb";
        if (model==Model::arch && name!="energy")
            e.model="plaza/models/variants/PLAZA-008-"+std::string(name=="water" ? "aquabio" : "ecocycle")+".glb";
        e.transform={position,glm::angleAxis(glm::radians(yaw),glm::vec3(0,1,0)),scale*s.modelScale.at(index)};
        validatePose(e.transform);
        if (tint) { e.visual=VisualProperties{}; e.visual->tint=*tint; e.visual->material.unlit=false; e.visual->material.metallic=.45f; }
        if (model==Model::kiosk) e.interaction=InteractionDescriptor{};
        result.push_back(std::move(e));
    };
    const float groundScale=s.radius/10.5f, perimeterScale=(s.radius+1.5f)/12;
    const float access=s.radius+s.pathLength-.5f, pathCenter=s.radius+s.pathLength*.5f-.5f;
    add(Model::pavement,"main",{0,0,0},0,{groundScale,1,groundScale});
    add(Model::perimeter,"verge",{0,0,0},0,{perimeterScale,1,perimeterScale});
    add(Model::pedestal,"center",{0,0,0});
    add(Model::globe,"sculpture",s.sculpturePosition);
    add(Model::basin,"fountain",{0,0,0});
    add(Model::jets,"water",{0,0,0});
    const glm::vec3 pathScale{s.pathWidth/3.6f,1,s.pathLength/8};
    add(Model::path,"entry",{0,0,pathCenter},0,pathScale);
    add(Model::path,"energy",{-pathCenter,0,0},90,pathScale);
    add(Model::path,"water",{0,0,-pathCenter},0,pathScale);
    add(Model::path,"cycle",{pathCenter,0,0},90,pathScale);
    add(Model::arch,"energy",{-access,0,0},90);
    add(Model::arch,"water",{0,0,-access});
    add(Model::arch,"cycle",{access,0,0},-90);
    add(Model::platform,"energy",{-access,0,0},90);
    add(Model::platform,"water",{0,0,-access});
    add(Model::platform,"cycle",{access,0,0},-90);
    add(Model::pergola,"garden",s.pergolaPosition);
    add(Model::solarCanopy,"energy",s.solarPosition);
    add(Model::signpost,"entry",s.signPosition);
    add(Model::kiosk,"information",s.kioskPosition);
    add(Model::panel,"water",s.panels[0]);
    add(Model::panel,"cycle",s.panels[1],-45);
    for (unsigned i=0;i<s.benchCount;++i) add(Model::bench,"bench-"+std::to_string(i),s.benches[i],towardCenter(s.benches[i]));
    for (unsigned i=0;i<s.lampCount;++i) {
        const float a=glm::radians(22.5f+360.0f*i/s.lampCount);
        const glm::vec3 p{(s.radius-1)*std::sin(a),0,(s.radius-1)*std::cos(a)};
        add(Model::lamp,"lamp-"+std::to_string(i),p,towardCenter(p));
    }
    add(Model::bicycleRack,"entry",s.bicyclePosition);
    for (std::size_t i=0;i<s.wasteStations.size();++i)
        add(Model::wasteStation,"station-"+std::to_string(i),s.wasteStations[i],towardCenter(s.wasteStations[i]));
    add(Model::drinkingFountain,"west",s.drinkingFountains[0]);
    add(Model::drinkingFountain,"east",s.drinkingFountains[1]);
    for (std::size_t i=0;i<s.planters.size();++i) {
        const auto p=s.planters[i]; add(Model::planter,"bed-"+std::to_string(i),p);
        for (int side : {-1,1}) add(Model::flowerBush,"bed-"+std::to_string(i)+(side<0?"-left":"-right"),p+glm::vec3(side*.62f,.64f,0));
    }
    for (unsigned i=0;i<s.broadTreeCount;++i) {
        const float a=glm::radians(22.5f+360.0f*i/s.broadTreeCount),r=s.radius+s.treeRingOffset;
        add(Model::broadTree,"tree-"+std::to_string(i),{r*std::sin(a),0,r*std::cos(a)},i*27.0f);
    }
    for (unsigned i=0;i<s.smallTreeCount;++i) add(Model::smallTree,"ornamental-"+std::to_string(i),s.smallTrees[i],i*40.0f);
    for (unsigned i=0;i<s.grassCount;++i) {
        const unsigned slots=(s.grassCount+3)/4;
        const float a=glm::radians((i%4)*90+15+60*(float(i/4)+.5f)/slots),r=s.radius+2.4f+(i%2)*.6f;
        add(Model::grasses,"grass-"+std::to_string(i),{r*std::sin(a),0,r*std::cos(a)},i*31.0f);
    }
    for (unsigned i=0;i<s.rockCount;++i) {
        const unsigned slots=(s.rockCount+3)/4;
        const float a=glm::radians((i%4)*90+15+60*(float(i/4)+.5f)/slots),r=s.radius+4;
        add(Model::rocks,"rocks-"+std::to_string(i),{r*std::sin(a),0,r*std::cos(a)},i*37.0f);
    }
    return result;
}
std::unique_ptr<IModule> makePlaza(const plaza::Settings& settings) {
    return std::make_unique<PlazaModule>(settings);
}
}
