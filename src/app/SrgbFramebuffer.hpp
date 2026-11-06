#pragma once
#include <glad/gl.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace ecosphere {
// Run once at startup, before drawing content, on the window's back buffer.
inline void verifySrgbFramebuffer() {
    if(const auto error=glGetError(); error!=GL_NO_ERROR)
        throw std::runtime_error("OpenGL error before sRGB validation: "+std::to_string(error));
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    GLint encoding=0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK_LEFT,
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
    const auto queryError=glGetError();
    GLfloat clear[4];glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    const auto enabled=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    std::array<unsigned char,4> linear{},srgb{};
    glClearColor(.25f,.25f,.25f,1);
    glDisable(GL_FRAMEBUFFER_SRGB);glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,linear.data());
    glEnable(GL_FRAMEBUFFER_SRGB);glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,srgb.data());
    const auto probeError=glGetError();
    glClearColor(clear[0],clear[1],clear[2],clear[3]);
    if(!enabled)glDisable(GL_FRAMEBUFFER_SRGB);
    std::cout<<"sRGB: encoding="<<encoding<<" query_error="<<queryError
             <<" probe_error="<<probeError<<" linear="<<int(linear[0])<<" encoded="<<int(srgb[0])<<std::endl;
    // IEC sRGB(.25) = .53710: about 137/255 versus 64/255 without conversion.
    // Three code values allow framebuffer quantization/dithering, not missing gamma.
    bool converted=probeError==GL_NO_ERROR;
    for(int c=0;c<3;++c)converted=converted && std::abs(int(linear[c])-64)<=3 && std::abs(int(srgb[c])-137)<=3;
    if(!converted)throw std::runtime_error("Default framebuffer failed sRGB conversion probe; OpenGL 3.3 with working sRGB conversion is required");
    if(queryError!=GL_NO_ERROR || encoding!=GL_SRGB)
        std::cout<<"sRGB: attachment query inconclusive; real pixel conversion verified"<<std::endl;
}
}
