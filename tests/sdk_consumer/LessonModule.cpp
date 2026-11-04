#include "LessonModule.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace lesson {
namespace {
class Module final : public ecosphere::IModule {
public:
    explicit Module(Settings settings) : settings_(std::move(settings)) {}
    std::string_view id() const noexcept override {return settings_.owner;}
    void initialize(ecosphere::ModuleServices& services) override {
        content_=services.content();
        if(!content_)throw std::runtime_error("Lesson requires ContentServices");
        using namespace ecosphere;
        LightingSettings lights;lights.legacyUnlit=false;lights.ambient=.16f;
        lights.point.color={1,.8f,.45f};content_->configureLighting(lights);
        for(int index=0;index<3;++index) {
            EntityDescriptor entity;
            entity.id=index==0 ? "simple" : index==1 ? "keyframe" : "procedural";
            entity.model=settings_.model;
            entity.transform.position=settings_.position+glm::vec3((index-1)*2,0,0);
            entity.transform.scale=glm::vec3(settings_.scale);
            entity.interaction=InteractionDescriptor{};
            entity.visual=VisualProperties{{.6f,.85f,1,1},MaterialSettings{false}};
            if(index==0)entity.animation=AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation,{0,1,0},settings_.rotationSpeed,0}};
            if(index==1) {
                Pose upper;upper.position.y=.8f;
                entity.animation=AnimationDescriptor{KeyframeTrack({{0,Pose{}},{2,upper},{4,Pose{}}}),4};
            }
            if(index==2)entity.animation=AnimationDescriptor{ProceduralAnimation{[](double t) {
                Pose offset;offset.position.x=.5f*static_cast<float>(std::sin(t*2));return offset;
            }}};
            content_->add(entity);
        }
    }
    void update(double seconds) override {
        elapsed_+=seconds;
        ecosphere::Pose pose;pose.position=settings_.position+glm::vec3(-2,.1f*static_cast<float>(elapsed_),0);
        pose.scale=glm::vec3(settings_.scale);
        content_->setTransform("simple",pose);
    }
    void shutdown() noexcept override {content_=nullptr;}
private:
    Settings settings_;
    ecosphere::ContentServices* content_=nullptr; // Prestado hasta shutdown, nunca se destruye aquí.
    double elapsed_=0;
};
}
std::unique_ptr<ecosphere::IModule> makeModule(Settings settings) {return std::make_unique<Module>(std::move(settings));}
}
