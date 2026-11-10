#pragma once
#include <glad/gl.h>

namespace ecosphere {
// guarda el estado de OpenGL al crearse y lo restaura al destruirse
class RenderState final {
public:
    RenderState()
        : blend_(glIsEnabled(GL_BLEND)),
          cull_(glIsEnabled(GL_CULL_FACE)),
          depth_(glIsEnabled(GL_DEPTH_TEST)) {
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite_);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc_);
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode_);
        glGetIntegerv(GL_FRONT_FACE, &frontFace_);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb_);
        glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb_);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha_);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha_);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRgb_);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqAlpha_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active_);
        // texturas de las unidades 0 (2D) y 1 (cubemap)
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &cube_);
        glActiveTexture(active_);
    }

    ~RenderState() {
        set(GL_BLEND, blend_);
        set(GL_CULL_FACE, cull_);
        set(GL_DEPTH_TEST, depth_);
        glDepthMask(depthWrite_);
        glDepthFunc(depthFunc_);
        glCullFace(cullMode_);
        glFrontFace(frontFace_);
        glBlendFuncSeparate(srcRgb_, dstRgb_, srcAlpha_, dstAlpha_);
        glBlendEquationSeparate(eqRgb_, eqAlpha_);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube_);
        glActiveTexture(active_);
    }

    RenderState(const RenderState&) = delete;
    RenderState& operator=(const RenderState&) = delete;
private:
    static void set(GLenum flag, GLboolean enabled) {
        if (enabled) {
            glEnable(flag);
        } else {
            glDisable(flag);
        }
    }
    GLboolean blend_, cull_, depth_, depthWrite_;
    GLint depthFunc_, cullMode_, frontFace_, srcRgb_, dstRgb_, srcAlpha_, dstAlpha_, eqRgb_, eqAlpha_;
    GLint program_, vao_, arrayBuffer_, active_, texture_, cube_;
};
}
