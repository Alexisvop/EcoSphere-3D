#include "LightingSmoke.hpp"
#include "world/LightingDemoSettings.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;
struct Region { int x0,y0,x1,y1; };
float linear(unsigned char c) {
    const float v=c/255.0f;
    return v<=0.04045f ? v/12.92f : std::pow((v+0.055f)/1.055f,2.4f);
}
}
void checkLightingFramebuffer(const std::filesystem::path& directory, int width, int height,
    const glm::mat4& vp, const LightingSettings& settings, Model& showcase,
    const std::function<void(const LightingSettings&)>& render) {
    std::filesystem::create_directories(directory);
    std::map<std::string,Pixels> shots;
    const auto capture=[&](const std::string& name,const LightingSettings& s) {
        render(s);
        auto& pixels=shots[name]; pixels.resize(static_cast<std::size_t>(width)*height*3);
        glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Lighting readback failed: "+name);
        std::ofstream out(directory/(name+".ppm"),std::ios::binary); out.exceptions(std::ios::badbit|std::ios::failbit);
        out<<"P6\n"<<width<<' '<<height<<"\n255\n";
        const auto stride=static_cast<std::size_t>(width)*3;
        for(int y=height-1;y>=0;--y) out.write(reinterpret_cast<const char*>(pixels.data()+y*stride),static_cast<std::streamsize>(stride));
        out.close();
    };
    auto s=settings;
    capture("combined",s);
    s.environment.reflectionEnabled=false; capture("direct-only",s);
    s.ambient=0; s.point.enabled=false; capture("directional",s);
    s.directional.enabled=false; s.point.enabled=true; capture("point",s);
    s=settings; s.directEnabled=false; capture("environment-only",s);
    s.environment.reflectionEnabled=false; capture("neither",s);
    s=settings; s.environment.fresnelPower=1; capture("fresnel-power-one",s);
    s=settings; s.environment.skyboxEnabled=false; capture("no-skybox",s);
    // Comparación de lóbulo especular sobre el mismo plástico y la misma luz.
    const float specular=showcase.meshes.at(1).material.specular;
    showcase.meshes.at(1).material.specular=0;
    capture("no-specular",settings);
    showcase.meshes.at(1).material.specular=specular;
    const float alpha=showcase.meshes.at(4).color.a;
    showcase.meshes.at(4).color.a=0; capture("glass-zero",settings);
    showcase.meshes.at(4).color.a=1; capture("glass-one",settings);
    showcase.meshes.at(4).color.a=alpha;
    showcase.meshes.at(5).visible=false; capture("no-mask",settings);
    showcase.meshes.at(5).visible=true;
    const auto region=[&](glm::vec3 center,float radius) {
        const auto clip=vp*glm::vec4(center,1);
        const auto edge=vp*glm::vec4(center+glm::vec3(radius,0,0),1);
        if(clip.w<=0 || edge.w<=0) throw std::runtime_error("Lighting region behind camera");
        const float x=(clip.x/clip.w+1)*0.5f*width, y=(clip.y/clip.w+1)*0.5f*height;
        const int r=std::max(1,int(std::abs(edge.x/edge.w-clip.x/clip.w)*0.5f*width));
        return Region{std::max(0,int(x)-r),std::max(0,int(y)-r),std::min(width,int(x)+r),std::min(height,int(y)+r)};
    };
    const auto difference=[&](const char* a,const char* b,Region r) {
        std::size_t count=0;
        for(int y=r.y0;y<r.y1;++y) for(int x=r.x0;x<r.x1;++x) {
            const auto offset=(static_cast<std::size_t>(y)*width+x)*3;
            int delta=0;
            for(int c=0;c<3;++c) delta+=std::abs(int(shots.at(a)[offset+c])-int(shots.at(b)[offset+c]));
            if(delta>12) ++count;
        }
        return count;
    };
    const auto checkDifference=[&](const char* a,const char* b,Region r,const char* label) {
        const auto count=difference(a,b,r);
        std::cout<<"LIGHTING "<<label<<": changed="<<count<<'\n';
        if(count<30) throw std::runtime_error(std::string("Insufficient visual signal: ")+label);
    };
    const auto matte=region(demo::materialPositions[0].position,0.42f);
    const auto plastic=region(demo::materialPositions[1].position,0.42f);
    const auto metal=region(demo::materialPositions[2].position,0.42f);
    checkDifference("directional","neither",matte,"directional on matte");
    checkDifference("point","neither",plastic,"point on plastic");
    checkDifference("directional","point",matte,"directional versus point");
    checkDifference("environment-only","neither",metal,"environment on metal");
    checkDifference("combined","direct-only",metal,"reflection with direct");
    checkDifference("combined","environment-only",matte,"direct with reflection");
    checkDifference("combined","no-specular",plastic,"plastic specular lobe");
    checkDifference("combined","fresnel-power-one",region(demo::materialPositions[1].position,0.65f),"Fresnel angular curve");
    checkDifference("combined","no-skybox",{0,height*3/4,width,height},"skybox background");
    const auto glass=region(demo::glassPlacement.position,0.35f);
    checkDifference("combined","glass-zero",glass,"glass on backdrop");
    checkDifference("combined","glass-one",glass,"backdrop through glass");
    std::size_t linearMatches=0, tested=0;
    for(int y=glass.y0;y<glass.y1;++y) for(int x=glass.x0;x<glass.x1;++x) {
        const auto p=(static_cast<std::size_t>(y)*width+x)*3;
        float error=0,signal=0;
        for(int c=0;c<3;++c) {
            const float bg=linear(shots.at("glass-zero")[p+c]), fg=linear(shots.at("glass-one")[p+c]);
            signal+=std::abs(bg-fg);
            error+=std::abs(linear(shots.at("combined")[p+c])-((1-alpha)*bg+alpha*fg));
        }
        if(signal>0.1f) { ++tested; if(error<0.025f) ++linearMatches; }
    }
    if(tested<100 || linearMatches<tested*0.95) throw std::runtime_error("Alpha blend not linear / backdrop missing");
    const auto mask=region(demo::maskPlacement.position,0.38f);
    const auto maskChanged=difference("combined","no-mask",mask);
    const auto area=static_cast<std::size_t>(mask.x1-mask.x0)*(mask.y1-mask.y0);
    if(maskChanged<30 || maskChanged>area*0.85) throw std::runtime_error("MASK must retain holes and opaque pixels");
    std::size_t unlitMagenta=0, unlitCyan=0;
    const auto& combined=shots.at("combined");
    for(std::size_t p=0;p<combined.size();p+=3) {
        const bool magenta=combined[p]>240 && combined[p+1]<10 && combined[p+2]>240;
        const bool cyan=combined[p]<10 && combined[p+1]>240 && combined[p+2]>240;
        if(!magenta && !cyan) continue;
        bool stable=true;
        for(const auto* name:{"neither","directional","point","environment-only"})
            for(int c=0;c<3;++c) stable=stable && combined[p+c]==shots.at(name)[p+c];
        if(stable) { if(magenta) ++unlitMagenta; if(cyan) ++unlitCyan; }
    }
    if(unlitMagenta<10 || unlitCyan<10) throw std::runtime_error("Imported unlit must remain invariant under lighting toggles");
    std::cout<<"LIGHTING unlit invariant: magenta="<<unlitMagenta<<" cyan="<<unlitCyan<<'\n';
    std::cout<<"LIGHTING PASS: linear alpha="<<alpha<<" matched="<<linearMatches<<'/'<<tested
             <<"; MASK changed="<<maskChanged<<'/'<<area<<"; framebuffer="<<width<<'x'<<height
             <<"; real captures="<<directory.string()<<'\n';
}
}
