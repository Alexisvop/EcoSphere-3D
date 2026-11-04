#include "LessonModule.hpp"
#include <ecosphere/sdk/ModuleServices.hpp>
#include <ecosphere/core/ModuleSession.hpp>
#include <iostream>
#include <stdexcept>

struct Content final : ecosphere::ContentServices {
    std::size_t& count;
    bool& active;
    Content(std::size_t& c,bool& a):count(c),active(a){active=true;}
    ~Content() override {active=false;}
    void add(const ecosphere::EntityDescriptor& e) override {ecosphere::validatePose(e.transform);++count;}
    void setTransform(const std::string&,const ecosphere::Pose& p) override {(void)p.matrix();}
    void manipulate(const std::string&,glm::vec3,glm::quat,float) override {}
    void setPlaying(bool) override {}
    void restart() override {}
    void configureLighting(const ecosphere::LightingSettings& s) override {ecosphere::validateLighting(s,{});}
};
struct Host final : ecosphere::ModuleServices {
    std::size_t count=0;bool active=false;
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view) override {return std::make_unique<Content>(count,active);}
};
int main() {
    try {
        Host host;{ecosphere::ModuleSession session(lesson::makeModule(),host);session.update(.25);}
        if(host.count!=3 || host.active)throw std::runtime_error("Public consumer did not register all animation families");
        ecosphere::SimpleAnimation invalid;invalid.motion=static_cast<ecosphere::SimpleMotion>(99);
        try {(void)invalid.sample(0);throw std::runtime_error("Invalid public enum accepted");}
        catch(const std::invalid_argument&) {}
        std::cout<<"EXTERNAL SDK PASS: public headers and declared CPU libraries only; three animation families and source-compatible ModuleSession\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
