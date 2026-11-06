#include "modules/EcoCycle/EcoCycle.hpp"
#include "world/Plaza.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "content/ContentRegistry.hpp"
#include "assets/AssetManager.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/modules/Stubs.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <fstream>
#include "InstanceCsv.hpp"
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition,const std::string& message) {if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
std::string instanceCsv() {
    std::ostringstream out;out<<std::setprecision(9);
    out<<"instance_id,owner,model_id,glb_file,x,y,z,qw,qx,qy,qz,sx,sy,sz\n";
    for(const auto& e:ecosphere::makePlazaInstances()) {
        const auto& p=e.transform;const auto q=glm::normalize(p.orientation);
        out<<e.id<<",hub,"<<e.id.substr(0,9)<<",assets/"<<e.model<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','
           <<q.w<<','<<q.x<<','<<q.y<<','<<q.z<<','<<p.scale.x<<','<<p.scale.y<<','<<p.scale.z<<'\n';
    }
    return out.str();
}
struct Host final:ecosphere::ModuleServices {
    ecosphere::ContentRegistry& registry;
    explicit Host(ecosphere::ContentRegistry& r):registry(r){}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override {return registry.open(owner);}
};
}
int main(int argc,char** argv) {
    try {
        using namespace ecosphere;
        if(argc==3 && std::string(argv[1])=="--write-instances") {
            std::ofstream out(argv[2]);out.exceptions(std::ios::badbit|std::ios::failbit);out<<instanceCsv();
            std::cout<<"Instance CSV generated from PlazaSettings\n";return 0;
        }
        require(argc==5,"Expected staged assets, instances CSV, import report and scratch");
        const std::filesystem::path root=argv[1],scratch=argv[4];
        std::ifstream instanceFile(argv[2]);
        const std::string recorded((std::istreambuf_iterator<char>(instanceFile)),{});
        require(sameInstanceCsv(recorded,instanceCsv()),"Instance CSV differs from actual configuration; regenerate explicitly");
        std::ofstream report(argv[3]);report.exceptions(std::ios::badbit|std::ios::failbit);
        report<<"model_id,triangles,meshes,textures,width_m,height_m,depth_m\n";
        AssetManager assets(root);
        std::set<std::set<std::array<int,3>>> shapes;
        std::size_t total=0;
        for(int i=1;i<=27;++i) {
            const auto id=i<=25 ? plazaModelId(static_cast<plaza::Model>(i)) : std::string("PLAZA-008-")+(i==26 ? "aquabio" : "ecocycle");
            const auto path="plaza/models/"+std::string(i<=25 ? "" : "variants/")+id+".glb";
            require(std::filesystem::is_regular_file(root/path),"Missing GLB "+id);
            const auto model=assets.loadModel(path);
            require(!model->meshes.empty() && !model->nodes.empty(),"Empty model "+id);
            std::size_t tris=0;std::vector<glm::vec3> points;
            glm::vec3 lo(std::numeric_limits<float>::infinity()),hi(-std::numeric_limits<float>::infinity());
            model->forEach([&](std::size_t meshIndex,const glm::mat4& world) {
                (void)normalMatrix(world);const auto& mesh=model->meshes.at(meshIndex);validateMaterial(mesh.material);
                require(!mesh.vertices.empty(),"Empty primitive "+id);
                for(const auto& v:mesh.vertices) {
                    require(finiteVector(v.position) && finiteVector(v.normal),"Nonfinite geometry "+id);
                    require(std::abs(glm::length(v.normal)-1)<.01f,"Invalid normal "+id);
                    const auto p=glm::vec3(world*glm::vec4(v.position,1));points.push_back(p);lo=glm::min(lo,p);hi=glm::max(hi,p);
                }
                for(auto index:mesh.indices)require(index<mesh.vertices.size(),"Invalid index "+id);
                const auto n=mesh.indices.empty()?mesh.vertices.size():mesh.indices.size();
                require(n%3==0,"Nontriangle geometry "+id);tris+=n/3;
                for(std::size_t t=0;t<n;t+=3) {
                    const auto& a=mesh.vertices.at(mesh.indices.empty()?t:mesh.indices[t]);
                    const auto& b=mesh.vertices.at(mesh.indices.empty()?t+1:mesh.indices[t+1]);
                    const auto& c=mesh.vertices.at(mesh.indices.empty()?t+2:mesh.indices[t+2]);
                    const auto normal=glm::cross(b.position-a.position,c.position-a.position);
                    require(glm::length(normal)>1e-10f,"Degenerate triangle "+id);
                    require(glm::dot(normal,a.normal+b.normal+c.normal)>0,"Incoherent winding/normal "+id);
                }
            });
            require(tris>0,"No rendered triangles "+id);total+=tris;
            const auto size=hi-lo;require(glm::all(glm::greaterThan(size,glm::vec3(0))),"Invalid dimensions "+id);
            std::set<std::array<int,3>> shape;
            for(const auto& p:points) {
                const auto v=(p-lo)/size;
                shape.insert({int(std::lround(v.x*10000)),int(std::lround(v.y*10000)),int(std::lround(v.z*10000))});
            }
            if(i<=25) require(shapes.insert(std::move(shape)).second,"Duplicate geometry after position/scale normalization "+id);
            report<<id<<','<<tris<<','<<model->meshes.size()<<','<<model->textures.size()<<','<<size.x<<','<<size.y<<','<<size.z<<'\n';
            require(assets.loadModel("./"+path)==model,"Canonical resource not reused");
        }
        report.close();assets.clear();
        const auto entities=makePlazaInstances();std::set<std::string> ids,models;
        for(const auto& e:entities) {require(ids.insert(e.id).second,"Duplicate instance ID");models.insert(e.model);(void)normalMatrix(e.transform.matrix());}
        require(models.size()==27 && entities.size()>25,"25 base models plus two arch variants required");
        auto settings=plaza::settings;settings.radius=0;rejects([&]{makePlazaInstances(settings);});
        settings=plaza::settings;settings.pathWidth=std::numeric_limits<float>::quiet_NaN();rejects([&]{makePlazaInstances(settings);});
        settings=plaza::settings;settings.benchCount=100;rejects([&]{makePlazaInstances(settings);});
        settings=plaza::settings;settings.modelScale[4].x=0;rejects([&]{makePlazaInstances(settings);});
        settings=plaza::settings;settings.visible[20]=false;
        const auto hidden=makePlazaInstances(settings);
        require(hidden.size()+settings.broadTreeCount==entities.size(),"Visibility/count setting failed");
        ContentRegistry registry(root);Host host(registry);
        std::weak_ptr<const Model> retained;std::vector<ContentPlacement> snapshot;
        {
            ModuleSession hub(makePlaza(),host);
            require(registry.placements().size()==entities.size()+2,"Hub registration count mismatch");
            ModuleSession energy(makeEcoEnergy(),host),water(makeAquaBio(),host),cycle(makeEcoCycle(),host);
            require(registry.placements().size()==entities.size()+makeEcoEnergyInstances().size()+makeAquaBioInstances().size()+makeEcoCycleInstances().size()+12,"Installation compatibility failed");
            snapshot=registry.placements();std::map<std::string,const Model*> shared;
            for(const auto& p:snapshot) if(p.owner=="hub") {
                const auto modelId=p.id.find("PLAZA-008-")==0 ? p.id : p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:9);const auto found=shared.find(modelId);
                if(found!=shared.end())require(found->second==p.resource.get(),"Repeated instance reloaded its model");
                shared[modelId]=p.resource.get();retained=p.resource;
            }
            require(registry.interactions("hub").objects().size()==1,"Kiosk interaction missing");
            registry.update(.1);hub.update(.1);
            rejects([&]{registry.open("hub");});
        }
        require(registry.placements().empty() && !retained.expired(),"Session/snapshot lifetime invalid");
        snapshot.clear();require(retained.expired(),"Last model reference did not release");
        auto content=registry.open("duplicate");content->add(entities.front());rejects([&]{content->add(entities.front());});content.reset();
        std::filesystem::create_directories(scratch/"plaza/models");
        for(const auto* id:{"PLAZA-001","PLAZA-002"}) std::filesystem::copy_file(root/"plaza/models"/(std::string(id)+".glb"),
            scratch/"plaza/models"/(std::string(id)+".glb"),std::filesystem::copy_options::overwrite_existing);
        ContentRegistry missing(scratch);Host brokenHost(missing);
        rejects([&]{ModuleSession partial(makePlaza(),brokenHost);});require(missing.placements().empty(),"Partial plaza init leaked content");
        {std::ofstream corrupt(scratch/"corrupt.glb",std::ios::binary);corrupt<<"not a GLB";}
        auto broken=missing.open("corrupt");auto d=entities.front();d.model="corrupt.glb";rejects([&]{broken->add(d);});
        d.model="../escape.glb";rejects([&]{broken->add(d);});require(missing.placements().empty(),"Bad resource registered");
        std::cout<<"PLAZA CPU PASS: 25 distinct models + 2 signage variants; "<<total<<" resource triangles; "<<entities.size()
                 <<" instances; geometry/normals/TRS, routes, sharing, module compatibility, cleanup and negative cases\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
