#include "ContentSmoke.hpp"
#include <glad/gl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace ecosphere {
void checkContentFramebuffer(const std::filesystem::path& directory, int width, int height,
    ContentRegistry& registry, const std::string& owner, Camera& camera,
    const glm::mat4& projection, const std::function<void(bool)>& render) {
    std::filesystem::create_directories(directory);
    std::ofstream states(directory/"states.csv"); states.exceptions(std::ios::badbit|std::ios::failbit);
    states << "stage,owner,id,x,y,z,m00,m01,m02\n";
    const auto capture = [&](const char* stage, bool includeContent) {
        render(includeContent); glFinish();
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
        std::vector<float> depth(static_cast<std::size_t>(width)*height);
        glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Content readback failed");
        std::size_t occupied = 0;
        for (float d : depth) {
            if (!std::isfinite(d) || d < 0 || d > 1) throw std::runtime_error("Invalid content depth");
            occupied += d < 1;
        }
        if (includeContent && occupied < 100) throw std::runtime_error("Registered content missing depth");
        std::ofstream file(directory/(std::string(stage)+".ppm"),std::ios::binary);
        file.exceptions(std::ios::badbit|std::ios::failbit);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        for (int y=height-1; y>=0; --y)
            file.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3),width*3);
        for (const auto& entity : registry.placements()) {
            const auto& m = entity.world;
            states << stage << ',' << entity.owner << ',' << entity.id << ',' << m[3].x << ',' << m[3].y << ',' << m[3].z
                   << ',' << m[0].x << ',' << m[0].y << ',' << m[0].z << '\n';
        }
        return pixels;
    };
    const auto changed = [](const auto& a, const auto& b) {
        std::size_t count = 0;
        for (std::size_t i=0; i<a.size(); i+=3)
            count += std::abs(int(a[i])-int(b[i]))+std::abs(int(a[i+1])-int(b[i+1]))+std::abs(int(a[i+2])-int(b[i+2])) > 12;
        if (count < 100) throw std::runtime_error("Content smoke missing visible change");
        std::cout << "Content changed pixels=" << count << '\n';
    };
    // Reencuadrar la muestra de prueba si la cámara de entrada ya no la incluye.
    if(registry.interactions(owner).objects().empty())throw std::runtime_error("Module did not register interaction");
    const auto& first=registry.interactions(owner).objects().front();
    const auto centerFirst=registry.interactions(owner).world(first.id)*glm::vec4((first.bounds.minimum+first.bounds.maximum)*.5f,1);
    const auto projected=projection*camera.view()*centerFirst;
    if(projected.w<=0 || std::abs(projected.x/projected.w)>.9f || std::abs(projected.y/projected.w)>.9f) {
        const auto center=glm::vec3(centerFirst);
        camera.position=center+glm::vec3(0,0,std::max(4.0f,glm::length(first.bounds.maximum-first.bounds.minimum)*1.6f));
        camera.position.y=1.7f;camera.yawDegrees=-90;
        camera.pitchDegrees=glm::degrees(std::atan2(center.y-camera.position.y,camera.position.z-center.z));
    }
    const auto background = capture("background",false);
    const auto initial = capture("initial",true); changed(background,initial);
    auto& system = registry.interactions(owner);
    if (system.objects().empty()) throw std::runtime_error("Module did not register interaction");
    const auto id = system.objects().front().id;
    const auto& box = system.object(id).bounds;
    const auto center = system.world(id)*glm::vec4((box.minimum+box.maximum)*0.5f,1);
    const auto clip = projection*camera.view()*center;
    if (system.pick(cameraRay(camera.view(),projection,glm::vec2(clip)/clip.w)) != std::optional<std::string>(id))
        throw std::runtime_error("Registered object selection failed");
    system.manipulate({0.6f,0.3f,0},glm::angleAxis(glm::radians(25.0f),glm::vec3(0,1,0)),1.2f);
    const auto manipulated = capture("manipulated",true); changed(initial,manipulated);
    registry.update(0.5); changed(manipulated,capture("animated",true));
    states.close();
    std::cout << "CONTENT SMOKE PASS: " << owner << "; registered GLB/material, selection/TRS, animation and real GPU readback\n";
}
}
