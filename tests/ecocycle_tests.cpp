#include "modules/EcoCycle/EcoCycle.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "world/Plaza.hpp"
#include "content/ContentRegistry.hpp"
#include "assets/AssetManager.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace ecosphere;
void require(bool ok,const std::string& message) { if(!ok)throw std::runtime_error(message); }
template<class F> void rejects(F fn) {
    try {fn();} catch(const std::exception&) {return;}
    throw std::runtime_error("Expected parameter/resource rejection");
}
std::string family(const std::optional<AnimationDescriptor>& a) {
    if(!a)return "none";
    return std::visit([](const auto& value)->std::string {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,SimpleAnimation>)return "simple";
        if constexpr(std::is_same_v<T,KeyframeTrack>)return "keyframe";
        return "procedural";
    },a->source);
}
std::string instanceCsv() {
    std::ostringstream out; out<<std::setprecision(9);
    out<<"instance_id,owner,model_id,glb_file,x,y,z,qw,qx,qy,qz,sx,sy,sz,interactive,animation_family\n";
    for(const auto& e:makeEcoCycleInstances()) {
        const auto& p=e.transform; const auto q=glm::normalize(p.orientation);
        out<<e.id<<",ecocycle,"<<e.id.substr(0,9)<<",assets/installations/"<<e.model<<','
           <<p.position.x<<','<<p.position.y<<','<<p.position.z<<','<<q.w<<','<<q.x<<','<<q.y<<','<<q.z<<','
           <<p.scale.x<<','<<p.scale.y<<','<<p.scale.z<<','<<(e.interaction?"true":"false")<<','<<family(e.animation)<<'\n';
    }
    return out.str();
}
using Shape=std::set<std::array<int,3>>;
struct Inspection {Shape shape; glm::vec3 size; std::size_t triangles=0;};
Inspection inspect(const Model& model) {
    Inspection r; std::vector<glm::vec3> points;
    glm::vec3 lo(std::numeric_limits<float>::infinity()), hi(-std::numeric_limits<float>::infinity());
    require(!model.meshes.empty() && !model.nodes.empty(),"Empty imported model");
    model.forEach([&](std::size_t index,const glm::mat4& world) {
        (void)normalMatrix(world);
        const auto& mesh=model.meshes.at(index); validateMaterial(mesh.material);
        for(const auto& v:mesh.vertices) {
            require(finiteVector(v.position) && finiteVector(v.normal) && std::isfinite(v.uv.x) && std::isfinite(v.uv.y),"Nonfinite geometry/UV");
            require(std::abs(glm::length(v.normal)-1)<.01f,"Invalid normal");
            const auto p=glm::vec3(world*glm::vec4(v.position,1));
            points.push_back(p);lo=glm::min(lo,p);hi=glm::max(hi,p);
        }
        const auto count=mesh.indices.empty()?mesh.vertices.size():mesh.indices.size();
        require(count>0 && count%3==0,"Nontriangle primitive");r.triangles+=count/3;
        for(std::size_t t=0;t<count;t+=3) {
            const auto vertex=[&](std::size_t i)->const Vertex& {return mesh.vertices.at(mesh.indices.empty()?i:mesh.indices.at(i));};
            const auto& a=vertex(t);const auto& b=vertex(t+1);const auto& c=vertex(t+2);
            const auto cross=glm::cross(b.position-a.position,c.position-a.position);
            require(glm::length(cross)>1e-10f && glm::dot(cross,a.normal+b.normal+c.normal)>0,"Degenerate triangle or winding/normal mismatch");
        }
    });
    r.size=hi-lo;require(glm::all(glm::greaterThan(r.size,glm::vec3(0))),"Invalid dimensions");
    for(const auto& p:points) {
        const auto v=(p-lo)/r.size;
        r.shape.insert({int(std::lround(v.x*10000)),int(std::lround(v.y*10000)),int(std::lround(v.z*10000))});
    }
    return r;
}
struct Host final:ModuleServices {
    ContentRegistry& registry;ContentServices* water=nullptr;
    explicit Host(ContentRegistry& r):registry(r){}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ContentServices> openContent(std::string_view owner) override {
        auto content=registry.open(owner);if(owner=="ecocycle")water=content.get();return content;
    }
};
void same(const std::vector<ContentPlacement>& a,const std::vector<ContentPlacement>& b) {
    require(a.size()==b.size(),"Placement count differs");
    for(std::size_t i=0;i<a.size();++i) {
        require(a[i].id==b[i].id,"Placement identity differs");
        for(int c=0;c<4;++c)for(int r=0;r<4;++r)
            require(std::abs(a[i].world[c][r]-b[i].world[c][r])<1e-4f,"Animation drift: "+a[i].id);
    }
}
}
int main(int argc,char** argv) {
    try {
        using namespace ecosphere;
        if(argc==3 && std::string(argv[1])=="--write-instances") {
            std::ofstream out(argv[2]);out.exceptions(std::ios::badbit|std::ios::failbit);out<<instanceCsv();return 0;
        }
        require(argc==5,"Expected staged assets, instances CSV, import CSV and scratch");
        const std::filesystem::path root=argv[1],scratch=std::filesystem::path(argv[4])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::ifstream csv(argv[2]);const std::string recorded((std::istreambuf_iterator<char>(csv)),{});
        require(recorded==instanceCsv(),"EcoCycle instance CSV differs from actual descriptors");
        AssetManager assets(root);std::set<Shape> previous,shapes;
        for(const auto& dir:{"plaza/models","ecoenergy/models","aquabio/models"})
            for(const auto& file:std::filesystem::recursive_directory_iterator(root/dir))
                if(file.path().extension()==".glb")previous.insert(inspect(*assets.loadModel(std::filesystem::relative(file.path(),root))).shape);
        std::ofstream report(argv[3]);report.exceptions(std::ios::badbit|std::ios::failbit);
        report<<"model_id,triangles,meshes,textures,width_m,height_m,depth_m\n";
        std::size_t triangles=0;
        for(int n=1;n<=24;++n) {
            const auto id=ecoCycleModelId(static_cast<ecocycle::Model>(n));
            const auto path="ecocycle/models/"+id+".glb";
            const auto model=assets.loadModel(path);const auto info=inspect(*model);
            require(!previous.count(info.shape),"Geometry reused from plaza/EcoEnergy: "+id);
            require(shapes.insert(info.shape).second,"Duplicate EcoCycle geometry: "+id);
            triangles+=info.triangles;
            report<<id<<','<<info.triangles<<','<<model->meshes.size()<<','<<model->textures.size()<<','
                  <<info.size.x<<','<<info.size.y<<','<<info.size.z<<'\n';
            require(assets.loadModel("./"+path)==model,"Canonical resource not shared");
        }
        report.close();assets.clear();
        const auto entities=makeEcoCycleInstances();std::set<std::string> ids,models;
        std::map<std::string,int> families;int interactions=0;
        for(const auto& e:entities) {
            require(ids.insert(e.id).second,"Duplicate instance ID");models.insert(e.model);
            (void)normalMatrix(e.transform.matrix());families[family(e.animation)]++;
            interactions+=e.interaction.has_value();
            require(!(e.interaction && e.animation),"Animated and interactive samples must be independent");
        }
        require(entities.size()==38 && models.size()==24 && interactions==7,"Unexpected definitive counts");
        require(families["simple"]==4 && families["keyframe"]==2 && families["procedural"]==2,"Unexpected animation families");
        rejects([]{ecoCycleModelId(static_cast<ecocycle::Model>(0));});rejects([]{ecoCycleModelId(static_cast<ecocycle::Model>(25));});
        auto bad=ecocycle::settings;bad.scale[0].x=0;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.positions[0].y=NAN;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.yawDegrees[0]=NAN;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.rollerSpeedDeg=NAN;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.gateDurationSeconds=0;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.pressTravelMeters=-1;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.feederFrequencyHz=NAN;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.sensorAmplitudeDegrees=90;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.lighting.ambient=-1;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.extraPaths[0].x=NAN;rejects([&]{makeEcoCycleInstances(bad);});
        bad=ecocycle::settings;bad.visible[2]=false;require(makeEcoCycleInstances(bad).size()==30,"Walkway visibility");
        ContentRegistry single(root);Host hs(single);ModuleSession one(makeEcoCycle(),hs);single.update(5.37);
        for(int hz:{30,60,144}) {
            ContentRegistry stepped(root);Host h(stepped);ModuleSession session(makeEcoCycle(),h);
            for(int i=0;i<5*hz;++i)stepped.update(1.0/hz);stepped.update(.37);
            same(single.placements(),stepped.placements());
        }
        // Límites reales importados: base y barrido temporal, sin contexto OpenGL.
        {
            ContentRegistry spatial(root);Host h(spatial);ModuleSession session(makeEcoCycle(),h);
            std::ofstream out(std::filesystem::path(argv[3]).parent_path()/"ecocycle-space-bounds.csv");
            out.exceptions(std::ios::badbit|std::ios::failbit);
            out<<std::setprecision(9)<<"time_s,instance_id,min_x,min_y,min_z,max_x,max_y,max_z";
            for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<",m"<<c<<r;
            out<<'\n';
            std::set<std::string> animated;
            for(const auto& e:entities)if(e.animation)animated.insert(e.id);
            for(int step=0;step<=200;++step) {
                if(step)spatial.update(.1);
                for(const auto& p:spatial.placements()) {
                    if(p.id.rfind("SHARED-",0)==0)continue;
                    if(step && !animated.count(p.id))continue;
                    glm::vec3 lo(std::numeric_limits<float>::infinity()),hi(-std::numeric_limits<float>::infinity());
                    p.model->forEach([&](std::size_t i,const glm::mat4& m) {
                        for(const auto& v:p.model->meshes.at(i).vertices) {
                            const auto point=glm::vec3(m*glm::vec4(v.position,1));lo=glm::min(lo,point);hi=glm::max(hi,point);
                        }
                    },p.world);
                    out<<step*.1<<','<<p.id<<','<<lo.x<<','<<lo.y<<','<<lo.z<<','<<hi.x<<','<<hi.y<<','<<hi.z;
                    for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<','<<p.world[c][r];
                    out<<'\n';
                }
            }
        }
        ContentRegistry registry(root);Host host(registry);std::vector<ContentPlacement> snapshot;
        std::weak_ptr<const Model> retained;std::vector<glm::mat4> pristine;
        {
            ModuleSession water(makeEcoCycle(),host);
            const auto initial=registry.placements();for(const auto& p:initial)pristine.push_back(p.world);registry.update(.4);
            const auto paused=registry.placements();host.water->setPlaying(false);registry.update(20);same(paused,registry.placements());
            host.water->setPlaying(true);registry.update(.3);
            const auto resumed=registry.placements();std::size_t advanced=0;
            for(std::size_t i=0;i<paused.size();++i)advanced+=paused[i].world!=resumed[i].world;
            require(advanced==8,"Resume must advance all eight mechanisms");
            host.water->restart();same(initial,registry.placements());
            auto& inter=registry.interactions("ecocycle");require(inter.objects().size()==7,"Interaction registration");
            for(const auto& e:entities)if(e.interaction) {
                const auto& box=inter.object(e.id).bounds;
                const auto center=glm::vec3(inter.world(e.id)*glm::vec4((box.minimum+box.maximum)*.5f,1));
                const auto offset=glm::vec3(.4f,.3f,1)*std::max(1.3f,glm::length(box.maximum-box.minimum)*1.25f);
                require(inter.pick(Ray{center+offset,glm::normalize(-offset)})==std::optional<std::string>(e.id),"Ray-pick occluded by another interactive sample: "+e.id);
                const auto before=inter.world(e.id);
                inter.manipulate({.3f,.2f,-.2f},glm::angleAxis(glm::radians(20.0f),glm::vec3(0,1,0)),1.1f);
                require(inter.world(e.id)!=before,"TRS failed");
                inter.take(Ray{{0,1.7f,6},{0,0,-1}},2.5f);require(inter.holding(),"Take failed");
                inter.follow(Ray{{.5f,1.7f,5},{0,0,-1}});inter.drop();require(!inter.holding(),"Drop failed");
            }
            inter.deselect();
            ModuleSession energy(makeEcoEnergy(),host),hub(makePlaza(),host),cycle(makeAquaBio(),host);
            require(registry.placements().size()==38+60+90+52+12,"Four-owner coexistence failed");
            std::map<std::string,const Model*> resources;
            snapshot=registry.placements();
            for(const auto& p:snapshot)if(p.owner=="ecocycle") {
                require(p.model==p.resource,"Unnecessary visual copy");const auto id=p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:9);
                if(resources.count(id))require(resources[id]==p.resource.get(),"Repeated mesh reloaded");
                resources[id]=p.resource.get();retained=p.resource;
            }
            require(resources.size()==28,"Missing original or shared model");rejects([&]{registry.open("ecocycle");});
        }
        require(registry.placements().empty() && !retained.expired(),"Session cleanup or snapshot lifetime");
        snapshot.clear();require(retained.expired(),"Model leaked after last snapshot");
        { ModuleSession reopened(makeEcoCycle(),host);const auto placements=registry.placements();
          require(placements.size()==pristine.size(),"Reentry placement count");
          for(std::size_t i=0;i<pristine.size();++i)for(int c=0;c<4;++c)for(int r=0;r<4;++r)
              require(std::abs(pristine[i][c][r]-placements[i].world[c][r])<1e-4f,"Manipulation persisted after reentry");
          require(!registry.interactions("ecocycle").selected(),"Selection persisted after reentry"); }
        std::filesystem::create_directories(scratch/"ecocycle/models");
        std::filesystem::copy_file(root/"ecocycle/models/CYCLE-001.glb",scratch/"ecocycle/models/CYCLE-001.glb",std::filesystem::copy_options::overwrite_existing);
        ContentRegistry broken(scratch);Host hb(broken);rejects([&]{ModuleSession failure(makeEcoCycle(),hb);});
        require(broken.placements().empty(),"Partial initialization leaked");
        {std::ofstream corrupt(scratch/"corrupt.glb");corrupt<<"invalid GLB";}
        auto content=broken.open("bad");auto e=entities.front();e.model="corrupt.glb";rejects([&]{content->add(e);});
        e.model="../escape.glb";rejects([&]{content->add(e);});
        for(const auto& file:std::filesystem::directory_iterator(root/"ecocycle/models"))
            std::filesystem::copy_file(file.path(),scratch/"ecocycle/models"/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
        std::filesystem::create_directories(scratch/"shared/models");
        for(const auto& f:std::filesystem::directory_iterator(root/"shared/models"))
            std::filesystem::copy_file(f.path(),scratch/"shared/models"/f.path().filename(),std::filesystem::copy_options::overwrite_existing);
        { ModuleSession retry(makeEcoCycle(),hb);require(broken.placements().size()==42,"Retry after load failure"); }
        require(broken.placements().empty(),"Retry cleanup");
        std::cout<<"ECOCYCLE CPU PASS: 24 distinct models (also distinct from plaza/EcoEnergy/AquaBio); "<<triangles
                 <<" source triangles; 38 instances; 7 interactions; 4S/2K/2P; 30/60/144 Hz and single step; pause/restart; sharing; cleanup; rollback/retry\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
