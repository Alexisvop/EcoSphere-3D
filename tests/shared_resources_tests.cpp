#include "shared/SharedResources.hpp"
#include "world/Plaza.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "modules/EcoCycle/EcoCycle.hpp"
#include "assets/AssetManager.hpp"
#include "content/ContentRegistry.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using namespace ecosphere;
void require(bool ok,const std::string& message) {if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F fn) {try{fn();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
const std::array<std::string,4> owners{{"hub","ecoenergy","aquabio","ecocycle"}};
using Factory=std::unique_ptr<IModule>(*)();
const std::array<Factory,4> factories{{+[](){return makePlaza();},makeEcoEnergy,makeAquaBio,makeEcoCycle}};
const std::array<std::size_t,4> originalCounts{{90,60,52,38}};
const std::array<std::size_t,4> addedCounts{{2,3,3,4}};
using Shape=std::set<std::array<int,3>>;
struct Inspection {Shape shape;glm::vec3 lo,hi;std::size_t triangles=0;};
Inspection inspect(const Model& model) {
    Inspection r;r.lo=glm::vec3(std::numeric_limits<float>::infinity());r.hi=-r.lo;
    std::vector<glm::vec3> points;
    require(!model.meshes.empty() && !model.nodes.empty(),"Empty shared geometry");
    model.forEach([&](std::size_t i,const glm::mat4& world) {
        (void)normalMatrix(world);const auto& mesh=model.meshes.at(i);validateMaterial(mesh.material);
        for(const auto& v:mesh.vertices) {
            require(finiteVector(v.position) && finiteVector(v.normal) && std::isfinite(v.uv.x) && std::isfinite(v.uv.y),"Nonfinite vertex/UV");
            require(std::abs(glm::length(v.normal)-1)<.01f,"Invalid normal");
            const auto p=glm::vec3(world*glm::vec4(v.position,1));points.push_back(p);r.lo=glm::min(r.lo,p);r.hi=glm::max(r.hi,p);
        }
        const auto count=mesh.indices.empty()?mesh.vertices.size():mesh.indices.size();require(count>0 && count%3==0,"Nontriangle geometry");r.triangles+=count/3;
        for(std::size_t t=0;t<count;t+=3) {
            const auto v=[&](std::size_t n)->const Vertex&{return mesh.vertices.at(mesh.indices.empty()?n:mesh.indices.at(n));};
            const auto& a=v(t);const auto& b=v(t+1);const auto& c=v(t+2);const auto cross=glm::cross(b.position-a.position,c.position-a.position);
            require(glm::length(cross)>1e-10f && glm::dot(cross,a.normal+b.normal+c.normal)>0,"Degenerate triangle or winding");
        }
    });
    const auto size=r.hi-r.lo;require(glm::all(glm::greaterThan(size,glm::vec3(0))),"Invalid imported dimensions");
    for(const auto& p:points){const auto q=(p-r.lo)/size;r.shape.insert({int(std::lround(q.x*10000)),int(std::lround(q.y*10000)),int(std::lround(q.z*10000))});}
    return r;
}
std::string instanceCsv() {
    std::ostringstream out;out<<std::setprecision(9);
    out<<"instance_id,owner,model_id,glb_file,x,y,z,qw,qx,qy,qz,sx,sy,sz,interactive,animation_family\n";
    for(const auto& owner:owners)for(const auto& e:shared::instances(owner)) {
        const auto& p=e.transform;const auto q=glm::normalize(p.orientation);
        out<<e.id<<','<<owner<<','<<e.id.substr(0,10)<<",assets/"<<e.model<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','
           <<q.w<<','<<q.x<<','<<q.y<<','<<q.z<<','<<p.scale.x<<','<<p.scale.y<<','<<p.scale.z<<",false,none\n";
    }
    return out.str();
}
struct Host:ModuleServices {
    ContentRegistry& registry;explicit Host(ContentRegistry& r):registry(r){}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override {return registry.open(owner);}
};
// Fault injection delegates to the real importer, after one shared resource already registered.
struct MissingShared final:ContentServices {
    std::unique_ptr<ContentServices> actual;unsigned added=0;
    explicit MissingShared(std::unique_ptr<ContentServices> p):actual(std::move(p)){}
    void add(const EntityDescriptor& e) override {
        auto d=e;if(shared::isShared(d.id) && ++added==2)d.model="shared/models/not-present.glb";actual->add(d);
    }
    void setTransform(const std::string& id,const Pose& p) override {actual->setTransform(id,p);}
    void manipulate(const std::string& id,glm::vec3 p,glm::quat q,float s) override {actual->manipulate(id,p,q,s);}
    void setPlaying(bool b) override {actual->setPlaying(b);}
    void restart() override {actual->restart();}
    void configureLighting(const LightingSettings& l) override {actual->configureLighting(l);}
};
struct FailingHost final:Host {
    using Host::Host;
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override {return std::make_unique<MissingShared>(registry.open(owner));}
};
void boundsCsv(ContentRegistry& registry,const std::filesystem::path& path) {
    std::set<std::pair<std::string,std::string>> animated;
    for(const auto& e:makeEcoEnergyInstances())if(e.animation)animated.insert({"ecoenergy",e.id});
    for(const auto& e:makeAquaBioInstances())if(e.animation)animated.insert({"aquabio",e.id});
    for(const auto& e:makeEcoCycleInstances())if(e.animation)animated.insert({"ecocycle",e.id});
    require(animated.size()==24,"Original 24 animations changed");
    std::ofstream out(path);out.exceptions(std::ios::badbit|std::ios::failbit);out<<std::setprecision(9);
    out<<"owner,time_s,instance_id,min_x,min_y,min_z,max_x,max_y,max_z";
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<",m"<<c<<r;
    out<<'\n';
    for(int step=0;step<=200;++step) {
        if(step)registry.update(.1);
        for(const auto& p:registry.placements()) {
            if(step && !animated.count({p.owner,p.id}))continue;
            glm::vec3 lo(std::numeric_limits<float>::infinity()),hi=-lo;
            p.model->forEach([&](std::size_t i,const glm::mat4& world){for(const auto& v:p.model->meshes.at(i).vertices){const auto w=glm::vec3(world*glm::vec4(v.position,1));lo=glm::min(lo,w);hi=glm::max(hi,w);}},p.world);
            out<<p.owner<<','<<step*.1<<','<<p.id<<','<<lo.x<<','<<lo.y<<','<<lo.z<<','<<hi.x<<','<<hi.y<<','<<hi.z;
            for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<','<<p.world[c][r];
            out<<'\n';
        }
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--write-instances") {
            std::ofstream out(argv[2]);out.exceptions(std::ios::badbit|std::ios::failbit);out<<instanceCsv();return 0;
        }
        require(argc==5,"Expected asset root, shared instances CSV, import report, spatial report");
        const std::filesystem::path root=argv[1];std::ifstream recorded(argv[2]);
        require(std::string((std::istreambuf_iterator<char>(recorded)),{})==instanceCsv(),"Shared instance CSV differs from descriptors");
        AssetManager assets(root);std::set<Shape> previous,novel;unsigned originalFiles=0;
        for(const auto* folder:{"plaza/models","ecoenergy/models","aquabio/models","ecocycle/models"})
            for(const auto& p:std::filesystem::recursive_directory_iterator(root/folder))if(p.path().extension()==".glb") {
                previous.insert(inspect(*assets.loadModel(std::filesystem::relative(p.path(),root))).shape);++originalFiles;
            }
        require(originalFiles==107,"105 original designs and two existing plaza variants must remain");
        std::ofstream report(argv[3]);report.exceptions(std::ios::badbit|std::ios::failbit);
        report<<"model_id,triangles,meshes,textures,width_m,height_m,depth_m,min_x,min_y,min_z,max_x,max_y,max_z\n";
        for(int n=1;n<=5;++n) {
            const auto id="SHARED-00"+std::to_string(n);const auto path="shared/models/"+id+".glb";
            const auto m=assets.loadModel(path);const auto info=inspect(*m);const auto size=info.hi-info.lo;
            require(!previous.count(info.shape) && novel.insert(info.shape).second,"Shared design duplicates normalized existing geometry: "+id);
            require(m->textures.empty() && m->images.empty(),"Unexpected shared texture dependency");
            require(m==assets.loadModel("./shared/models/../models/"+id+".glb"),"Canonical asset cache identity");
            report<<id<<','<<info.triangles<<','<<m->meshes.size()<<','<<m->textures.size()<<','<<size.x<<','<<size.y<<','<<size.z<<','
                  <<info.lo.x<<','<<info.lo.y<<','<<info.lo.z<<','<<info.hi.x<<','<<info.hi.y<<','<<info.hi.z<<'\n';
        }
        report.close();assets.clear();rejects([]{shared::instances("unknown");});
        ContentRegistry registry(root);Host host(registry);std::array<std::unique_ptr<ModuleSession>,4> sessions;
        for(std::size_t i=0;i<4;++i)sessions[i]=std::make_unique<ModuleSession>(factories[i](),host);
        auto snapshot=registry.placements();require(snapshot.size()==252,"Expected 240 original plus 12 shared instances");
        std::map<std::string,const Model*> identity;std::map<std::string,std::set<std::string>> reuse;
        std::set<std::pair<std::string,std::string>> ids;std::weak_ptr<const Model> clock;
        unsigned sharedCount=0,originalCount=0;
        for(const auto& p:snapshot) {
            require(ids.insert({p.owner,p.id}).second,"Duplicate owner/instance identity");
            if(!shared::isShared(p.id)){++originalCount;continue;}
            ++sharedCount;const auto model=p.id.substr(0,10);require(p.model==p.resource,"Shared visual override duplicated geometry");
            if(identity.count(model))require(identity[model]==p.resource.get(),"Independent copy of shared resource");
            identity[model]=p.resource.get();reuse[model].insert(p.owner);if(model=="SHARED-001")clock=p.resource;
        }
        require(originalCount==240 && sharedCount==12 && identity.size()==5,"Global shared/original count");
        for(const auto& r:reuse)require(r.second.size()>=2,"Shared design used by fewer than two owners");
        require(reuse["SHARED-001"].size()==4,"One resource must be present in all four zones");
        require(registry.interactions("ecoenergy").objects().size()+registry.interactions("aquabio").objects().size()+registry.interactions("ecocycle").objects().size()==20,"Original 20 educational interactions changed");
        require(registry.interactions("hub").objects().size()==1,"Existing hub kiosk interaction changed");
        {
            auto alias=registry.open("canonical-proof");EntityDescriptor e;e.id="alias";e.model="./shared/models/../models/SHARED-001.glb";alias->add(e);
            for(const auto& p:registry.placements())if(p.owner=="canonical-proof")require(p.resource.get()==identity.at("SHARED-001"),"Registry canonical path not shared");
            rejects([&]{alias->add(e);});e.id="bad";e.model="../escape.glb";rejects([&]{alias->add(e);});
        }
        boundsCsv(registry,argv[4]);snapshot.clear();
        for(std::size_t i=0;i<3;++i){sessions[i].reset();require(!clock.expired(),"Shared resource freed while another owner uses it");}
        auto lastSnapshot=registry.placements();sessions[3].reset();require(registry.placements().empty() && !clock.expired(),"Snapshot must retain the last shared resource");
        lastSnapshot.clear();require(clock.expired(),"Shared resource retained after last owner/snapshot closed");
        for(std::size_t i=0;i<4;++i) {
            ContentRegistry failures(root);Host healthy(failures);FailingHost faulty(failures);
            const std::size_t survivor=i==0?3:0;ModuleSession alive(factories[survivor](),healthy);
            const auto before=failures.placements().size();rejects([&]{ModuleSession broken(factories[i](),faulty);});
            require(failures.placements().size()==before,"Shared partial failure leaked entities or removed another owner");
            {ModuleSession retry(factories[i](),healthy);require(failures.placements().size()==before+originalCounts[i]+addedCounts[i],"Retry after partial shared failure");}
            require(failures.placements().size()==before,"Independent retry cleanup");
        }
        for(int round=0;round<25;++round)for(std::size_t i=0;i<4;++i) {
            {ModuleSession reopened(factories[i](),host);require(registry.placements().size()==originalCounts[i]+addedCounts[i],"Shared reentry count");}
            require(registry.placements().empty(),"Shared reentry leaked owner");
        }
        std::cout<<"SHARED CPU PASS: 5 novel models vs 105 originals/2 variants; 240+12 instances; 20 educational interactions/24 animations unchanged; canonical identity across four owners; last-use release, snapshots, independent close, four partial failures/retries and 100 reentries; no GL context\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
