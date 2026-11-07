#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "app/NavigationHud.hpp"
#include "app/FpsCounter.hpp"
#include "app/SrgbFramebuffer.hpp"
#include "world/ZoneNavigator.hpp"
#include "Environment.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ecosphere;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    GLFWwindow* window=nullptr;
    try {
        require(argc==3,"Expected staged binary directory and evidence directory");
        const std::filesystem::path binary=argv[1],output=argv[2];
        std::filesystem::create_directories(output);
        require(glfwInit(),"GLFW init failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        window=glfwCreateWindow(1280,720,"EcoSphere FPS validation",nullptr,nullptr);
        require(window,"Window failed");glfwMakeContextCurrent(window);
        require(gladLoadGL(glfwGetProcAddress),"GLAD failed");
        std::cout<<"GPU: "<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<"; GL "<<glGetString(GL_VERSION)<<"; GLFW "<<glfwGetVersionString()<<'\n';
        verifySrgbFramebuffer();
        {
            NavigationHud hud(binary/"shaders"),fpsHud(binary/"shaders");
            ModelRenderer renderer(binary/"shaders");Environment environment(binary/"shaders");
            ZoneNavigator nav(binary/"assets");FpsCounter fps;
            std::ofstream report(output/"overlay-performance.csv");
            report<<"zone,width,height,swap_interval,pass,overlay,frames,seconds,fps\n";
            const ZoneSettings settings;
            for(int zone=0;zone<4;++zone) {
                if(zone) {nav.cancelInput();nav.input({},false,false);require(nav.input(settings.portals[zone-1].center,true,false),"Zone entry failed");}
                const auto camera=nav.entryCamera();
                for(const auto& size:{std::pair<int,int>{1280,720},{640,360},{320,240}}) {
                    glfwSetWindowSize(window,size.first,size.second);glfwPollEvents();
                    int w=0,h=0;
                    const auto resizeStart=glfwGetTime();
                    do {
                        glfwPollEvents();glfwGetFramebufferSize(window,&w,&h);
                        if(w==size.first && h==size.second)break;
                        glfwWaitEventsTimeout(.01);
                    } while(glfwGetTime()-resizeStart<2);
                    require(w==size.first && h==size.second,"Compositor did not grant test resolution");
                    glViewport(0,0,w,h);glEnable(GL_DEPTH_TEST);glEnable(GL_FRAMEBUFFER_SRGB);
                    const auto draw=[&](bool overlay) {
                        glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                        const auto lighting=nav.lighting();
                        environment.draw(camera.view(),*camera.projection(w,h));
                        std::vector<ModelPlacement> models;
                        for(const auto& p:nav.placements())models.push_back({p.model,p.world});
                        renderer.drawModels(models,camera.view(),*camera.projection(w,h),lighting,camera.position,environment.texture());
                        hud.draw(nav.message(camera.position),w,h);
                        if(overlay)fpsHud.draw(fps.text(),w,h,HudAnchor::topRight);
                    };
                    fps.sample(0,true);for(int i=1;i<=32;++i)fps.sample(i/64.,true);
                    const auto pixels=[&] {std::vector<unsigned char> data(w*h*3);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,data.data());return data;};
                    // Let the native drawable allocate its resized back buffers.
                    for(int warm=0;warm<3;++warm){draw(false);glfwSwapBuffers(window);glfwPollEvents();}
                    draw(false);const auto before=pixels();draw(true);const auto after=pixels();
                    std::size_t changed=0,outside=0;
                    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                        const auto i=(y*w+x)*3;
                        if(before[i]!=after[i] || before[i+1]!=after[i+1] || before[i+2]!=after[i+2]) {
                            ++changed;if(x<w-150 || y<h-45)++outside;
                        }
                    }
                    std::cout<<zoneId(nav.active())<<" "<<w<<"x"<<h<<" changed="<<changed<<" outside="<<outside<<std::endl;
                    require(changed>100 && outside==0,"FPS missing, clipped, or changed scene outside its panel");
                    const auto name=std::string(zoneId(nav.active()))+"-"+std::to_string(w)+"x"+std::to_string(h);
                    std::ofstream image(output/(name+".ppm"),std::ios::binary);
                    image<<"P6\n"<<w<<' '<<h<<"\n255\n";
                    for(int y=h-1;y>=0;--y)image.write(reinterpret_cast<const char*>(after.data()+y*w*3),w*3);
                    fps.key(GLFW_KEY_F3,GLFW_PRESS);draw(fps.visible);require(pixels()==before,"F3 off did not restore framebuffer");
                    fps.key(GLFW_KEY_F3,GLFW_PRESS);draw(fps.visible);require(pixels()==after,"F3 on did not restore overlay");
                    std::cout<<name<<": overlay pixels="<<changed<<"; F3 off/on PASS\n";
                    if(w==1280)for(int swap:{0,1}) {
                        glfwSwapInterval(swap);
                        for(int pass=0;pass<(swap==0 ? 3 : 1);++pass)
                        for(bool overlay:std::array<bool,2>{pass%2!=0,pass%2==0}) {
                            for(int warm=0;warm<30;++warm){draw(overlay);glfwSwapBuffers(window);}
                            glFinish();const auto start=glfwGetTime();
                            for(int i=0;i<300;++i){draw(overlay);glfwSwapBuffers(window);}
                            glFinish();const auto seconds=glfwGetTime()-start;
                            report<<zoneId(nav.active())<<','<<w<<','<<h<<','<<swap<<','<<pass<<','<<overlay<<",300,"<<seconds<<','<<300/seconds<<'\n';
                        }
                    }
                    require(glGetError()==GL_NO_ERROR,"Graphics GL error");
                    fps.sample(0,false);
                }
                if(zone){nav.cancelInput();nav.input({},false,false);require(nav.input({},false,true),"Zone return failed");}
            }
        }
        glfwDestroyWindow(window);glfwTerminate();std::cout<<"FPS GPU PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
