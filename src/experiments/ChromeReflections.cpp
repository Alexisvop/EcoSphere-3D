#include "ChromeScene.hpp"
#include "Environment.hpp"
#include "ModelRenderer.hpp"
#include "RenderState.hpp"
#include "app/SrgbFramebuffer.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

using namespace ecosphere;
using namespace ecosphere::chrome_experiment;
namespace {
using Clock = std::chrono::steady_clock;
using Pixels = std::vector<unsigned char>;
constexpr int width=1280, height=720;
void require(bool ok, const char* why) { if(!ok) throw std::runtime_error(why); }
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
// Capture borrows framebuffer state in addition to the state saved by RenderState.
struct CaptureState {
    RenderState render;
    GLint draw, read, buffer, viewport[4]; GLfloat clear[4];
    GLboolean srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB), scissor=glIsEnabled(GL_SCISSOR_TEST);
    CaptureState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&buffer);
        glGetIntegerv(GL_VIEWPORT,viewport); glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    }
    ~CaptureState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw); glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glBindRenderbuffer(GL_RENDERBUFFER,buffer);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glClearColor(clear[0],clear[1],clear[2],clear[3]);
        if(srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
        if(scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }
};
class StaticCapture {
public:
    GLuint texture=0;
    explicit StaticCapture(const std::filesystem::path& shaders, const Scene& scene, const Environment& sky) {
        CaptureState state;
        GLuint fbo=0, depth=0;
        try {
            glActiveTexture(GL_TEXTURE1); glGenTextures(1,&texture);
            glBindTexture(GL_TEXTURE_CUBE_MAP,texture);
            for(int face=0;face<6;++face)
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA16F,probeSize,probeSize,0,GL_RGBA,GL_FLOAT,nullptr);
            for(int face=0;face<6;++face) {
                GLint w=0,h=0,format=0;
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_TEXTURE_WIDTH,&w);
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_TEXTURE_HEIGHT,&h);
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
                require(w==probeSize && h==probeSize && format==GL_RGBA16F,"Unexpected capture storage");
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            for(auto axis:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_WRAP_R})
                glTexParameteri(GL_TEXTURE_CUBE_MAP,axis,GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
            glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth);
            glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,probeSize,probeSize);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
            glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
            glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_SCISSOR_TEST);
            glViewport(0,0,probeSize,probeSize);
            const glm::vec3 direction[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            const glm::vec3 up[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
            const auto projection=glm::perspective(glm::radians(90.f),1.f,.05f,60.f);
            ModelRenderer renderer(shaders);
            for(int face=0;face<6;++face) {
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,texture,0);
                require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete cubemap face");
                glClearColor(0,0,0,1); glDepthMask(GL_TRUE);
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                const auto view=glm::lookAt(center,center+direction[face],up[face]);
                sky.draw(view,projection);
                // shortcut: first-order static capture; moving surroundings require a new capture.
                renderer.draw(scene.surroundings,glm::mat4(1),view,projection,demo::lighting,center,sky.texture());
            }
            require(glGetError()==GL_NO_ERROR,"Static capture GL error");
            glFinish();
        } catch(...) {
            glDeleteRenderbuffers(1,&depth); glDeleteFramebuffers(1,&fbo);
            glDeleteTextures(1,&texture); texture=0; throw;
        }
        glDeleteRenderbuffers(1,&depth); glDeleteFramebuffers(1,&fbo);
    }
    ~StaticCapture() { glDeleteTextures(1,&texture); }
    StaticCapture(const StaticCapture&)=delete;
    StaticCapture& operator=(const StaticCapture&)=delete;
};
glm::vec3 eyeFor(int view) {
    return center+(view==0 ? glm::vec3(0,.15f,3.6f) : glm::vec3(3.6f,.15f,.9f));
}
struct Demo {
    Scene scene=makeScene();
    Environment sky;
    ModelRenderer renderer;
    Demo(const std::filesystem::path& shaders):sky(shaders),renderer(shaders) {}
    void draw(char variant, GLuint capture, glm::vec3 eye, bool reflections=true, ShadingModel shading=ShadingModel::BlinnPhong) {
        auto lights=demo::lighting; lights.environment.reflectionEnabled=reflections;
        const auto view=glm::lookAt(eye,center+glm::vec3(0,0,.72f),glm::vec3(0,1,0));
        const auto projection=glm::perspective(glm::radians(45.f),float(width)/height,.05f,60.f);
        glViewport(0,0,width,height); glEnable(GL_FRAMEBUFFER_SRGB);
        glDepthMask(GL_TRUE); glClearColor(.05f,.08f,.12f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        sky.draw(view,projection);
        renderer.setShadingModel(shading);
        renderer.draw(variant=='A'?scene.original:scene.chrome,glm::mat4(1),view,projection,lights,eye,variant=='C'?capture:sky.texture());
        renderer.draw(scene.surroundings,glm::mat4(1),view,projection,lights,eye,sky.texture());
    }
};
Pixels readPixels() {
    Pixels result(width*height*3);
    glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,result.data());
    require(glGetError()==GL_NO_ERROR,"Framebuffer readback error");
    return result;
}
void save(const std::filesystem::path& path,const Pixels& pixels) {
    require(!std::filesystem::exists(path),"Refusing to overwrite evidence");
    std::ofstream file(path,std::ios::binary); file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)
        file.write(reinterpret_cast<const char*>(pixels.data()+y*width*3),width*3);
    require(bool(file),"Capture write failed");
}
long residentKiB() {
    std::ifstream file("/proc/self/status"); std::string line;
    while(std::getline(file,line)) if(line.rfind("VmRSS:",0)==0) return std::stol(line.substr(6));
    return -1;
}
void evidence(Demo& demo, const std::filesystem::path& shaders,const std::filesystem::path& out) {
    require(!std::filesystem::exists(out),"Evidence destination must be new");
    std::filesystem::create_directories(out);
    std::ofstream log(out/"gpu.txt");
    log<<"GPU="<<glGetString(GL_RENDERER)<<"\nVendor="<<glGetString(GL_VENDOR)
       <<"\nGL="<<glGetString(GL_VERSION)<<"\nGLSL="<<glGetString(GL_SHADING_LANGUAGE_VERSION)
       <<"\nResolution=1280x720 VSync=0 Fresnel=ON\n";
    demo.draw('B',0,eyeFor(0)); glFinish();
    const auto before=residentKiB(); const auto start=Clock::now();
    StaticCapture capture(shaders,demo.scene,demo.sky);
    log<<"Static capture ms="<<milliseconds(start)<<"\nRSS before KiB="<<before
       <<"\nRSS after KiB="<<residentKiB()<<"\nCube RGBA16F bytes="<<6*probeSize*probeSize*8
       <<"\nFaces rendered once=6, reflector excluded, no updates during movement\n";
    GLint framebuffer=0,viewport[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer); glGetIntegerv(GL_VIEWPORT,viewport);
    require(framebuffer==0 && viewport[2]==width && viewport[3]==height && glIsEnabled(GL_FRAMEBUFFER_SRGB),"Capture did not restore window state");
    demo.draw('C',capture.texture,eyeFor(0)); glFinish();
    log<<"RSS after first C frame KiB="<<residentKiB()<<"\nPASS capture storage and window state restored\n";
    for(int view=0;view<2;++view) for(auto shading:{ShadingModel::BlinnPhong,ShadingModel::Phong}) {
        Pixels bOff,bOn,cOff,cOn;
        for(char variant:{'A','B','C'}) for(bool reflection:{false,true}) {
            demo.draw(variant,capture.texture,eyeFor(view),reflection,shading);
            auto pixels=readPixels();
            const auto name=std::string(1,variant)+"-"+(view==0?"front":"grazing")+"-"+shadingName(shading)+(reflection?"-on":"-off")+".ppm";
            save(out/name,pixels);
            if(variant=='B') (reflection?bOn:bOff)=pixels;
            if(variant=='C') (reflection?cOn:cOff)=pixels;
        }
        require(bOff==cOff,"B/C must be identical with ambient reflections OFF");
        require(bOn!=cOn && cOn!=cOff,"Captured environment must visibly affect pixels");
        log<<"PASS B/C OFF exact equality and ON difference view="<<view<<" shading="<<shadingName(shading)<<'\n';
    }
    for(char variant:{'A','B','C'}) {
        Pixels first,last;
        for(int frame=0;frame<24;++frame) {
            const float angle=glm::radians(-60.f+120.f*frame/23.f);
            demo.draw(variant,capture.texture,center+glm::vec3(3.6f*std::sin(angle),.15f,3.6f*std::cos(angle)));
            auto pixels=readPixels(); if(frame==0) first=pixels; last=pixels;
            std::ostringstream name; name<<variant<<"-motion-"<<std::setw(2)<<std::setfill('0')<<frame<<".ppm";
            save(out/name.str(),pixels);
        }
        require(first!=last,"Camera movement did not change image");
    }
    std::ofstream metrics(out/"performance.csv");
    metrics<<"pass,variant,frames,mean_ms,p95_ms,fps\n";
    const std::string orders[]={"ABC","CBA","BCA"};
    for(int pass=0;pass<3;++pass) for(char variant:orders[pass]) {
        for(int i=0;i<30;++i) demo.draw(variant,capture.texture,eyeFor(0));
        glFinish(); std::vector<double> samples;
        for(int i=0;i<120;++i) {
            const auto tick=Clock::now(); demo.draw(variant,capture.texture,eyeFor(0)); glFinish();
            samples.push_back(milliseconds(tick));
        }
        const double mean=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();
        std::sort(samples.begin(),samples.end());
        metrics<<pass<<','<<variant<<",120,"<<mean<<','<<samples[113]<<','<<1000/mean<<'\n';
    }
    require(glGetError()==GL_NO_ERROR,"Final GL error");
    log<<"PASS GL errors=0, 24 stills + 72 moving-camera frames, performance 3x120 frames/variant\n";
    std::cout<<"CHROME GPU PASS "<<out<<'\n';
}
}
int main(int argc,char** argv) {
    GLFWwindow* window=nullptr;
    try {
        const bool record=argc==3 && std::string(argv[1])=="--evidence";
        char variant='C';
        if(!record && argc!=1) {
            require(argc==3 && std::string(argv[1])=="--variant" && std::string(argv[2]).size()==1,"Usage: chrome_reflections_demo [--variant A|B|C | --evidence NEW_DIRECTORY]");
            variant=argv[2][0]; require(variant>='A' && variant<='C',"Variant must be A, B or C");
        }
        require(glfwInit(),"GLFW init failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE,record?GLFW_FALSE:GLFW_TRUE); glfwWindowHint(GLFW_RESIZABLE,GLFW_FALSE);
        window=glfwCreateWindow(width,height,"EcoSphere isolated chrome experiment",nullptr,nullptr);
        require(window,"Window creation failed"); glfwMakeContextCurrent(window);
        require(gladLoadGL(glfwGetProcAddress),"GLAD failed"); glfwSwapInterval(record?0:1);
        int w,h; glfwGetFramebufferSize(window,&w,&h); require(w==width && h==height,"1280x720 framebuffer required");
        verifySrgbFramebuffer();
        const auto shaders=std::filesystem::absolute(argv[0]).parent_path()/"shaders";
        {
            Demo demo(shaders);
            if(record) evidence(demo,shaders,argv[2]);
            else {
                std::unique_ptr<StaticCapture> capture;
                if(variant=='C') capture=std::make_unique<StaticCapture>(shaders,demo.scene,demo.sky);
                while(!glfwWindowShouldClose(window)) {
                    const float angle=.65f*std::sin(float(glfwGetTime())*.35f);
                    demo.draw(variant,capture?capture->texture:0,center+glm::vec3(3.6f*std::sin(angle),.15f,3.6f*std::cos(angle)));
                    glfwSwapBuffers(window); glfwPollEvents();
                }
            }
        }
        glfwDestroyWindow(window); glfwTerminate(); return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        if(window) glfwDestroyWindow(window);
        glfwTerminate(); return 1;
    }
}
