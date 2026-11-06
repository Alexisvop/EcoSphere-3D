#include "app/FpsCounter.hpp"
#include "app/NavigationHudLayout.hpp"
#include "InstanceCsv.hpp"
#include <limits>
#include <stdexcept>
#include <iostream>
void require(bool ok) {if(!ok)throw std::runtime_error("FPS/serialization check failed");}
int main() {
    ecosphere::FpsCounter fps;
    require(fps.visible && fps.text()=="FPS: -");
    fps.sample(0,true);for(int i=1;i<=32;++i)fps.sample(i/64.,true);
    require(fps.text()=="FPS: 64");
    fps.key(GLFW_KEY_F3,GLFW_REPEAT);require(fps.visible);
    fps.key(GLFW_KEY_F3,GLFW_PRESS);require(!fps.visible);
    fps.key(GLFW_KEY_F3,GLFW_RELEASE);require(!fps.visible);
    fps.key(GLFW_KEY_F3,GLFW_PRESS);require(fps.visible);
    for(int key:{GLFW_KEY_ENTER,GLFW_KEY_H,GLFW_KEY_F1,GLFW_KEY_F2,GLFW_KEY_P,GLFW_KEY_R,GLFW_KEY_G})fps.key(key,GLFW_PRESS);
    require(fps.visible);
    for(double invalid:{.5,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),99.}) {
        fps.sample(invalid,true);require(fps.text()=="FPS: -");
    }
    fps.sample(100,false);fps.sample(101,true);fps.sample(101.5,true);require(fps.text()=="FPS: 2");
    fps.sample(102,false);require(fps.text()=="FPS: -");
    for(const auto& size:{std::pair<int,int>{1280,720},{640,360},{320,240}}) {
        const auto layout=ecosphere::layoutNavigationHud("FPS: 0123456789",size.first,size.second);
        require(!layout.lines.empty() && layout.width<size.first-20 && layout.height<=40);
        for(char digit:std::string("0123456789:")) {
            const auto model=ecosphere::makeNavigationHudModel(ecosphere::layoutNavigationHud(std::string(1,digit),size.first,size.second));
            require(!model->meshes.at(1).vertices.empty());
        }
    }
    const std::string csv="id,owner,model,path,x,y,z,qw,qx,qy,qz,sx,sy,sz\na,hub,m,p,0,0,0,1,0,0,0,1,1,1\n";
    require(sameInstanceCsv(csv,csv));
    auto changed=csv;changed.replace(changed.find(",0,0,0,1"),8,",0.0000001,0,0,1");require(sameInstanceCsv(csv,changed));
    changed=csv;changed.replace(changed.find("a,hub"),1,"b");require(!sameInstanceCsv(csv,changed));
    changed=csv;changed.replace(changed.find(",0,0,0,1"),8,",0.001,0,0,1");require(!sameInstanceCsv(csv,changed));
    require(!sameInstanceCsv(csv,csv+"extra\n"));
    std::cout<<"FPS sampling, invalid time, focus reset, F3 edges, controls, glyphs, resolutions and CSV tolerance PASS\n";
}
