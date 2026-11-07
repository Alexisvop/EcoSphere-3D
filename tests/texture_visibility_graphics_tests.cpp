#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "app/SrgbFramebuffer.hpp"
#include "world/ZoneNavigator.hpp"
#include "Environment.hpp"
#include "ModelRenderer.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

using namespace ecosphere;
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct View { const char* name; int zone; glm::vec3 target; std::array<glm::vec3,3> eyes; };
const std::array<View,7> views{{
    {"wetland",2,{14,.6f,5},{{{8.3f,1.7f,9},{11,2.5f,7.8f},{13.3f,1.8f,5.9f}}}},
    {"treatment",2,{-13.5f,.65f,-13},{{{-7.5f,1.7f,-17.5f},{-10.5f,2.6f,-16.2f},{-12.6f,1.7f,-14.6f}}}},
    {"wood",2,{18.8f,1.4f,14.5f},{{{16,1.7f,19},{17.3f,1.8f,17},{19.1f,1.8f,15.6f}}}},
    {"solar",1,{-9.2f,.8f,-9.2f},{{{-6.2f,2.3f,-3.5f},{-8.7f,1.7f,-5.8f},{-9.4f,.8f,-8.3f}}}},
    {"pavement-metal",1,{-2.4f,.55f,1.4f},{{{0,1.7f,5},{-1.2f,1.7f,3},{-2.7f,1,.8f}}}},
    {"belt",3,{-15,1.27f,-8},{{{-10,1.7f,-2},{-11.8f,2.7f,-5.2f},{-14,2.4f,-6.7f}}}},
    {"containers",3,{-16,1,9},{{{-10,1.7f,14},{-13.5f,1.7f,12},{-15.5f,1.5f,10.8f}}}},
}};
void look(Camera& c, glm::vec3 eye, glm::vec3 target) {
    c.position=eye; const auto d=target-eye;
    c.yawDegrees=glm::degrees(std::atan2(d.z,d.x));
    c.pitchDegrees=glm::degrees(std::atan2(d.y,glm::length(glm::vec2(d.x,d.z))));
}
void capture(const std::filesystem::path& path) {
    std::vector<unsigned char> pixels(1280*720*3);
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,1280,720,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Framebuffer GL error");
    std::ofstream out(path,std::ios::binary); out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<"P6\n1280 720\n255\n";
    for(int y=719;y>=0;--y) out.write(reinterpret_cast<const char*>(pixels.data()+y*1280*3),1280*3);
}
}
int main(int argc, char** argv) {
    GLFWwindow* window=nullptr;
    try {
        require(argc==4,"Expected binary directory, historical stages root, output directory");
        const std::filesystem::path binary=argv[1],stages=argv[2],output=argv[3];
        std::filesystem::create_directories(output);
        require(glfwInit(),"GLFW init");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        window=glfwCreateWindow(1280,720,"EcoSphere texture visibility A/B/C",nullptr,nullptr);
        require(window,"Window creation"); glfwMakeContextCurrent(window);
        require(gladLoadGL(glfwGetProcAddress),"GLAD"); glfwSwapInterval(0);
        int w=0,h=0; glfwGetFramebufferSize(window,&w,&h);
        require(w==1280 && h==720,"Exact 1280x720 required"); verifySrgbFramebuffer();
        std::cout<<"GPU="<<glGetString(GL_RENDERER)<<" GL="<<glGetString(GL_VERSION)<<std::endl;
        {
            ModelRenderer renderer(binary/"shaders"); Environment environment(binary/"shaders");
            renderer.setShadingModel(ShadingModel::BlinnPhong);
            std::ofstream cameras(output/"cameras.csv"),performance(output/"performance.csv");
            cameras.exceptions(std::ios::badbit|std::ios::failbit);
            performance.exceptions(std::ios::badbit|std::ios::failbit);
            cameras<<std::setprecision(9)<<"state,view,distance,width,height,time,camera_x,camera_y,camera_z,target_x,target_y,target_z,yaw,pitch,fov,shading\n";
            performance<<"state,view,pass,width,height,frames,fps,frame_mean_ms,frame_p95_ms\n";
            std::map<int,std::vector<float>> referenceLights;
            std::map<std::string,std::array<float,16>> referencePoses;
            const std::set<std::string> targets={"AQUA-001","AQUA-003","AQUA-010","AQUA-023","AQUA-027","ENERGY-001","ENERGY-003","ENERGY-004","ENERGY-005","ENERGY-010","CYCLE-001","CYCLE-005","CYCLE-007","CYCLE-023"};
            const auto run=[&](const std::string& state,bool images,int pass) {
                renderer.clear();
                ZoneNavigator nav(state=="C-improved" ? binary/"assets" : stages/state/"assets");
                const ZoneSettings settings; Camera camera; LightingSettings lights;
                std::vector<ModelPlacement> models;
                const auto enter=[&](int zone) {
                    if(nav.active()!=Zone::hub) {nav.cancelInput();nav.input({},false,false);require(nav.input({},false,true),"Zone return");}
                    nav.cancelInput();nav.input({},false,false);
                    require(nav.input(settings.portals.at(zone-1).center,true,false),"Zone entry");
                    renderer.clear(); models.clear(); lights=nav.lighting();
                    std::vector<float> signature{float(lights.legacyUnlit),float(lights.directEnabled),lights.ambient};
                    for(const auto& light:{lights.directional,lights.point})
                        signature.insert(signature.end(),{float(light.enabled),light.intensity,light.vector.x,light.vector.y,light.vector.z,light.color.x,light.color.y,light.color.z});
                    signature.insert(signature.end(),{lights.attenuation.x,lights.attenuation.y,lights.attenuation.z,float(lights.environment.reflectionEnabled),float(lights.environment.skyboxEnabled),lights.environment.intensity,lights.environment.fresnelPower});
                    if(!referenceLights.count(zone)) referenceLights[zone]=signature;
                    require(referenceLights.at(zone)==signature,"A/B/C lighting differs");
                    for(const auto& p:nav.placements()) {
                        std::array<float,16> pose{};
                        for(int c=0;c<4;++c) for(int r=0;r<4;++r) pose[c*4+r]=p.world[c][r];
                        const auto key=std::to_string(zone)+"/"+p.id;
                        if(!referencePoses.count(key)) referencePoses[key]=pose;
                        require(referencePoses.at(key)==pose,"A/B/C animation pose differs");
                        models.push_back({p.model,p.world});
                    }
                };
                const auto draw=[&] {
                    glViewport(0,0,1280,720); glEnable(GL_FRAMEBUFFER_SRGB);
                    glClearColor(.05f,.08f,.12f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                    environment.draw(camera.view(),*camera.projection(1280,720));
                    renderer.drawModels(models,camera.view(),*camera.projection(1280,720),lights,camera.position,environment.texture());
                };
                std::filesystem::create_directories(output/state/"assets");
                for(const auto& v:views) {
                    if(int(nav.active())!=v.zone) enter(v.zone);
                    if(images) for(int distance=0;distance<3;++distance) {
                        look(camera,v.eyes[distance],v.target);
                        for(int i=0;i<3;++i) {draw();glfwSwapBuffers(window);glfwPollEvents();}
                        draw(); const std::string label=std::array<const char*,3>{"normal","medium","close"}[distance];
                        capture(output/state/(std::string(v.name)+"-"+label+".ppm"));
                        cameras<<state<<','<<v.name<<','<<label<<",1280,720,0,"<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<','<<v.target.x<<','<<v.target.y<<','<<v.target.z<<','<<camera.yawDegrees<<','<<camera.pitchDegrees<<','<<camera.fovDegrees<<",BLINN-PHONG\n";
                    }
                    if(!images) {
                        look(camera,v.eyes[0],v.target);
                        for(int i=0;i<30;++i) {draw();glfwSwapBuffers(window);glFinish();}
                        std::vector<double> times; const auto start=glfwGetTime();
                        for(int i=0;i<200;++i) {const auto t=glfwGetTime();draw();glfwSwapBuffers(window);glFinish();times.push_back((glfwGetTime()-t)*1000);}
                        const auto elapsed=glfwGetTime()-start; std::sort(times.begin(),times.end());
                        performance<<state<<','<<v.name<<','<<pass<<",1280,720,200,"<<200/elapsed<<','<<elapsed*5<<','<<times.at(189)<<'\n';
                    }
                }
                if(images) for(int zone:{1,2,3}) {
                    enter(zone); std::set<std::string> seen;
                    for(const auto& p:nav.placements()) {
                        const auto id=p.id.substr(0,p.id.find('-',p.id.find('-')+1));
                        if(!targets.count(id) || !seen.insert(id).second) continue;
                        glm::vec3 lo(std::numeric_limits<float>::infinity()),hi=-lo;
                        p.model->forEach([&](std::size_t index,const glm::mat4& world) {
                            for(const auto& vertex:p.model->meshes.at(index).vertices) {
                                const auto point=glm::vec3(world*glm::vec4(vertex.position,1));lo=glm::min(lo,point);hi=glm::max(hi,point);
                            }
                        },p.world);
                        const auto center=(lo+hi)*.5f;
                        look(camera,center+glm::normalize(glm::vec3(1.2f,1.4f,2.2f))*std::max(glm::length(hi-lo)*.5f,.3f)*3.1f,center);
                        models={{p.model,p.world}};draw();capture(output/state/"assets"/(id+".ppm"));
                    }
                }
                require(glGetError()==GL_NO_ERROR,"Texture visibility GL error");
            };
            for(const auto* state:{"A-main","B-previous","C-improved"}) {run(state,true,0);std::cout<<"CAPTURES PASS "<<state<<std::endl;}
            for(int pass=0;pass<3;++pass) for(const auto* state:(pass%2 ? std::array<const char*,2>{"C-improved","B-previous"} : std::array<const char*,2>{"B-previous","C-improved"})) run(state,false,pass);
        }
        glfwDestroyWindow(window);glfwTerminate();std::cout<<"TEXTURE VISIBILITY GPU PASS: 63 scene captures, 42 asset captures, paired B/C performance"<<std::endl;return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
