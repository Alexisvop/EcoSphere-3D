#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include "content/ContentRegistry.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function) {
    try { function(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected rejection");
}
bool near(const glm::mat4& a, const glm::mat4& b) {
    for (int c=0;c<4;++c) for (int r=0;r<4;++r) if (std::abs(a[c][r]-b[c][r])>0.0001f) return false;
    return true;
}
struct Host final : ecosphere::ModuleServices {
    ecosphere::ContentRegistry& registry;
    explicit Host(ecosphere::ContentRegistry& value) : registry(value) {}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override { return registry.open(owner); }
};
struct PartialModule final : ecosphere::IModule {
    ecosphere::EntityDescriptor entity;
    bool fail, &closed;
    PartialModule(ecosphere::EntityDescriptor e, bool failure, bool& closing) : entity(std::move(e)), fail(failure), closed(closing) {}
    std::string_view id() const noexcept override { return "partial"; }
    void initialize(ecosphere::ModuleServices& services) override {
        require(services.content()!=nullptr,"Session did not bind content");
        // El módulo no debe poder abrir una capacidad con otro propietario.
        require(!services.openContent("other"),"Module escaped its session");
        services.content()->add(entity);
        if (fail) throw std::runtime_error("Initialization failed after registration");
    }
    void update(double) override {}
    void shutdown() noexcept override { closed=true; } // La sesión limpia aunque el módulo lo omita.
};
}
int main(int argc, char** argv) {
    try {
        using namespace ecosphere;
        require(argc==3,"Expected asset root and scratch");
        ContentRegistry registry(argv[1]);
        rejects([&]{registry.open("");}); rejects([&]{registry.open("bad/owner");});
        auto first=registry.open("first");
        rejects([&]{registry.open("first");});
        EntityDescriptor d; d.id="same"; d.model="models/blender_probe_unlit.glb";
        d.transform.position={1,2,3};
        d.transform.orientation=glm::angleAxis(glm::radians(45.0f),glm::vec3(0,1,0));
        d.transform.scale={2,1,0.5f};
        first->add(d);
        rejects([&]{first->add(d);});
        auto second=registry.open("second"); second->add(d);
        auto placements=registry.placements();
        require(placements.size()==2 && placements[0].owner!=placements[1].owner,"Namespaces collided");
        require(placements[0].resource==placements[1].resource && placements[0].model==placements[1].model,"GLB not shared");
        require(near(placements[0].world,d.transform.matrix()),"TRS mismatch");
        const auto originalLocal=placements[0].resource->nodes.at(1).local;
        Pose changed; changed.position={5,4,3}; first->setTransform(d.id,changed);
        require(near(registry.placements()[0].world,changed.matrix()),"Transform edit failed");
        require(near(registry.placements()[1].world,d.transform.matrix()),"Transform leaked across modules");
        auto invalid=d; invalid.id=""; rejects([&]{first->add(invalid);});
        invalid.id="other/name"; rejects([&]{first->add(invalid);});
        invalid.id="invalid"; invalid.transform.scale.x=0; rejects([&]{first->add(invalid);});
        invalid=d; invalid.id="invalid"; invalid.model="models/missing.glb"; rejects([&]{first->add(invalid);});
        invalid.model="../escape.glb"; rejects([&]{first->add(invalid);});
        invalid.model=std::filesystem::absolute(argv[1]).string(); rejects([&]{first->add(invalid);});
        std::filesystem::create_directories(argv[2]);
        { std::ofstream file(std::filesystem::path(argv[2])/"bad.glb"); file<<"corrupt"; }
        ContentRegistry corrupt(argv[2]); auto bad=corrupt.open("corrupt");
        invalid.model="bad.glb"; rejects([&]{bad->add(invalid);});
        require(corrupt.placements().empty(),"Corrupt file registered an entity");
        invalid=d; invalid.id="invalid"; invalid.visual=VisualProperties{};
        invalid.visual->tint.x=std::numeric_limits<float>::quiet_NaN(); rejects([&]{first->add(invalid);});
        invalid.visual->tint.x=1; invalid.visual->material.roughness=2; rejects([&]{first->add(invalid);});
        invalid=d; invalid.id="invalid"; invalid.interaction=InteractionDescriptor{Aabb{{1,1,1},{-1,-1,-1}}};
        rejects([&]{first->add(invalid);});
        invalid=d; invalid.id="invalid"; invalid.animation=AnimationDescriptor{};
        invalid.animation->duration=0; rejects([&]{first->add(invalid);});
        invalid.animation->duration=1; invalid.animation->speed=-1; rejects([&]{first->add(invalid);});
        invalid.animation->speed=1; invalid.animation->source=SimpleAnimation{static_cast<SimpleMotion>(99)};
        rejects([&]{first->add(invalid);});
        invalid.animation->source=ProceduralAnimation{}; rejects([&]{first->add(invalid);});
        require(registry.placements().size()==2,"Failed registration was not atomic");
        invalid=d; invalid.id="interactive"; invalid.interaction=InteractionDescriptor{};
        invalid.animation=AnimationDescriptor{SimpleAnimation{SimpleMotion::rotation,{0,1,0},90,0}};
        first->add(invalid);
        first->manipulate("interactive",{1,0,0},glm::quat(1,0,0,0),1);
        require(registry.interactions("first").selected()==std::optional<std::string>("interactive"),"Interaction registration failed");
        rejects([&]{first->manipulate(d.id,{1,0,0},glm::quat(1,0,0,0),1);});
        const auto before=registry.interactions("first").world("interactive");
        registry.update(0.5);
        require(!near(before,registry.interactions("first").world("interactive")),"Animation not evaluated");
        first->setPlaying(false);
        const auto paused=registry.interactions("first").world("interactive"); registry.update(1);
        require(near(paused,registry.interactions("first").world("interactive")),"Pause failed");
        first->restart(); require(near(before,registry.interactions("first").world("interactive")),"Restart lost manipulation");
        LightingSettings lights; lights.legacyUnlit=false; first->configureLighting(lights);
        require(registry.lighting("first")->legacyUnlit==false && !registry.lighting("second"),"Lighting leaked");
        lights.point.intensity=std::numeric_limits<float>::infinity(); rejects([&]{first->configureLighting(lights);});
        rejects([&]{registry.update(-1);});
        rejects([&]{first->setTransform("missing",Pose{});});
        auto faulty=d; faulty.id="faulty";
        faulty.animation=AnimationDescriptor{ProceduralAnimation{[](double t){
            if(t>0) throw std::runtime_error("Procedural evaluation failed");
            return Pose{};
        }}};
        first->add(faulty);
        const auto checkpoint=registry.interactions("first").world("interactive");
        rejects([&]{registry.update(1);});
        require(near(checkpoint,registry.interactions("first").world("interactive")),"Failed update partially changed installation");
        require(near(registry.placements()[0].resource->nodes.at(1).local,originalLocal),"Original glTF hierarchy mutated");
        std::weak_ptr<const Model> retained=placements[0].resource;
        first.reset(); require(registry.placements().size()==1,"Owner cleanup removed other installation");
        { auto newcomer=registry.open("newcomer"); newcomer->add(d);
          const auto shared=registry.placements();
          require(shared[0].resource==shared[1].resource,"Resource sharing lost after another module closed"); }
        second->setTransform(d.id,Pose{});
        require(!retained.expired(),"Shared resource invalidated on first shutdown");
        second.reset(); require(registry.placements().empty(),"Entities survived session close");
        require(!retained.expired(),"Snapshot has dangling model");
        placements.clear(); require(retained.expired(),"Last reference did not release resource");
        auto reopen=registry.open("first"); reopen.reset();
        Host host(registry); bool closed=false;
        rejects([&]{ModuleSession session(std::make_unique<PartialModule>(d,true,closed),host);});
        require(closed && registry.placements().empty(),"Partial initialization cleanup failed");
        auto missing=d; missing.model="models/missing.glb"; closed=false;
        rejects([&]{ModuleSession session(std::make_unique<PartialModule>(missing,false,closed),host);});
        require(closed && registry.placements().empty(),"Load failure did not close session");
        closed=false;
        { ModuleSession session(std::make_unique<PartialModule>(d,false,closed),host);
          require(registry.placements().size()==1,"Session registration missing"); session.update(0.25); }
        require(closed && registry.placements().empty(),"Session failed to clean forgotten entities");
        // La capacidad comparte el estado CPU y puede cerrarse después de destruir la fachada.
        std::unique_ptr<ContentServices> survivor;
        { ContentRegistry temporary(argv[1]); survivor=temporary.open("survivor"); survivor->add(d); }
        survivor->setTransform(d.id,Pose{}); survivor.reset();
        std::cout<<"Content registry PASS: namespaces, TRS, GLB sharing, validation/rollback, interaction, playback, lighting, lifetime and session cleanup\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
