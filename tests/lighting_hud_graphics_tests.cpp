#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "app/LightingControls.hpp"
#include "app/NavigationHud.hpp"
#include "app/FpsCounter.hpp"
#include "app/SrgbFramebuffer.hpp"
#include "world/ZoneNavigator.hpp"
#include "Environment.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ecosphere;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<GLint> state() {
    std::vector<GLint> out;
    for(GLenum name:{GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,
        GL_DEPTH_FUNC,GL_DEPTH_WRITEMASK,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,
        GL_READ_BUFFER,GL_DRAW_BUFFER,GL_SCISSOR_TEST,GL_BLEND,GL_CULL_FACE,GL_DEPTH_TEST,GL_FRAMEBUFFER_SRGB}) {
        GLint value=0;glGetIntegerv(name,&value);out.push_back(value);
    }
    for(GLenum name:{GL_VIEWPORT,GL_SCISSOR_BOX}) {GLint box[4];glGetIntegerv(name,box);out.insert(out.end(),box,box+4);}
    return out;
}
int main(int argc,char** argv) {
    GLFWwindow* window=nullptr;
    try {
        require(argc==3,"Expected binary directory and evidence directory");
        const std::filesystem::path binary=argv[1],output=argv[2];std::filesystem::create_directories(output);
        require(glfwInit(),"GLFW init failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        window=glfwCreateWindow(1280,720,"EcoSphere lighting HUD validation",nullptr,nullptr);
        require(window,"Window failed");glfwMakeContextCurrent(window);require(gladLoadGL(glfwGetProcAddress),"GLAD failed");
        std::cout<<"GPU: "<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<"; GL "<<glGetString(GL_VERSION)<<"; GLFW "<<glfwGetVersionString()<<std::endl;
        verifySrgbFramebuffer();
        {
            NavigationHud navigation(binary/"shaders"),fpsHud(binary/"shaders"),panel(binary/"shaders");
            ModelRenderer renderer(binary/"shaders");Environment environment(binary/"shaders");
            ZoneNavigator nav(binary/"assets");ZoneSettings zones;LightingControls controls;FpsCounter fps;
            fps.sample(0,true);for(int i=1;i<=32;++i)fps.sample(i/64.,true);
            bool regression=false;
            std::ofstream report(output/"performance.csv");
            report<<"zone,swap_interval,pass,overlay,frames,seconds,fps\n";
            for(int zone=0;zone<4;++zone) {
                if(zone) {nav.cancelInput();nav.input({},false,false);require(nav.input(zones.portals[zone-1].center,true,false),"Enter zone");}
                auto lighting=nav.lighting();controls.defaults(nav.lighting());
                require(controls.visible==(zone==0),"Visibility lost during zone transition");
                controls.visible=true;
                const auto camera=nav.entryCamera();
                for(const auto& size:{std::pair<int,int>{1280,720},{640,360},{320,240}}) {
                    glfwSetWindowSize(window,size.first,size.second);
                    int w=0,h=0;const auto startResize=glfwGetTime();
                    do {
                        glfwPollEvents();glfwGetFramebufferSize(window,&w,&h);
                        if(w==size.first && h==size.second)break;
                        glfwWaitEventsTimeout(.01);
                    }while(glfwGetTime()-startResize<2);
                    require(w==size.first && h==size.second,"Resize not granted");
                    glViewport(0,0,w,h);glEnable(GL_DEPTH_TEST);glEnable(GL_FRAMEBUFFER_SRGB);
                    const auto text=[&]{return lightingHudText(lighting,regression,w);};
                    auto layout=layoutNavigationHud(text(),w,h);layout.x=w-10-layout.width;layout.y=10;
                    const auto render=[&](bool withPanel,bool checkState=false) {
                        glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                        if(lighting.environment.skyboxEnabled)environment.draw(camera.view(),*camera.projection(w,h));
                        std::vector<ModelPlacement> models;for(const auto& p:nav.placements())models.push_back({p.model,p.world});
                        renderer.drawModels(models,camera.view(),*camera.projection(w,h),lighting,camera.position,environment.texture());
                        navigation.draw(nav.message(camera.position),w,h);
                        if(fps.visible)fpsHud.draw(fps.text(),w,h,HudAnchor::topRight);
                        if(withPanel) {
                            const auto before=checkState ? state() : std::vector<GLint>{};panel.draw(text(),w,h,HudAnchor::bottomRight);
                            if(checkState)require(state()==before,"HUD changed borrowed OpenGL state");
                        }
                    };
                    const auto pixels=[&]{std::vector<unsigned char> data(w*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,data.data());return data;};
                    const auto shot=[&](const std::string& name,const std::vector<unsigned char>& rgb) {
                        std::ofstream file(output/(name+".ppm"),std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);
                        file<<"P6\n"<<w<<' '<<h<<"\n255\n";
                        for(int y=h-1;y>=0;--y)file.write(reinterpret_cast<const char*>(rgb.data()+y*w*3),w*3);
                    };
                    for(int i=0;i<3;++i){render(false);glfwSwapBuffers(window);glfwPollEvents();}
                    render(false);const auto without=pixels();render(true,true);const auto with=pixels();
                    std::size_t changed=0,outside=0;
                    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                        const auto i=(y*w+x)*3;
                        if(with[i]!=without[i] || with[i+1]!=without[i+1] || with[i+2]!=without[i+2]) {
                            ++changed;if(x<int(layout.x) || x>=int(std::ceil(layout.x+layout.width)) || y<10 || y>=int(std::ceil(10+layout.height)))++outside;
                        }
                    }
                    require(changed>100 && outside==0,"Missing/clipped HUD or changes outside panel");
                    const auto name=std::string(zoneId(nav.active()))+"-"+std::to_string(w)+"x"+std::to_string(h);
                    shot(name,with);
                    controls.key(GLFW_KEY_F4,GLFW_PRESS,lighting);render(controls.visible);require(pixels()==without,"F4 off changed scene/FPS/education");
                    controls.key(GLFW_KEY_F4,GLFW_REPEAT,lighting);require(!controls.visible,"F4 repeated");
                    controls.key(GLFW_KEY_F4,GLFW_PRESS,lighting);render(controls.visible);require(pixels()==with,"F4 on failed");
                    fps.key(GLFW_KEY_F3,GLFW_PRESS);render(true);const auto noFps=pixels();
                    std::size_t fpsChanges=0;
                    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                        const auto i=(y*w+x)*3;
                        if(noFps[i]!=with[i] || noFps[i+1]!=with[i+1] || noFps[i+2]!=with[i+2]) {
                            ++fpsChanges;require(x>=w-150 && y>=h-45,"F3 changed lighting/educational HUD");
                        }
                    }
                    require(fpsChanges>100,"FPS missing");fps.key(GLFW_KEY_F3,GLFW_PRESS);
                    for(int key=GLFW_KEY_1;key<=GLFW_KEY_5;++key) {
                        controls.key(key,GLFW_PRESS,lighting);render(true);require(pixels()!=with,"Light indicator failed to change");
                        controls.key(GLFW_KEY_0,GLFW_PRESS,lighting);render(true);require(pixels()==with,"Zone reset failed to restore framebuffer");
                    }
                    controls.key(GLFW_KEY_F4,GLFW_PRESS,lighting);controls.key(GLFW_KEY_1,GLFW_PRESS,lighting);
                    render(false);const auto dark=pixels();require(dark!=without,"Direct-light toggle produced no scene effect");
                    controls.key(GLFW_KEY_F4,GLFW_PRESS,lighting);render(true);
                    if(w==320)shot(std::string(zoneId(nav.active()))+"-masked-320x240",pixels());
                    controls.key(GLFW_KEY_0,GLFW_PRESS,lighting);render(true);require(pixels()==with,"Hidden change/reset failed");
                    require(glGetError()==GL_NO_ERROR,"OpenGL error");
                    std::cout<<name<<" PASS: "<<changed<<" panel pixels; outside=0; F4/F3; five flags; hidden update; reset; GL state"<<std::endl;
                    if(w==1280)for(int swap:{0,1}) {
                        glfwSwapInterval(swap);
                        for(int pass=0;pass<(swap==0?3:1);++pass)for(bool overlay:std::array<bool,2>{pass%2!=0,pass%2==0}) {
                            for(int i=0;i<30;++i){render(overlay);glfwSwapBuffers(window);}
                            glFinish();const auto start=glfwGetTime();
                            for(int i=0;i<300;++i){render(overlay);glfwSwapBuffers(window);}
                            glFinish();const auto elapsed=glfwGetTime()-start;
                            report<<zoneId(nav.active())<<','<<swap<<','<<pass<<','<<overlay<<",300,"<<elapsed<<','<<300/elapsed<<'\n';
                        }
                    }
                }
                controls.key(GLFW_KEY_F4,GLFW_PRESS,lighting);
                if(zone){nav.cancelInput();nav.input({},false,false);require(nav.input({},false,true),"Return to plaza");}
                renderer.clear();
            }
        }
        glfwDestroyWindow(window);glfwTerminate();std::cout<<"LIGHTING HUD GPU PASS"<<std::endl;return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
