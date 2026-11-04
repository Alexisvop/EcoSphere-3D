#include "AquaBioSmoke.hpp"
#include "modules/AquaBio/AquaBioSettings.hpp"
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
    for(float d:depth)if(!std::isfinite(d)||d<0||d>1)throw std::runtime_error("Invalid AquaBio depth");
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("AquaBio readback failed");
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
void checkAquaBioFramebuffer(const std::filesystem::path& directory,int width,int height,
    ContentRegistry& registry,Camera& camera,const LightingSettings& lighting,double loadSeconds,
    const PlazaRender& render,const std::function<bool()>& present) {
    for(const auto* name:{"assets","interactions","animations","panels","tour"})std::filesystem::create_directories(directory/name);
    const auto placements=registry.placements();std::map<std::string,ContentPlacement> unique;
    std::map<std::string,unsigned> counts;std::set<const Model*> resources;
    std::size_t sourceTriangles=0,sceneTriangles=0,geometries=0,cpuBytes=0;
    for(const auto& p:placements) {
        unique.emplace(p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:8),p);counts[p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:8)]++;resources.insert(p.resource.get());sceneTriangles+=triangles(*p.model);
        if(p.model!=p.resource)throw std::runtime_error("AquaBio unexpectedly duplicated visual resources");
    }
    if(placements.size()!=55 || unique.size()!=31 || resources.size()!=31)throw std::runtime_error("AquaBio count mismatch");
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
        if(count<30)throw std::runtime_error("AquaBio missing visible model: "+id);
        usage<<id<<','<<p.id<<','<<counts[id]<<','<<triangles(*p.model)<<','<<p.model->meshes.size()<<",0,"<<count<<'\n';
        std::cout<<"AQUABIO MODEL PASS "<<id<<" pixels="<<count<<'\n';
    }
    usage.close();
    struct View {const char* name;glm::vec3 position,target;};
    const std::array<View,10> stations{{
        {"overview",{35,27,39},{0,0,-2}}, {"overview-equivalent",{23,21,26},{0,0,-3}},
        {"entry-equivalent",{0,1.7f,6},{0,1.5f,-7}},
        {"rain-capture",{-12,1.7f,13},{-18,1.6f,6}},
        {"treatment",{-11,5,-9},{-12,1.3f,-18}},
        {"treatment-visitor",{-12,1.7f,-11.2f},{-17,1.2f,-16}},
        {"sampling",{12,1.7f,-2.5f},{15,1.3f,-7}},
        {"water-cycle",{1,1.7f,-16},{2,1.8f,-22}},
        {"wetland",{8.3f,1.7f,9},{14,1,4}},
        {"biodiversity",{12.3f,1.7f,11},{18,1,14.5f}}
    }};
    for(const auto& s:stations){lookAt(camera,s.position,s.target);render(lighting,true,{});shot(s.name,0);}
    camera=ZoneSettings{}.entries[2].make();render(lighting,true,{});shot("entry",0);
    struct PanelView {int model;glm::vec3 center;};
    const std::array<PanelView,11> panels{{
        {4,{-3.6f,1.5f,.5f}}, {6,{-3.2f,1.5f,0}}, {8,{-3,1.5f,-.2f}},
        {10,{-4,1.5f,0}}, {12,{-2.5f,1.5f,0}}, {13,{-2.5f,1.5f,0}},
        {15,{2.8f,1.5f,-.4f}}, {16,{-3.5f,1.5f,0}}, {20,{3.1f,1.5f,0}},
        {22,{0,1.5f,.8f}}, {23,{-5.9f,1.5f,-1.8f}}
    }};
    for(const auto& p:panels) {
        const auto target=aquabio::settings.positions[p.model-1]+p.center;
        auto observer=target+glm::vec3(0,0,4.5f);observer.y=1.7f;
        lookAt(camera,observer,target);render(lighting,true,{});
        shot("panels/AQUA-"+std::string(p.model<10?"00":"0")+std::to_string(p.model),0);
    }
    lookAt(camera,stations[0].position,stations[0].target);
    auto direct=lighting;direct.environment.reflectionEnabled=false;render(direct,true,{});const auto d=shot("direct-only",0);
    auto environment=lighting;environment.directEnabled=false;render(environment,true,{});const auto e=shot("environment-only",0);
    if(changed(d,e)<200)throw std::runtime_error("AquaBio material/light comparison missing");
    auto& inter=registry.interactions("aquabio");
    const std::array<const char*,7> interactive{{"AQUA-006-cistern","AQUA-009-demo","AQUA-013-carbon","AQUA-014-demo","AQUA-018-manual","AQUA-020-sampling","AQUA-028-demo"}};
    if(inter.objects().size()!=7)throw std::runtime_error("AquaBio interaction count");
    std::ofstream interactionCsv(directory/"interaction-states.csv");interactionCsv.exceptions(std::ios::badbit|std::ios::failbit);header(interactionCsv);
    for(std::size_t i=0;i<interactive.size();++i) {
        const std::string id=interactive[i],code="INT-AQUA-00"+std::to_string(i+1);
        inter.deselect();const auto& box=inter.object(id).bounds;
        const auto center=glm::vec3(inter.world(id)*glm::vec4((box.minimum+box.maximum)*.5f,1));
        const float radius=glm::length(box.maximum-box.minimum)*.5f;
        auto observer=center+glm::vec3(.4f,0,1)*std::max(1.3f,radius*2.5f);observer.y=1.7f;
        lookAt(camera,observer,center);
        render(lighting,true,{});const auto before=shot("interactions/"+code+"-before",0);
        states(interactionCsv,code+"_before",0,registry.placements());
        const auto ray=cameraRay(camera.view(),*camera.projection(width,height),{0,0});
        if(inter.pick(ray)!=std::optional<std::string>(id))throw std::runtime_error("AquaBio ray-pick failed: "+id);
        render(lighting,true,{});const auto selected=shot("interactions/"+code+"-selected",0);
        if(changed(before,selected)<25)throw std::runtime_error("AquaBio highlight invisible: "+id);
        inter.manipulate({.35f,.24f,-.10f},glm::angleAxis(glm::radians(23.0f),glm::vec3(0,1,0)),1.12f);
        inter.take(ray,glm::length(center-ray.origin));inter.follow(Ray{ray.origin+glm::vec3(.25f,.15f,0),ray.direction});inter.drop();
        render(lighting,true,{});const auto manipulated=shot("interactions/"+code+"-manipulated",0);
        if(changed(selected,manipulated)<60)throw std::runtime_error("AquaBio manipulation invisible: "+id);
        states(interactionCsv,code+"_manipulated",0,registry.placements());
        inter.object(id).manipulation=Pose{};inter.deselect();
        std::cout<<"AQUABIO INTERACTION PASS "<<code<<" select/TRS/take/follow/drop\n";
    }
    interactionCsv.close();
    const std::array<const char*,8> animated{{"AQUA-009-pump","AQUA-011-scraper","AQUA-019-indicator","AQUA-021-cycle","AQUA-005-adjustable","AQUA-014-extraction","AQUA-024-floating","AQUA-025-sway"}};
    std::ofstream animationCsv(directory/"animation-states.csv");animationCsv.exceptions(std::ios::badbit|std::ios::failbit);header(animationCsv);states(animationCsv,"t0",0,registry.placements());
    std::array<std::vector<unsigned char>,8> initial;
    std::array<Camera,8> cameras;
    for(std::size_t i=0;i<animated.size();++i) {
        const auto p=std::find_if(placements.begin(),placements.end(),[&](const auto& p){return p.id==animated[i];});
        if(p==placements.end())throw std::runtime_error("Animated instance missing");
        const auto [center,radius]=bounds(*p);
        lookAt(camera,center+glm::normalize(glm::vec3(.4f,.7f,2))*std::max(.4f,radius)*3.6f,center);cameras[i]=camera;
        render(lighting,true,p->id);initial[i]=shot("animations/ANIM-AQUA-00"+std::to_string(i+1)+"-t0",0);
    }
    registry.update(2);states(animationCsv,"t2",2,registry.placements());
    for(std::size_t i=0;i<animated.size();++i) {
        camera=cameras[i];render(lighting,true,std::string(animated[i]));
        const auto pixels=shot("animations/ANIM-AQUA-00"+std::to_string(i+1)+"-t1",2);
        const auto count=changed(initial[i],pixels);
        if(count<40)throw std::runtime_error("AquaBio motion invisible: "+std::string(animated[i]));
        std::cout<<"AQUABIO ANIMATION PASS "<<animated[i]<<" pixels="<<count<<'\n';
    }
    animationCsv.close();views.close();
    // Medición de throughput presentada: simulación fija; las capturas quedan fuera.
    const std::array<glm::vec3,8> route{{{0,1.7f,13},{-11.5f,1.7f,2.7f},{-11.5f,1.7f,-8},{0,1.7f,-10},{0,1.7f,-18},{0,1.7f,4},{8.3f,1.7f,8},{12.3f,1.7f,11}}};
    const std::array<glm::vec3,8> targets{{{-5,1,9},{-17,1.5f,7},{-16,1.2f,-7},{-13,1.5f,-18},{2,1.8f,-22},{13,1,-4},{15,1,4},{18,1,14.5f}}};
    const auto move=[&](int frame) {
        const float p=static_cast<float>(frame)/599*7;
        const auto i=std::min(static_cast<std::size_t>(p),std::size_t{6});const float t=p-static_cast<float>(i);
        lookAt(camera,glm::mix(route[i],route[i+1],t),glm::mix(targets[i],targets[i+1],t));
    };
    for(int i=0;i<60;++i){registry.update(1.0/60);render(lighting,true,{});if(present())throw std::runtime_error("AquaBio warmup interrupted");}
    std::ofstream frames(directory/"frames.csv");frames.exceptions(std::ios::badbit|std::ios::failbit);frames<<"frame,seconds,frame_seconds\n";
    std::vector<double> durations;glFinish();const double start=glfwGetTime();
    for(int i=0;i<600;++i) {
        const double frameStart=glfwGetTime();move(i);registry.update(1.0/60);render(lighting,true,{});
        if(present())throw std::runtime_error("AquaBio benchmark interrupted");
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
        if(present())throw std::runtime_error("AquaBio recording interrupted");
    }
    const double recordingSeconds=glfwGetTime()-recordingStart;tourCameras.close();
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("AquaBio GL error");
    std::sort(durations.begin(),durations.end());
    std::ofstream metrics(directory/"metrics.csv");metrics.exceptions(std::ios::badbit|std::ios::failbit);
    metrics<<std::setprecision(9)<<"metric,value\nunique_models,28\nmodel_resources,28\ninstances,52\ninteractive_objects,7\nanimated_entities,8\nsource_triangles,"<<sourceTriangles
            <<"\nscene_triangles,"<<sceneTriangles<<"\nsource_geometries,"<<geometries<<"\nsource_textures,0\nwidth,"<<width<<"\nheight,"<<height
            <<"\ncpu_geometry_bytes_lower_bound,"<<cpuBytes<<"\ncpu_load_seconds,"<<loadSeconds<<"\nframes,600\nseconds,"<<seconds<<"\nfps_mean,"<<600/seconds
            <<"\nframe_p50_ms,"<<durations[299]*1000<<"\nframe_p95_ms,"<<durations[569]*1000<<"\nframe_p99_ms,"<<durations[593]*1000<<"\nrecording_seconds,"<<recordingSeconds<<'\n';
#if defined(__linux__)
    std::ifstream status("/proc/self/status");std::string line;
    while(std::getline(status,line))if(line.rfind("VmRSS:",0)==0)metrics<<"process_rss_kib,"<<std::stoull(line.substr(6))<<'\n';
#endif
    metrics.close();
    std::cout<<"AQUABIO SMOKE PASS: 28 models; 52 instances; 7 interactions; 8 animations; frames=600 seconds="<<seconds<<" fps_mean="<<600/seconds<<" framebuffer="<<width<<'x'<<height<<'\n';
}
}
