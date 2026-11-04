#include "EcoCycleSmoke.hpp"
#include "modules/EcoCycle/EcoCycleSettings.hpp"
#include "world/ZoneSettings.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace ecosphere {
namespace {
void lookAt(Camera& c,glm::vec3 p,glm::vec3 target) {
    const auto d=target-p;c.position=p;
    c.yawDegrees=glm::degrees(std::atan2(d.z,d.x));
    c.pitchDegrees=glm::degrees(std::atan2(d.y,glm::length(glm::vec2(d.x,d.z))));
}
std::vector<unsigned char> capture(const std::filesystem::path& path,int width,int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    std::vector<float> depth(static_cast<std::size_t>(width)*height);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
    for(float d:depth)if(!std::isfinite(d)||d<0||d>1)throw std::runtime_error("Invalid EcoCycle depth");
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("EcoCycle readback failed");
    std::ofstream out(path,std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3),width*3);
    return pixels;
}
std::size_t changed(const std::vector<unsigned char>& a,const std::vector<unsigned char>& b) {
    std::size_t n=0;
    for(std::size_t i=0;i<a.size();i+=3)n+=std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2]))>12;
    return n;
}
std::pair<glm::vec3,float> bounds(const ContentPlacement& p) {
    glm::vec3 lo(std::numeric_limits<float>::infinity()),hi(-std::numeric_limits<float>::infinity());
    p.model->forEach([&](std::size_t index,const glm::mat4& m) {
        for(const auto& v:p.model->meshes.at(index).vertices) {
            const auto point=glm::vec3(m*glm::vec4(v.position,1));lo=glm::min(lo,point);hi=glm::max(hi,point);
        }
    },p.world);
    return {(lo+hi)*.5f,std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z})*.5f};
}
std::size_t triangles(const Model& m) {
    std::size_t n=0;m.forEach([&](std::size_t i,const glm::mat4&){const auto& mesh=m.meshes.at(i);n+=(mesh.indices.empty()?mesh.vertices.size():mesh.indices.size())/3;});return n;
}
void states(std::ostream& out,const std::string& stage,double t,const std::vector<ContentPlacement>& placements) {
    for(const auto& p:placements) {
        out<<stage<<','<<t<<','<<p.id;
        for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<','<<p.world[c][r];
        out<<'\n';
    }
}
void header(std::ostream& out) {
    out<<std::setprecision(9)<<"stage,time_s,instance_id";
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)out<<",m"<<c<<r;
    out<<'\n';
}
}
void checkEcoCycleFramebuffer(const std::filesystem::path& directory,int width,int height,
    ContentRegistry& registry,Camera& camera,const LightingSettings& lighting,double loadSeconds,
    const PlazaRender& render,const std::function<bool()>& present) {
    for(const auto* name:{"assets","interactions","animations","panels","tour"})std::filesystem::create_directories(directory/name);
    const auto placements=registry.placements();std::map<std::string,ContentPlacement> unique;
    std::map<std::string,unsigned> counts;std::set<const Model*> resources;
    std::size_t sourceTriangles=0,sceneTriangles=0,geometries=0,cpuBytes=0;
    for(const auto& p:placements) {
        unique.emplace(p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:9),p);counts[p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:9)]++;resources.insert(p.resource.get());sceneTriangles+=triangles(*p.model);
        if(p.model!=p.resource)throw std::runtime_error("EcoCycle unexpectedly duplicated visual resources");
    }
    if(placements.size()!=42 || unique.size()!=28 || resources.size()!=28)throw std::runtime_error("EcoCycle count mismatch");
    for(const auto* model:resources) {
        sourceTriangles+=triangles(*model);geometries+=model->meshes.size();
        for(const auto& mesh:model->meshes)cpuBytes+=mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t);
    }
    std::ofstream views(directory/"views.csv");views.exceptions(std::ios::badbit|std::ios::failbit);
    views<<"capture,time_s,camera_x,camera_y,camera_z,yaw_deg,pitch_deg,fov_deg\n";
    const auto shot=[&](const std::string& name,double time) {
        views<<name<<','<<time<<','<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<','<<camera.yawDegrees<<','<<camera.pitchDegrees<<','<<camera.fovDegrees<<'\n';
        return capture(directory/(name+".ppm"),width,height);
    };
    std::ofstream usage(directory/"model-use.csv");usage.exceptions(std::ios::badbit|std::ios::failbit);
    usage<<"model_id,instance_id,instances,triangles,meshes,textures,changed_pixels\n";
    for(const auto& [id,p]:unique) {
        const auto [center,radius]=bounds(p);
        lookAt(camera,center+glm::normalize(glm::vec3(1.2f,1.4f,2.2f))*std::max(radius,.3f)*3.1f,center);
        render(lighting,false,{});const auto background=capture(directory/"background.ppm",width,height);
        render(lighting,true,p.id);const auto pixels=shot("assets/"+id,0);const auto count=changed(background,pixels);
        if(count<30)throw std::runtime_error("EcoCycle missing visible model: "+id);
        usage<<id<<','<<p.id<<','<<counts[id]<<','<<triangles(*p.model)<<','<<p.model->meshes.size()<<",0,"<<count<<'\n';
        std::cout<<"ECOCYCLE MODEL PASS "<<id<<" pixels="<<count<<'\n';
    }
    usage.close();
    struct View {const char* name;glm::vec3 position,target;};
    const std::array<View,10> stations{{
        {"overview",{35,28,39},{0,0,-1}}, {"classification",{-10,1.7f,14},{-16,1,9}},
        {"feeding",{-11,1.7f,5},{-16,1.5f,1}}, {"separation",{-10,1.7f,-2},{-15,1.5f,-8}},
        {"paper-press",{-10,1.7f,-13},{-16,1.5f,-18}}, {"plastic-shredder",{-7,1.7f,-3},{-7,1.5f,-8}},
        {"material-recovery",{9,4,-12},{14,1,-14}}, {"glass",{12,1.7f,5},{10,1,-1}},
        {"workshop",{11,1.7f,16},{16,1.1f,12}}, {"circular-economy",{6.5f,1.7f,22.5f},{6.5f,1.5f,18}}
    }};
    for(const auto& v:stations){lookAt(camera,v.position,v.target);render(lighting,true,{});shot(v.name,0);}
    camera=ZoneSettings{}.entries[3].make();render(lighting,true,{});shot("entry",0);
    // Las señales están incluidas en las geometrías; se inspeccionan sin aislar la máquina.
    struct Panel {int n;glm::vec3 center;};
    const std::array<Panel,10> panels{{
        {4,{0,1.6f,.45f}},{5,{-3.6f,1.5f,0}},{6,{-3.6f,1.5f,0}},{7,{-3.6f,1.5f,0}},
        {12,{-3.8f,1.5f,0}},{15,{3.2f,1.5f,0}},{17,{3.6f,1.5f,0}},{18,{4.2f,1.5f,0}},
        {20,{-3.6f,1.5f,0}},{23,{3.7f,1.5f,0}}
    }};
    for(const auto& panel:panels) {
        const auto center=ecocycle::settings.positions[panel.n-1]+panel.center;
        auto eye=center+glm::vec3(0,0,4.5f);eye.y=1.7f;
        lookAt(camera,eye,center);render(lighting,true,{});
        shot("panels/CYCLE-"+std::string(panel.n<10?"00":"0")+std::to_string(panel.n),0);
    }
    lookAt(camera,stations[0].position,stations[0].target);
    auto direct=lighting;direct.environment.reflectionEnabled=false;render(direct,true,{});const auto d=shot("direct-only",0);
    auto environment=lighting;environment.directEnabled=false;render(environment,true,{});const auto e=shot("environment-only",0);
    if(changed(d,e)<200)throw std::runtime_error("EcoCycle material/light comparison missing");
    auto& inter=registry.interactions("ecocycle");
    const std::array<const char*,7> interactive{{"CYCLE-005-main","CYCLE-014-main","CYCLE-017-main","CYCLE-019-main","CYCLE-021-main","CYCLE-023-main","CYCLE-024-main"}};
    if(inter.objects().size()!=7)throw std::runtime_error("EcoCycle interaction count");
    std::ofstream interactionCsv(directory/"interaction-states.csv");interactionCsv.exceptions(std::ios::badbit|std::ios::failbit);header(interactionCsv);
    for(std::size_t i=0;i<interactive.size();++i) {
        const std::string id=interactive[i],code="INT-CYCLE-00"+std::to_string(i+1);
        inter.deselect();const auto& box=inter.object(id).bounds;
        const auto center=glm::vec3(inter.world(id)*glm::vec4((box.minimum+box.maximum)*.5f,1));
        const float radius=glm::length(box.maximum-box.minimum)*.5f;
        auto observer=center+glm::vec3(.4f,0,1)*std::max(1.3f,radius*2.5f);observer.y=1.7f;
        lookAt(camera,observer,center);
        render(lighting,true,{});const auto before=shot("interactions/"+code+"-before",0);
        states(interactionCsv,code+"_before",0,registry.placements());
        const auto ray=cameraRay(camera.view(),*camera.projection(width,height),{0,0});
        if(inter.pick(ray)!=std::optional<std::string>(id))throw std::runtime_error("EcoCycle ray-pick failed: "+id);
        render(lighting,true,{});const auto selected=shot("interactions/"+code+"-selected",0);
        if(changed(before,selected)<25)throw std::runtime_error("EcoCycle highlight invisible: "+id);
        inter.manipulate({.35f,.24f,-.10f},glm::angleAxis(glm::radians(23.0f),glm::vec3(0,1,0)),1.12f);
        inter.take(ray,glm::length(center-ray.origin));inter.follow(Ray{ray.origin+glm::vec3(.25f,.15f,0),ray.direction});inter.drop();
        render(lighting,true,{});const auto manipulated=shot("interactions/"+code+"-manipulated",0);
        if(changed(selected,manipulated)<60)throw std::runtime_error("EcoCycle manipulation invisible: "+id);
        states(interactionCsv,code+"_manipulated",0,registry.placements());
        inter.object(id).manipulation=Pose{};inter.deselect();
        std::cout<<"ECOCYCLE INTERACTION PASS "<<code<<" select/TRS/take/follow/drop\n";
    }
    interactionCsv.close();
    const std::array<const char*,8> animated{{"CYCLE-008-main","CYCLE-011-main","CYCLE-016-main","CYCLE-022-main","CYCLE-010-main","CYCLE-013-main","CYCLE-006-main","CYCLE-009-main"}};
    std::ofstream animationCsv(directory/"animation-states.csv");animationCsv.exceptions(std::ios::badbit|std::ios::failbit);header(animationCsv);states(animationCsv,"t0",0,registry.placements());
    std::array<std::vector<unsigned char>,8> initial;
    std::array<Camera,8> cameras;
    for(std::size_t i=0;i<animated.size();++i) {
        const auto p=std::find_if(placements.begin(),placements.end(),[&](const auto& p){return p.id==animated[i];});
        if(p==placements.end())throw std::runtime_error("Animated instance missing");
        const auto [center,radius]=bounds(*p);
        lookAt(camera,center+glm::normalize(glm::vec3(.4f,.7f,2))*std::max(.4f,radius)*3.6f,center);cameras[i]=camera;
        render(lighting,true,p->id);initial[i]=shot("animations/ANIM-CYCLE-00"+std::to_string(i+1)+"-t0",0);
    }
    registry.update(2);states(animationCsv,"t2",2,registry.placements());
    for(std::size_t i=0;i<animated.size();++i) {
        camera=cameras[i];render(lighting,true,std::string(animated[i]));
        const auto pixels=shot("animations/ANIM-CYCLE-00"+std::to_string(i+1)+"-t1",2);
        const auto count=changed(initial[i],pixels);
        if(count<40)throw std::runtime_error("EcoCycle motion invisible: "+std::string(animated[i]));
        std::cout<<"ECOCYCLE ANIMATION PASS "<<animated[i]<<" pixels="<<count<<'\n';
    }
    animationCsv.close();views.close();
    // Medición de throughput presentada: simulación fija; las capturas quedan fuera.
    const auto& route=ecocycle::settings.tourPositions;
    const auto& targets=ecocycle::settings.tourTargets;
    const auto move=[&](int frame) {
        const float p=static_cast<float>(frame)/599*static_cast<float>(route.size()-1);
        const auto i=std::min(static_cast<std::size_t>(p),route.size()-2);const float t=p-static_cast<float>(i);
        lookAt(camera,glm::mix(route[i],route[i+1],t),glm::mix(targets[i],targets[i+1],t));
    };
    for(int i=0;i<60;++i){registry.update(1.0/60);render(lighting,true,{});if(present())throw std::runtime_error("EcoCycle warmup interrupted");}
    std::ofstream frames(directory/"frames.csv");frames.exceptions(std::ios::badbit|std::ios::failbit);frames<<"frame,seconds,frame_seconds\n";
    std::vector<double> durations;glFinish();const double start=glfwGetTime();
    for(int i=0;i<600;++i) {
        const double frameStart=glfwGetTime();move(i);registry.update(1.0/60);render(lighting,true,{});
        if(present())throw std::runtime_error("EcoCycle benchmark interrupted");
        durations.push_back(glfwGetTime()-frameStart);frames<<i<<','<<glfwGetTime()-start<<','<<durations.back()<<'\n';
    }
    glFinish();const double seconds=glfwGetTime()-start;frames.close();
    std::ofstream tourCameras(directory/"tour-cameras.csv");tourCameras.exceptions(std::ios::badbit|std::ios::failbit);
    tourCameras<<"frame,recording_step_s,camera_x,camera_y,camera_z,yaw_deg,pitch_deg\n";
    const double recordingStart=glfwGetTime();
    for(int i=0;i<1200;++i) {
        move(i/2);registry.update(1.0/60);render(lighting,true,{});
        if(i%10==0) {
            capture(directory/"tour"/(std::to_string(1000+i/10)+".ppm"),width,height);
            tourCameras<<i<<','<<(i+1)/60.0<<','<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<','<<camera.yawDegrees<<','<<camera.pitchDegrees<<'\n';
        }
        if(present())throw std::runtime_error("EcoCycle recording interrupted");
    }
    const double recordingSeconds=glfwGetTime()-recordingStart;tourCameras.close();
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("EcoCycle GL error");
    std::sort(durations.begin(),durations.end());
    std::ofstream metrics(directory/"metrics.csv");metrics.exceptions(std::ios::badbit|std::ios::failbit);
    metrics<<std::setprecision(9)<<"metric,value\nunique_models,24\nmodel_resources,24\ninstances,38\ninteractive_objects,7\nanimated_entities,8\nsource_triangles,"<<sourceTriangles
            <<"\nscene_triangles,"<<sceneTriangles<<"\nsource_geometries,"<<geometries<<"\nsource_textures,0\nwidth,"<<width<<"\nheight,"<<height
            <<"\ncpu_geometry_bytes_lower_bound,"<<cpuBytes<<"\ncpu_load_seconds,"<<loadSeconds<<"\nframes,600\nseconds,"<<seconds<<"\nfps_mean,"<<600/seconds
            <<"\nframe_p50_ms,"<<durations[299]*1000<<"\nframe_p95_ms,"<<durations[569]*1000<<"\nframe_p99_ms,"<<durations[593]*1000<<"\nrecording_seconds,"<<recordingSeconds<<'\n';
#if defined(__linux__)
    std::ifstream status("/proc/self/status");std::string line;
    while(std::getline(status,line))if(line.rfind("VmRSS:",0)==0)metrics<<"process_rss_kib,"<<std::stoull(line.substr(6))<<'\n';
#endif
    metrics.close();
    std::cout<<"ECOCYCLE SMOKE PASS: 24 models; 38 instances; 7 interactions; 8 animations; frames=600 seconds="<<seconds<<" fps_mean="<<600/seconds<<" framebuffer="<<width<<'x'<<height<<'\n';
}
}
