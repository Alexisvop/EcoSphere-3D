#include "world/InteractionAnimationDemo.hpp"
#include "Triangle.hpp"
#include "SceneRenderer.hpp"
#include "ModelRenderer.hpp"
#include "Environment.hpp"
#include <map>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <set>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

namespace {
int deletedShaders = 0, deletedPrograms = 0;
GLuint nextShader = 0;
bool compileSucceeds = false, simulateAllocationFailure = false;

template<class Construct>
void checkFailures(const Construct& construct) {
    for (bool linkFailure : {false, true}) {
        compileSucceeds = linkFailure;
        for (bool allocationFailure : {false, true}) {
            simulateAllocationFailure = allocationFailure;
            deletedShaders = deletedPrograms = 0;
            bool reported = false;
            try { construct(); }
            catch (const std::bad_alloc&) { reported = allocationFailure; }
            catch (const std::runtime_error& error) {
                reported = !allocationFailure && std::string(error.what()).find(
                    linkFailure ? "simulated link failure" : "simulated invalid GLSL") != std::string::npos;
            }
            if (!reported || deletedShaders != (linkFailure ? 2 : 1) || deletedPrograms != (linkFailure ? 1 : 0)) {
                throw std::runtime_error("Shader compile/link/diagnostic failure must report and release exactly once");
            }
        }
    }
}
}

int main(int argc, char** argv) {
    // Funciones simuladas: conserva la regresión original y prueba también el renderer nuevo.
    glad_glCreateShader = [](GLenum) -> GLuint { return ++nextShader; };
    glad_glShaderSource = [](GLuint, GLsizei, const GLchar* const*, const GLint*) {};
    glad_glCompileShader = [](GLuint) {};
    glad_glGetShaderiv = [](GLuint, GLenum parameter, GLint* value) {
        *value = parameter == GL_COMPILE_STATUS ? (compileSucceeds ? GL_TRUE : GL_FALSE) : 64;
    };
    glad_glGetShaderInfoLog = [](GLuint, GLsizei, GLsizei*, GLchar* text) {
        // Excepción durante diagnóstico: representa fallo al reservar memoria para el log.
        if (simulateAllocationFailure) throw std::bad_alloc();
        std::strcpy(text, "simulated invalid GLSL");
    };
    glad_glDeleteShader = [](GLuint) { ++deletedShaders; };
    glad_glCreateProgram = []() -> GLuint { return 100; };
    glad_glAttachShader = [](GLuint, GLuint) {};
    glad_glLinkProgram = [](GLuint) {};
    glad_glGetProgramiv = [](GLuint, GLenum parameter, GLint* value) {
        *value = parameter == GL_LINK_STATUS ? GL_FALSE : 64;
    };
    glad_glGetProgramInfoLog = [](GLuint, GLsizei, GLsizei*, GLchar* text) {
        if (simulateAllocationFailure) throw std::bad_alloc();
        std::strcpy(text, "simulated link failure");
    };
    glad_glDeleteTextures = [](GLsizei, const GLuint*) {};
    glad_glDeleteBuffers = [](GLsizei, const GLuint*) {};
    glad_glDeleteVertexArrays = [](GLsizei, const GLuint*) {};
    glad_glDeleteProgram = [](GLuint program) { if (program) ++deletedPrograms; };

    try {
        if (argc != 2) throw std::runtime_error("Test needs shader directory");
        checkFailures([] { ecosphere::Triangle triangle; });
        checkFailures([&] { ecosphere::SceneRenderer renderer(argv[1]); });
        checkFailures([&] { ecosphere::ModelRenderer renderer(argv[1]); });
        checkFailures([&] { ecosphere::Environment environment(argv[1]); });
        bool missingReported = false;
        try { ecosphere::SceneRenderer renderer("missing-shader-directory-for-test"); }
        catch (const std::runtime_error& error) {
            missingReported = std::string(error.what()).find("Cannot open shader:") != std::string::npos;
        }
        if (!missingReported) throw std::runtime_error("Missing shader must report path");
        // GL simulado permite contar handles vivos y verificar rollback/reintento sin GPU.
        std::set<GLuint> buffers, arrays, textures;
        GLuint nextHandle = 200;
        int checks = 0, failAt = 0, drawCalls = 0, bufferUploads = 0, textureUploads = 0;
        // Punteros GL C no capturan: el estado vive hasta terminar esta prueba.
        struct State {
            std::set<GLuint>* buffers; std::set<GLuint>* arrays; std::set<GLuint>* textures;
            GLuint* next; int* checks; int* failAt; int* draws; int* bufferUploads; int* textureUploads;
        };
        static State state{&buffers, &arrays, &textures, &nextHandle, &checks, &failAt, &drawCalls, &bufferUploads, &textureUploads};
        glad_glGetShaderiv = [](GLuint, GLenum parameter, GLint* value) { *value = parameter == GL_COMPILE_STATUS ? GL_TRUE : 0; };
        glad_glGetProgramiv = [](GLuint, GLenum parameter, GLint* value) { *value = parameter == GL_LINK_STATUS ? GL_TRUE : 0; };
        glad_glGetUniformLocation = [](GLuint, const GLchar*) -> GLint { return 1; };
        glad_glGenBuffers = [](GLsizei n, GLuint* ids) { while (n--) { *ids = ++*state.next; state.buffers->insert(*ids++); } };
        glad_glGenVertexArrays = [](GLsizei n, GLuint* ids) { while (n--) { *ids = ++*state.next; state.arrays->insert(*ids++); } };
        glad_glGenTextures = [](GLsizei n, GLuint* ids) { while (n--) { *ids = ++*state.next; state.textures->insert(*ids++); } };
        glad_glDeleteBuffers = [](GLsizei n, const GLuint* ids) { while (n--) { if (*ids && state.buffers->erase(*ids) != 1) throw std::runtime_error("Double buffer deletion"); ++ids; } };
        glad_glDeleteVertexArrays = [](GLsizei n, const GLuint* ids) { while (n--) { if (*ids && state.arrays->erase(*ids) != 1) throw std::runtime_error("Double VAO deletion"); ++ids; } };
        glad_glDeleteTextures = [](GLsizei n, const GLuint* ids) { while (n--) { if (*ids && state.textures->erase(*ids) != 1) throw std::runtime_error("Double texture deletion"); ++ids; } };
        glad_glBindVertexArray = [](GLuint) {};
        glad_glBindBuffer = [](GLenum, GLuint) {};
        glad_glBufferData = [](GLenum, GLsizeiptr, const void*, GLenum) { ++*state.bufferUploads; };
        glad_glVertexAttribPointer = [](GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {};
        glad_glEnableVertexAttribArray = [](GLuint) {};
        glad_glBindTexture = [](GLenum, GLuint) {};
        glad_glTexParameteri = [](GLenum, GLenum, GLint) {};
        glad_glTexImage2D = [](GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { ++*state.textureUploads; };
        glad_glGenerateMipmap = [](GLenum) {};
        glad_glGetError = []() -> GLenum { return ++*state.checks == *state.failAt ? GL_OUT_OF_MEMORY : GL_NO_ERROR; };
        glad_glUseProgram = [](GLuint) {};
        glad_glUniformMatrix4fv = [](GLint, GLsizei, GLboolean, const GLfloat*) {};
        glad_glUniform4fv = [](GLint, GLsizei, const GLfloat*) {};
        glad_glUniform1i = [](GLint, GLint) {};
        glad_glActiveTexture = [](GLenum) {};
        glad_glDrawElements = [](GLenum, GLsizei, GLenum, const void*) { ++*state.draws; };
        // Estados reales del contrato GL: detectar fugas de blend/depth/cull y bindings.
        struct GlState {
            std::map<GLenum,GLint> values{{GL_DEPTH_FUNC,GL_LESS},{GL_CULL_FACE_MODE,GL_BACK},{GL_FRONT_FACE,GL_CCW},
                {GL_BLEND_SRC_RGB,GL_ONE},{GL_BLEND_DST_RGB,GL_ZERO},{GL_BLEND_SRC_ALPHA,GL_ONE},{GL_BLEND_DST_ALPHA,GL_ZERO},
                {GL_BLEND_EQUATION_RGB,GL_FUNC_ADD},{GL_BLEND_EQUATION_ALPHA,GL_FUNC_ADD},{GL_ACTIVE_TEXTURE,GL_TEXTURE0}};
            std::set<GLenum> enabled{GL_DEPTH_TEST};
            GLboolean depthWrite = GL_TRUE;
            std::map<std::pair<GLint,GLenum>,GLuint> bindings;
            std::vector<bool> blendedDraws, depthWrites, culledDraws;
            std::vector<float> modelZ;
            float currentZ = 0;
        };
        static GlState gl;
        glad_glIsEnabled = [](GLenum flag) -> GLboolean { return gl.enabled.count(flag) ? GL_TRUE : GL_FALSE; };
        glad_glEnable = [](GLenum flag) { gl.enabled.insert(flag); };
        glad_glDisable = [](GLenum flag) { gl.enabled.erase(flag); };
        glad_glGetBooleanv = [](GLenum, GLboolean* v) { *v = gl.depthWrite; };
        glad_glGetIntegerv = [](GLenum key, GLint* v) {
            if (key == GL_TEXTURE_BINDING_2D || key == GL_TEXTURE_BINDING_CUBE_MAP)
                *v = gl.bindings[{gl.values[GL_ACTIVE_TEXTURE],key == GL_TEXTURE_BINDING_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP}];
            else *v = gl.values[key];
        };
        glad_glDepthMask = [](GLboolean v) { gl.depthWrite = v; };
        glad_glDepthFunc = [](GLenum v) { gl.values[GL_DEPTH_FUNC] = v; };
        glad_glCullFace = [](GLenum v) { gl.values[GL_CULL_FACE_MODE] = v; };
        glad_glFrontFace = [](GLenum v) { gl.values[GL_FRONT_FACE] = v; };
        glad_glBlendFuncSeparate = [](GLenum a,GLenum b,GLenum c,GLenum d) {
            gl.values[GL_BLEND_SRC_RGB]=a; gl.values[GL_BLEND_DST_RGB]=b;
            gl.values[GL_BLEND_SRC_ALPHA]=c; gl.values[GL_BLEND_DST_ALPHA]=d;
        };
        glad_glBlendEquationSeparate = [](GLenum a,GLenum b) { gl.values[GL_BLEND_EQUATION_RGB]=a; gl.values[GL_BLEND_EQUATION_ALPHA]=b; };
        glad_glUseProgram = [](GLuint v) { gl.values[GL_CURRENT_PROGRAM]=v; };
        glad_glBindVertexArray = [](GLuint v) { gl.values[GL_VERTEX_ARRAY_BINDING]=v; };
        glad_glBindBuffer = [](GLenum target,GLuint v) { if(target == GL_ARRAY_BUFFER) gl.values[GL_ARRAY_BUFFER_BINDING]=v; };
        glad_glActiveTexture = [](GLenum v) { gl.values[GL_ACTIVE_TEXTURE]=v; };
        glad_glBindTexture = [](GLenum target,GLuint v) { gl.bindings[{gl.values[GL_ACTIVE_TEXTURE],target}]=v; };
        glad_glUniformMatrix3fv = [](GLint,GLsizei,GLboolean,const GLfloat*) {};
        glad_glUniform3fv = [](GLint,GLsizei,const GLfloat*) {};
        glad_glUniform1f = [](GLint,GLfloat) {};
        glad_glUniformMatrix4fv = [](GLint location,GLsizei,GLboolean,const GLfloat* m) { if(location==2) gl.currentZ=m[14]; };
        glad_glGetUniformLocation = [](GLuint,const GLchar* name) -> GLint { return std::strcmp(name,"model")==0 ? 2 : 1; };
        glad_glDrawElements = [](GLenum,GLsizei,GLenum,const void*) {
            ++*state.draws;
            gl.blendedDraws.push_back(gl.enabled.count(GL_BLEND)); gl.depthWrites.push_back(gl.depthWrite);
            gl.culledDraws.push_back(gl.enabled.count(GL_CULL_FACE)); gl.modelZ.push_back(gl.currentZ);
        };
        glad_glDrawArrays = [](GLenum,GLint,GLsizei) {
            if (gl.depthWrite || gl.values[GL_DEPTH_FUNC]!=GL_LEQUAL || gl.enabled.count(GL_CULL_FACE) || gl.enabled.count(GL_BLEND))
                throw std::runtime_error("Skybox must use LEQUAL, no depth writes/cull/blend");
        };
        const auto sameState = [](const GlState& before) {
            // Consultas de GL pueden insertar ceros en el mock; comparar todos los valores efectivos.
            for(const auto& pair : gl.values) {
                auto it=before.values.find(pair.first);
                if(pair.second != (it==before.values.end()?0:it->second)) throw std::runtime_error("GL state leaked");
            }
            if(gl.enabled!=before.enabled || gl.depthWrite!=before.depthWrite) throw std::runtime_error("GL enable/depth write leaked");
            for(const auto& binding : gl.bindings) {
                auto it=before.bindings.find(binding.first);
                if(binding.second != (it==before.bindings.end()?0:it->second)) throw std::runtime_error("Texture binding leaked");
            }
        };
        glad_glGetUniformLocation = [](GLuint,const GLchar*) -> GLint { return -1; };
        for (bool skybox : {false,true}) {
            const auto before = deletedPrograms;
            bool rejected = false;
            try {
                if(skybox) { ecosphere::Environment environment(argv[1]); }
                else { ecosphere::ModelRenderer renderer(argv[1]); }
            } catch(const std::runtime_error&) { rejected = true; }
            if(!rejected || deletedPrograms!=before+1 || !textures.empty() || !arrays.empty())
                throw std::runtime_error("Missing uniforms must release linked program");
        }
        glad_glGetUniformLocation = [](GLuint,const GLchar* name) -> GLint { return std::strcmp(name,"model")==0 ? 2 : 1; };
        auto cpu = std::make_shared<ecosphere::Model>();
        ecosphere::Mesh mesh; mesh.vertices.resize(3); mesh.indices = {0,1,2}; mesh.texture = 0;
        cpu->meshes = {mesh, mesh};
        cpu->images.push_back({1, 1, {255,0,255,255}}); cpu->textures.emplace_back();
        ecosphere::ModelNode node; node.meshes = {0,1}; cpu->nodes = {node,node};
        for (failAt = 1; failAt <= 3; ++failAt) {
            checks = 0;
            ecosphere::ModelRenderer renderer(argv[1]);
            const auto savedState = gl;
            bool failed = false;
            try { renderer.draw(cpu, glm::mat4(1), glm::mat4(1), glm::mat4(1)); }
            catch (const std::runtime_error&) { failed = true; }
            sameState(savedState);
            if (!failed || !buffers.empty() || !arrays.empty() || !textures.empty())
                throw std::runtime_error("Partial GPU load must release every handle");
            const int savedFailure = failAt; failAt = 0; checks = 0;
            renderer.draw(cpu, glm::mat4(1), glm::mat4(1), glm::mat4(1));
            renderer.clear(); failAt = savedFailure;
            if (!buffers.empty() || !arrays.empty() || !textures.empty()) throw std::runtime_error("GPU retry cleanup");
        }
        failAt = 0; checks = drawCalls = bufferUploads = textureUploads = 0;
        {
            ecosphere::ModelRenderer renderer(argv[1]);
            renderer.draw(cpu, glm::mat4(1), glm::mat4(1), glm::mat4(1));
            renderer.draw(cpu, glm::mat4(1), glm::mat4(1), glm::mat4(1));
            if (bufferUploads != 4 || textureUploads != 1 || drawCalls != 8 || buffers.size() != 4 || arrays.size() != 2 || textures.size() != 1)
                throw std::runtime_error("Shared nodes/repeated draw must reuse GPU uploads");
        }
        if (!buffers.empty() || !arrays.empty() || !textures.empty()) throw std::runtime_error("GPU destructor leak");
        // Estado prestado no predeterminado: restaurar valores y bindings exactos.
        gl.enabled={GL_BLEND,GL_CULL_FACE}; gl.depthWrite=GL_FALSE;
        gl.values[GL_DEPTH_FUNC]=GL_GREATER; gl.values[GL_FRONT_FACE]=GL_CW; gl.values[GL_CULL_FACE_MODE]=GL_FRONT;
        gl.values[GL_BLEND_SRC_RGB]=GL_ONE; gl.values[GL_BLEND_DST_RGB]=GL_ONE;
        gl.values[GL_CURRENT_PROGRAM]=77; gl.values[GL_VERTEX_ARRAY_BINDING]=78; gl.values[GL_ARRAY_BUFFER_BINDING]=79;
        gl.values[GL_ACTIVE_TEXTURE]=GL_TEXTURE2;
        gl.bindings[{GL_TEXTURE0,GL_TEXTURE_2D}]=80; gl.bindings[{GL_TEXTURE1,GL_TEXTURE_CUBE_MAP}]=81;
        // Fallo en cada cara y en VAO final: cubemap parcial no deja handles.
        for (failAt=1; failAt<=7; ++failAt) {
            checks=0; const auto before=gl;
            bool failed=false;
            try { ecosphere::Environment environment(argv[1]); } catch(const std::runtime_error&) { failed=true; }
            sameState(before);
            if(!failed || !textures.empty() || !arrays.empty()) throw std::runtime_error("Cubemap partial upload leak");
        }
        failAt=0; checks=0;
        {
            const auto before=gl;
            ecosphere::Environment environment(argv[1]);
            environment.draw(glm::mat4(1),glm::mat4(1)); sameState(before);
        }
        if(!textures.empty() || !arrays.empty()) throw std::runtime_error("Cubemap destructor leak");
        {
            auto transparent=std::make_shared<ecosphere::Model>(*cpu);
            transparent->meshes[0].material.alphaMode=ecosphere::AlphaMode::blend;
            transparent->meshes[0].material.doubleSided=true;
            transparent->nodes[0].local[3].z=-1; transparent->nodes[1].local[3].z=-8;
            ecosphere::LightingSettings lighting; lighting.legacyUnlit=false;
            ecosphere::ModelRenderer renderer(argv[1]);
            const auto before=gl;
            gl.blendedDraws.clear(); gl.depthWrites.clear(); gl.culledDraws.clear(); gl.modelZ.clear();
            renderer.draw(transparent,glm::mat4(1),glm::mat4(1),glm::mat4(1),lighting);
            sameState(before);
            if(gl.blendedDraws != std::vector<bool>({false,false,true,true}) ||
               gl.depthWrites != std::vector<bool>({true,true,false,false}) ||
               gl.culledDraws != std::vector<bool>({true,true,false,false}) || gl.modelZ[2]!=-8 || gl.modelZ[3]!=-1)
                throw std::runtime_error("Opaque first, sorted blend without writes, doubleSided culling");
            // Error durante dibujo (después de cambiar estados) debe restablecerlos.
            transparent->meshes[0].texture=999;
            bool failed=false;
            try { renderer.draw(transparent,glm::mat4(1),glm::mat4(1),glm::mat4(1),lighting); }
            catch(const std::out_of_range&) { failed=true; }
            sameState(before);
            if(!failed) throw std::runtime_error("Bad texture reference accepted");
            transparent->meshes[0].texture=0;
            auto farModel=std::make_shared<ecosphere::Model>(*transparent);
            gl.blendedDraws.clear(); gl.modelZ.clear();
            const auto farPlacement=glm::translate(glm::mat4(1),glm::vec3(0,0,-20));
            renderer.drawModels({{transparent,glm::mat4(1)},{farModel,farPlacement}},glm::mat4(1),glm::mat4(1),lighting,{},0);
            sameState(before);
            if(gl.blendedDraws!=std::vector<bool>({false,false,false,false,true,true,true,true}) ||
               gl.modelZ[4]!=-28 || gl.modelZ[5]!=-21 || gl.modelZ[6]!=-8 || gl.modelZ[7]!=-1)
                throw std::runtime_error("BLEND must sort across models in a shared batch");
        }
        if(!textures.empty() || !arrays.empty() || !buffers.empty()) throw std::runtime_error("Final GPU handle leak");
        // Fase 4 reutiliza el pipeline: fallo en cada malla nueva, reintento y cierre.
        for (int meshFailure=1; meshFailure<=8; ++meshFailure) {
            ecosphere::InteractionAnimationDemo demo(*cpu);
            demo.interaction.select("simple-rotation"); demo.synchronize();
            ecosphere::ModelRenderer renderer(argv[1]);
            const auto before=gl;
            checks=0; failAt=meshFailure;
            bool failed=false;
            try { renderer.drawModels({{demo.cubes,glm::mat4(1)},{demo.outline,glm::mat4(1)}},glm::mat4(1),glm::mat4(1),{}, {},0); }
            catch(const std::runtime_error&) { failed=true; }
            sameState(before);
            if(!failed) throw std::runtime_error("Phase 4 injected upload failure not detected");
            renderer.clear();
            if(!textures.empty() || !arrays.empty() || !buffers.empty()) throw std::runtime_error("Phase 4 partial GPU leak");
            failAt=0; checks=0;
            renderer.drawModels({{demo.cubes,glm::mat4(1)},{demo.outline,glm::mat4(1)}},glm::mat4(1),glm::mat4(1),{}, {},0);
            renderer.clear(); sameState(before);
            if(!textures.empty() || !arrays.empty() || !buffers.empty()) throw std::runtime_error("Phase 4 GPU retry leak");
        }
        std::cout << "Phase 4 GPU simulated: seven cube uploads + outline failure/retry/cleanup PASS\n";
        std::cout << "Phase 3 GPU: six cubemap faces/rollback, skybox state, sorted blend/depth/cull, exception restoration PASS\n";
        std::cout << "GPU assets: reuse, mesh/texture partial failures, retry and exact handle cleanup PASS (simulated GL)\n";
        std::cout << "Triangle + SceneRenderer + ModelRenderer: compile/link, diagnostic exceptions, cleanup and missing file PASS (simulated GL)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
