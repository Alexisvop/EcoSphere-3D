#include "NavigationSmoke.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <limits>

namespace ecosphere {
namespace {
void require(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
std::vector<unsigned char> readRgb(int w,int h) {
    std::vector<unsigned char> rgb(static_cast<std::size_t>(w)*h*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    require(glGetError()==GL_NO_ERROR,"Navigation framebuffer capture failed");
    return rgb;
}
void capture(const std::filesystem::path& path,int w,int h) {
    const auto rgb=readRgb(w,h);
    std::ofstream out(path,std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+static_cast<std::size_t>(y)*w*3),w*3);
}
std::size_t rss() {
    std::ifstream in("/proc/self/status");std::string key,line;std::size_t value=0;
    while(in>>key) {if(key=="VmRSS:"){in>>value;return value;}std::getline(in,line);}
    return 0; // Sin /proc se informa no medido, no cero memoria utilizada.
}
void look(Camera& camera,glm::vec3 position,glm::vec3 target) {
    const auto d=target-position;camera.position=position;
    camera.yawDegrees=glm::degrees(std::atan2(d.z,d.x));
    camera.pitchDegrees=glm::degrees(std::atan2(d.y,glm::length(glm::vec2(d.x,d.z))));
}
}
void checkNavigationFramebuffer(const std::filesystem::path& directory,int width,int height,
    ZoneNavigator& nav,Camera& camera,const std::function<bool(glm::vec3,bool,bool)>& navigate,
    const std::function<void()>& render,const std::function<void()>& present,
    const std::function<void(bool,const std::string&)>& renderContent,bool integral,int swapInterval) {
    std::filesystem::create_directories(directory/"tour");
    std::filesystem::create_directories(directory/"shared");
    const ZoneSettings settings;
    const auto frame=[&] {nav.update(1.0/60);render();present();require(glGetError()==GL_NO_ERROR,"Navigation GL error");};
    const auto shot=[&](const std::string& name) {render();capture(directory/(name+".ppm"),width,height);present();};
    std::ofstream transitions(directory/"transitions.csv");transitions.exceptions(std::ios::badbit|std::ios::failbit);
    transitions<<"index,from,to,seconds,entities,resources,meshes,textures,rss_kib,expired_outgoing_resources,reused_shared_references\n";
    unsigned index=0;
    const auto change=[&](const Portal& portal,bool home) {
        navigate(camera.position,false,false);
        const auto from=nav.active();std::vector<std::weak_ptr<const Model>> outgoing;
        for(const auto& p:nav.placements())outgoing.emplace_back(p.resource);
        glFinish();const auto start=glfwGetTime();
        require(navigate(home ? camera.position : portal.center,!home,home),"Navigation transition failed");
        glFinish();const auto seconds=glfwGetTime()-start;
        for(int i=0;i<3;++i)frame();
        std::set<const Model*> resources,incoming;std::size_t meshes=0,textures=0,expired=0,reused=0;
        for(const auto& p:nav.placements())incoming.insert(p.resource.get());
        for(const auto& p:nav.placements()) if(resources.insert(p.model.get()).second) {meshes+=p.model->meshes.size();textures+=p.model->textures.size();}
        for(const auto& p:outgoing) {
            if(p.expired())++expired;
            else {require(incoming.count(p.lock().get())==1,"Outgoing resource leaked without an incoming owner");++reused;}
        }
        require(expired+reused==outgoing.size(),"Outgoing/shared reference accounting mismatch");
        transitions<<index++<<','<<zoneId(from)<<','<<zoneId(nav.active())<<','<<seconds<<','<<nav.placements().size()<<','
            <<resources.size()<<','<<meshes<<','<<textures<<','<<rss()<<','<<expired<<','<<reused<<'\n';
        navigate(camera.position,false,false);
    };
    std::ofstream fps(directory/"fps.csv");fps.exceptions(std::ios::badbit|std::ios::failbit);
    fps<<"zone,width,height,swap_interval,frames,seconds,fps_mean,frame_mean_ms,frame_p95_ms,rss_kib\n";
    const auto benchmark=[&] {
        for(int i=0;i<30;++i)frame();
        std::vector<double> times;
        glFinish();const auto start=glfwGetTime();
        for(int i=0;i<300;++i){const auto t=glfwGetTime();frame();times.push_back(glfwGetTime()-t);}
        glFinish();const auto duration=glfwGetTime()-start;std::sort(times.begin(),times.end());
        fps<<zoneId(nav.active())<<','<<width<<','<<height<<','<<swapInterval<<",300,"<<duration<<','<<300/duration<<','
            <<std::accumulate(times.begin(),times.end(),0.0)/times.size()*1000<<','<<times.at(284)*1000<<','<<rss()<<'\n';
    };
    std::ofstream sharedViews(directory/"shared-views.csv");
    sharedViews.exceptions(std::ios::badbit|std::ios::failbit);
    sharedViews<<"owner,instance_id,model_id,capture,camera_x,camera_y,camera_z,target_x,target_y,target_z,contribution_pixels\n";
    const auto sharedShots=[&] {
        const auto saved=camera;
        for(const auto& p:nav.placements())if(p.id.rfind("SHARED-",0)==0) {
            glm::vec3 lo(std::numeric_limits<float>::infinity()),hi=-lo;
            p.model->forEach([&](std::size_t i,const glm::mat4& world){for(const auto& v:p.model->meshes.at(i).vertices){const auto point=glm::vec3(world*glm::vec4(v.position,1));lo=glm::min(lo,point);hi=glm::max(hi,point);}},p.world);
            const auto center=(lo+hi)*.5f;const glm::vec3 observer{center.x,1.7f,center.z+3};
            look(camera,observer,center);
            renderContent(false,{});const auto background=readRgb(width,height);
            renderContent(true,p.id);const auto isolated=readRgb(width,height);std::size_t changed=0;
            for(std::size_t i=0;i<isolated.size();i+=3)
                changed+=std::abs(int(isolated[i])-int(background[i]))+std::abs(int(isolated[i+1])-int(background[i+1]))+std::abs(int(isolated[i+2])-int(background[i+2]))>12;
            require(changed>100,"Shared model made no framebuffer contribution");
            const auto name="shared/"+p.owner+"-"+p.id.substr(0,10);shot(name);
            sharedViews<<p.owner<<','<<p.id<<','<<p.id.substr(0,10)<<','<<name<<','<<observer.x<<','<<observer.y<<','<<observer.z<<','<<center.x<<','<<center.y<<','<<center.z<<','<<changed<<'\n';
        }
        camera=saved;
    };
    shot("hub-entry");benchmark();sharedShots();
    look(camera,{24,24,29},{0,0,0});shot("hub-overview");
    look(camera,{-2.8f,1.7f,14.7f},{-2.8f,2.1f,11.2f});shot("signpost-first-person");
    if(integral) for(const auto& view:std::vector<std::pair<std::string,glm::vec3>>{
        {"left",{-5.1f,1.7f,14.5f}},{"right",{-.5f,1.7f,14.5f}},{"path",{0,1.7f,16}}}) {
        look(camera,view.second,{-2.8f,2.05f,11.2f});shot("signpost-"+view.first);
    }
    const auto panel=plaza::settings.panels[0];
    look(camera,panel+glm::vec3(0,1.7f,3.5f),panel+glm::vec3(0,1.65f,0));shot("panel-first-person");
    look(camera,{3.8f,1.7f,13},{3.8f,1.95f,9.5f});shot("kiosk-first-person");
    for(const auto& p:settings.portals) {
        const auto name=std::string(zoneId(p.destination));
        look(camera,p.returnCamera.make().position,{p.center.x,2.5f,p.center.z});shot(name+"-access");
        camera.position=p.center;shot(name+"-prompt");
        change(p,false);shot(name+"-inside");benchmark();sharedShots();
        change(p,true);shot(name+"-return");
    }
    std::ofstream endurance(directory/"endurance.csv");endurance.exceptions(std::ios::badbit|std::ios::failbit);
    endurance<<"cycle,transitions,rss_kib\n0,"<<index<<','<<rss()<<'\n';
    const int cycles=integral ? 100 : 30;
    for(int cycle=0;cycle<cycles;++cycle) {
        const auto& p=settings.portals.at(static_cast<std::size_t>(cycle)%3);
        change(p,false);change(p,true);
        endurance<<cycle+1<<','<<index<<','<<rss()<<'\n';
    }
    shot(integral ? "hub-after-103-visits" : "hub-after-33-visits");
    // Segunda pasada: recorrido reproducible con transiciones reales, fuera de la medición de FPS.
    std::ofstream recording(directory/"recording.csv");recording<<"frame,seconds,zone\n";
    const auto start=glfwGetTime();unsigned sample=0;
    for(const auto& p:settings.portals) {
        const auto entry=p.returnCamera.make().position;
        for(int i=0;i<60;++i) {
            look(camera,glm::mix(entry,p.center,float(i)/59),{p.center.x,2.4f,p.center.z});render();
            if(i%5==0){capture(directory/"tour"/(std::to_string(1000+sample)+".ppm"),width,height);recording<<sample++<<','<<glfwGetTime()-start<<','<<zoneId(nav.active())<<'\n';}
            present();
        }
        change(p,false);
        for(int i=0;i<60;++i) {
            frame();
            if(i%5==0){render();capture(directory/"tour"/(std::to_string(1000+sample)+".ppm"),width,height);recording<<sample++<<','<<glfwGetTime()-start<<','<<zoneId(nav.active())<<'\n';}
        }
        change(p,true);
    }
    std::cout<<"NAVIGATION GPU PASS: 4 zones, "<<index<<" transitions, exclusive resources released/shared incoming references retained, 12 shared-model framebuffer views, 1280x720 requested; actual "<<width<<'x'<<height<<std::endl;
}
}
