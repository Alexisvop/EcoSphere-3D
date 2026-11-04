#include "NavigationHud.hpp"
#include "RenderState.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace ecosphere {
void NavigationHud::draw(const std::string& message,int width,int height) {
    if(!model_ || message!=text_ || width!=width_ || height!=height_) {
        layout_=layoutNavigationHud(message,width,height);
        renderer_.clear();model_.reset();text_=message;width_=width;height_=height;
        if(!layout_.lines.empty())model_=makeNavigationHudModel(layout_);
    }
    if(!model_)return;
    RenderState restore;
    // Reserve only the panel's depth; restore the borrowed scissor/clear state.
    GLint box[4];glGetIntegerv(GL_SCISSOR_BOX,box);const auto scissor=glIsEnabled(GL_SCISSOR_TEST);
    GLdouble oldClear;glGetDoublev(GL_DEPTH_CLEAR_VALUE,&oldClear);
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(layout_.x),int(layout_.y),int(std::ceil(layout_.width)),int(std::ceil(layout_.height)));
    glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glClearDepth(oldClear);
    glScissor(box[0],box[1],box[2],box[3]);if(!scissor)glDisable(GL_SCISSOR_TEST);
    const auto placement=glm::translate(glm::mat4(1),glm::vec3(layout_.x,layout_.y,0));
    LightingSettings flat;flat.legacyUnlit=false;
    renderer_.draw(model_,placement,glm::mat4(1),glm::ortho(0.f,float(width),0.f,float(height),-1.f,1.f),flat);
}
}
