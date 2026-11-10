#pragma once
#include <glad/gl.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ecosphere {
// prueba al inicio que el back buffer convierte a sRGB
inline void verifySrgbFramebuffer() {
    if (const auto error = glGetError(); error != GL_NO_ERROR)
        throw std::runtime_error("OpenGL error before sRGB validation: " + std::to_string(error));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    GLint encoding = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
    const auto queryError = glGetError();

    // guardar el estado que se va a tocar
    GLfloat clear[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    const auto enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);

    // limpiar con gris 0.25 sin y con conversión, y leer un píxel
    std::array<unsigned char, 4> linear{};
    std::array<unsigned char, 4> srgb{};
    glClearColor(.25f, .25f, .25f, 1);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, linear.data());
    glEnable(GL_FRAMEBUFFER_SRGB);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, srgb.data());
    const auto probeError = glGetError();

    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    if (!enabled) glDisable(GL_FRAMEBUFFER_SRGB);
    std::cout << "sRGB: encoding=" << encoding << " query_error=" << queryError
              << " probe_error=" << probeError << " linear=" << int(linear[0]) << " encoded=" << int(srgb[0])
              << std::endl;

    // 0.25 lineal da 64, en sRGB da 137 (tolerancia de 3)
    bool converted = probeError == GL_NO_ERROR;
    for (int channel = 0; channel < 3; ++channel) {
        converted = converted && std::abs(int(linear[channel]) - 64) <= 3
            && std::abs(int(srgb[channel]) - 137) <= 3;
    }
    if (!converted)
        throw std::runtime_error("Default framebuffer failed sRGB conversion probe; "
                                 "OpenGL 3.3 with working sRGB conversion is required");
    if (queryError != GL_NO_ERROR || encoding != GL_SRGB)
        std::cout << "sRGB: attachment query inconclusive; real pixel conversion verified" << std::endl;
}
}
