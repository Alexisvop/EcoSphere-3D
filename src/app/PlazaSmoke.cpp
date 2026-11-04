#include "PlazaSmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>

namespace ecosphere {
namespace {
void lookAt(Camera& camera,glm::vec3 position,glm::vec3 target) {
    const auto d=target-position;
    camera.position=position;
    camera.yawDegrees=glm::degrees(std::atan2(d.z,d.x));
    camera.pitchDegrees=glm::degrees(std::atan2(d.y,glm::length(glm::vec2(d.x,d.z))));
}
std::vector<unsigned char> capture(const std::filesystem::path& path,int width,int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::vector<float> depth(static_cast<std::size_t>(width)*height);
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
    for (float d:depth) if (!std::isfinite(d) || d<0 || d>1) throw std::runtime_error("Invalid plaza depth");
    if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Plaza framebuffer readback failed");
    std::ofstream file(path,std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);
    file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for (int y=height-1;y>=0;--y) file.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3),width*3);
    return pixels;
}
std::size_t changed(const std::vector<unsigned char>& a,const std::vector<unsigned char>& b) {
    std::size_t n=0;
    for (std::size_t i=0;i<a.size();i+=3)
        n+=std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2]))>12;
    return n;
}
std::size_t triangles(const Model& model) {
    std::size_t n=0;
    model.forEach([&](std::size_t index,const glm::mat4&) {
        const auto& m=model.meshes.at(index);n+=(m.indices.empty()?m.vertices.size():m.indices.size())/3;
    });
    return n;
}
std::size_t payload(const Model& model) {
    std::size_t n=sizeof(Model)+model.nodes.size()*sizeof(ModelNode)+model.meshes.size()*sizeof(Mesh);
    for (const auto& m:model.meshes) n+=m.vertices.size()*sizeof(Vertex)+m.indices.size()*sizeof(std::uint32_t);
    for (const auto& image:model.images) n+=image.rgba.size();
    for (const auto& node:model.nodes) n+=node.name.size()+node.meshes.size()*sizeof(std::size_t);
    return n;
}
}
void checkPlazaFramebuffer(const std::filesystem::path& directory,int width,int height,
    ContentRegistry& registry,Camera& camera,const LightingSettings& lighting,double loadSeconds,
    const PlazaRender& render,const std::function<bool()>& present) {
    std::filesystem::create_directories(directory/"assets");
    std::filesystem::create_directories(directory/"tour");
    const auto placements=registry.placements();
    std::map<std::string,ContentPlacement> unique;
    std::map<std::string,unsigned> counts;
    std::set<const Model*> resources,renderModels,cpuModels;
    std::size_t sceneTriangles=0,geometryCount=0,renderGeometryCount=0,sourceTriangles=0,textureCount=0,cpuBytes=0;
    for (const auto& p:placements) {
        const auto id=p.id.substr(0,p.id.rfind("SHARED-",0)==0?10:9);
        unique.emplace(id,p);++counts[id];sceneTriangles+=triangles(*p.model);
        if (p.id=="PLAZA-008-energy") unique[id]=p;
        resources.insert(p.resource.get());renderModels.insert(p.model.get());
        cpuModels.insert(p.resource.get());cpuModels.insert(p.model.get());
    }
    if (unique.size()!=27 || resources.size()!=29) throw std::runtime_error("Plaza requires 25 base models, two shared designs and two signage variants");
    for (const auto* model:resources) {geometryCount+=model->meshes.size();textureCount+=model->textures.size();sourceTriangles+=triangles(*model);}
    for (const auto* model:renderModels) renderGeometryCount+=model->meshes.size();
    for (const auto* model:cpuModels) cpuBytes+=payload(*model);
    std::ofstream evidence(directory/"model-use.csv");evidence.exceptions(std::ios::badbit|std::ios::failbit);
    evidence<<"model_id,instance_id,instances,triangles,meshes,textures,changed_pixels\n";
    for (const auto& item:unique) {
        const auto& p=item.second;
        glm::vec3 lo(std::numeric_limits<float>::infinity()),hi(-std::numeric_limits<float>::infinity());
        p.model->forEach([&](std::size_t mesh,const glm::mat4& world) {
            for (const auto& v:p.model->meshes.at(mesh).vertices) {
                const auto position=glm::vec3(world*glm::vec4(v.position,1));lo=glm::min(lo,position);hi=glm::max(hi,position);
            }
        },p.world);
        const auto center=(lo+hi)*.5f;
        const float radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z})*.5f;
        const auto front=glm::normalize(glm::vec3(p.world*glm::vec4(1.2f,1.4f,2.2f,0)));
        lookAt(camera,center+front*(3*std::max(radius,.5f)),center);
        render(lighting,false,{});
        // El fondo de referencia queda ignorado; las capturas individuales son modelos reales registrados.
        const auto background=capture(directory/"asset-background.ppm",width,height);
        render(lighting,true,p.id);
        const auto pixels=capture(directory/"assets"/(item.first+".ppm"),width,height);
        const auto difference=changed(background,pixels);
        if (difference<30) throw std::runtime_error("Plaza model did not contribute visible pixels: "+item.first);
        evidence<<item.first<<','<<p.id<<','<<counts.at(item.first)<<','<<triangles(*p.model)<<','
                <<p.model->meshes.size()<<','<<p.model->textures.size()<<','<<difference<<'\n';
        std::cout<<"PLAZA MODEL PASS "<<item.first<<" pixels="<<difference<<'\n';
    }
    evidence.close();
    struct View {const char* name;glm::vec3 position,target;};
    const std::array<View,7> views{{
        {"overview",{24,24,29},{0,0,0}}, {"entry",{0,1.7f,19},{0,1.8f,0}},
        {"center",{6,4,7},{0,1.9f,0}}, {"ecoenergy",{-6,1.7f,3},{-18,1.8f,0}},
        {"aquabio",{4,1.7f,-6},{0,1.8f,-18}}, {"ecocycle",{6,1.7f,3},{18,1.8f,0}},
        {"garden",{-9,2,12},{-6,.9f,7}}}};
    std::vector<unsigned char> combined;
    for (const auto& view:views) {
        lookAt(camera,view.position,view.target);render(lighting,true,{});
        const auto pixels=capture(directory/(std::string(view.name)+".ppm"),width,height);
        if (std::string(view.name)=="overview") combined=pixels;
    }
    lookAt(camera,views[0].position,views[0].target);
    auto direct=lighting;direct.environment.reflectionEnabled=false;render(direct,true,{});
    const auto directPixels=capture(directory/"direct-only.ppm",width,height);
    auto reflection=lighting;reflection.directEnabled=false;render(reflection,true,{});
    const auto reflected=capture(directory/"environment-only.ppm",width,height);
    if (changed(combined,directPixels)<30 || changed(directPixels,reflected)<100)
        throw std::runtime_error("Plaza lighting comparison missing visible difference");

    const std::array<glm::vec3,8> route{{{0,1.7f,17},{0,1.7f,10},{5.5f,1.7f,7},{7.5f,1.7f,0},
        {0,1.7f,-7.5f},{-7.5f,1.7f,0},{-5.5f,1.7f,7},{0,1.7f,10}}};
    std::ofstream frames(directory/"frames.csv");frames.exceptions(std::ios::badbit|std::ios::failbit);
    frames<<"frame,seconds,x,y,z,frame_seconds\n";
    std::vector<double> durations;
    for (int i=0;i<60;++i) {render(lighting,true,{});if(present())throw std::runtime_error("Plaza smoke interrupted");}
    const auto moveOnRoute=[&](int i) {
        const float progress=float(i)/599*(route.size()-1);
        const auto segment=std::min(static_cast<std::size_t>(progress),route.size()-2);
        lookAt(camera,glm::mix(route[segment],route[segment+1],progress-segment),{0,1.6f,0});
    };
    const double start=glfwGetTime();
    for (int i=0;i<600;++i) {
        const double frameStart=glfwGetTime();
        moveOnRoute(i);render(lighting,true,{});
        if (present()) throw std::runtime_error("Plaza smoke interrupted");
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Plaza benchmark GL error");
        const double duration=glfwGetTime()-frameStart;durations.push_back(duration);
        frames<<i<<','<<glfwGetTime()-start<<','<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<','<<duration<<'\n';
    }
    const double seconds=glfwGetTime()-start;frames.close();
    // Segunda pasada para vídeo: lectura/escritura de capturas fuera de la medición de FPS.
    const double recordingStart=glfwGetTime();
    for(int i=0;i<600;++i) {
        moveOnRoute(i);render(lighting,true,{});
        if(i%10==0)capture(directory/"tour"/(std::to_string(1000+i/10)+".ppm"),width,height);
        if(present())throw std::runtime_error("Plaza recording interrupted");
    }
    const double recordingSeconds=glfwGetTime()-recordingStart;
    std::sort(durations.begin(),durations.end());
    std::ofstream metrics(directory/"metrics.csv");metrics.exceptions(std::ios::badbit|std::ios::failbit);
    metrics<<"metric,value\nunique_models,"<<unique.size()<<"\nmodel_resources,"<<resources.size()<<"\ninstances,"<<placements.size()
           <<"\nscene_triangles,"<<sceneTriangles<<"\nsource_triangles,"<<sourceTriangles<<"\nsource_geometries,"<<geometryCount
           <<"\nrender_geometries,"<<renderGeometryCount<<"\nsource_textures,"<<textureCount
           <<"\nrender_models,"<<renderModels.size()<<"\ncpu_model_payload_bytes_lower_bound,"<<cpuBytes
           <<"\nwidth,"<<width<<"\nheight,"<<height<<"\ncpu_load_seconds,"<<loadSeconds
           <<"\nframes,600\nseconds,"<<seconds<<"\nfps_mean,"<<600/seconds
           <<"\nrecording_seconds,"<<recordingSeconds
           <<"\nframe_p95_ms,"<<durations.at(569)*1000<<"\n";
#if defined(__linux__)
    std::ifstream status("/proc/self/status");std::string line;
    while(std::getline(status,line)) if(line.rfind("VmRSS:",0)==0) metrics<<"process_rss_kib,"<<std::stoull(line.substr(6))<<'\n';
#endif
    metrics.close();
    std::cout<<"PLAZA SMOKE PASS: unique="<<resources.size()<<" instances="<<placements.size()<<" triangles="<<sceneTriangles
             <<" geometries="<<geometryCount<<" textures="<<textureCount<<" load_seconds="<<loadSeconds
             <<" frames=600 seconds="<<seconds<<" fps_mean="<<600/seconds<<" framebuffer="<<width<<'x'<<height<<std::endl;
}
}
