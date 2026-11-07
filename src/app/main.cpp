#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/modules/Stubs.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <glm/common.hpp>
#include "SceneRenderer.hpp"
#include "ModelRenderer.hpp"
#include "Environment.hpp"
#include "LightingSmoke.hpp"
#include "InteractionSmoke.hpp"
#include "InteractionDemoInput.hpp"
#include "ContentSmoke.hpp"
#include "content/ContentRegistry.hpp"
#include "world/Plaza.hpp"
#include "PlazaSmoke.hpp"
#include "EcoEnergySmoke.hpp"
#include "AquaBioSmoke.hpp"
#include "EcoCycleSmoke.hpp"
#include "NavigationHud.hpp"
#include "FpsCounter.hpp"
#include "LightingControls.hpp"
#include "SrgbFramebuffer.hpp"
#include "NavigationSmoke.hpp"
#include "world/ZoneNavigator.hpp"
#include "world/LightingDemo.hpp"
#include "world/LightingDemoSettings.hpp"
#include "assets/AssetManager.hpp"
#include "camera/Camera.hpp"
#include "world/DemoSceneSettings.hpp"
#include <filesystem>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr int smokeFrameLimit = 30;
struct GlfwLifetime {
    GlfwLifetime() {
        glfwSetErrorCallback([](int code, const char* message) {
            std::cerr << "GLFW " << code << ": " << message << '\n';
        });
        if (!glfwInit()) {
            throw std::runtime_error("GLFW initialization failed; check Wayland/X11 and driver");
        }
    }
    ~GlfwLifetime() { glfwTerminate(); }
};

class DemoServices final : public ecosphere::ModuleServices {
public:
    ecosphere::ContentRegistry* registry = nullptr;
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override {
        return registry ? registry->open(owner) : nullptr;
    }
    void log(std::string_view message) noexcept override {
        std::cout << "[module] " << message << std::endl;
    }
    void setClearColor(glm::vec3 value) noexcept override {
        clearColor = glm::clamp(value, glm::vec3(0.0f), glm::vec3(1.0f));
    }
    glm::vec3 clearColor{0.0f};
};

void captureAndCheck(const std::string& path, int width, int height, glm::vec3 clearColor, const std::vector<unsigned char>& withoutImported) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL readback failed");
    }
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_DEPTH_TEST)) {
        throw std::runtime_error("Smoke failed: depth readback/test unavailable");
    }
    // Fondo arriba a la izquierda; cada objeto debe aportar píxeles con profundidad.
    const auto sky = static_cast<std::size_t>(height - 1) * width;
    for (int channel = 0; channel < 3; ++channel) {
        if (std::abs(static_cast<int>(pixels[sky * 3 + channel]) -
                     static_cast<int>(std::lround(clearColor[channel] * 255))) > 3) {
            throw std::runtime_error("Smoke failed: module background missing");
        }
    }
    const std::array<glm::vec3, 5> colors{ecosphere::demo::groundColor, ecosphere::demo::redColor,
        ecosphere::demo::blueColor, ecosphere::demo::parentColor, ecosphere::demo::childColor};
    std::array<std::size_t, 5> counts{};
    float nearest = 1.0f, farthest = 0.0f;
    for (std::size_t pixel = 0; pixel < depth.size(); ++pixel) {
        if (!std::isfinite(depth[pixel]) || depth[pixel] < 0 || depth[pixel] > 1) {
            throw std::runtime_error("Smoke failed: invalid depth value");
        }
        if (depth[pixel] >= 1.0f) continue;
        nearest = std::min(nearest, depth[pixel]);
        farthest = std::max(farthest, depth[pixel]);
        for (std::size_t object = 0; object < colors.size(); ++object) {
            bool matchesFace = false;
            for (float tint : {ecosphere::demo::frontBackTint, ecosphere::demo::leftRightTint,
                               ecosphere::demo::topBottomTint}) {
                bool matches = true;
                for (int channel = 0; channel < 3; ++channel) {
                    matches &= std::abs(static_cast<int>(pixels[pixel * 3 + channel]) -
                        static_cast<int>(std::lround(colors[object][channel] * tint * 255))) <= 3;
                }
                matchesFace |= matches;
            }
            if (matchesFace) ++counts[object];
        }
    }
    for (const auto count : counts) {
        if (count < 10) throw std::runtime_error("Smoke failed: a demo object is missing");
    }
    if (depth[sky] != 1.0f || farthest - nearest < 0.001f) {
        throw std::runtime_error("Smoke failed: foreground/background depth separation missing");
    }
    if (withoutImported.size() != pixels.size()) throw std::runtime_error("Smoke missing imported baseline");
    std::size_t changed = 0, magenta = 0, cyan = 0;
    for (std::size_t p = 0; p < depth.size(); ++p) {
        const auto r = pixels[p*3], g = pixels[p*3+1], b = pixels[p*3+2];
        if (r != withoutImported[p*3] || g != withoutImported[p*3+1] || b != withoutImported[p*3+2]) {
            if (depth[p] >= 1.0f) throw std::runtime_error("Imported pixels lack depth");
            ++changed;
            if (r > 240 && b > 240 && g < 10) ++magenta;
            if (g > 240 && b > 240 && r < 10) ++cyan;
        }
    }
    if (changed < 100 || magenta < 10 || cyan < 10)
        throw std::runtime_error("Smoke failed: imported geometry/UV checker missing");
    std::cout << "IMPORTED SMOKE PASS: changed=" << changed << "; magenta=" << magenta << "; cyan=" << cyan << '\n';
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    const auto stride = static_cast<std::size_t>(width) * 3;
    // OpenGL empieza abajo; PPM almacena la primera fila arriba.
    for (int y = height - 1; y >= 0; --y) {
        output.write(reinterpret_cast<const char*>(pixels.data() + y * stride),
                     static_cast<std::streamsize>(stride));
    }
    output.close();
    std::cout << "SMOKE PASS: object pixels (ground/red/blue/parent/child)=";
    for (const auto count : counts) std::cout << count << ' ';
    std::cout << "; depth range=" << nearest << ".." << farthest << "; capture=" << path << '\n';
}

std::filesystem::path executableDirectory(const char* argv0) {
#if defined(__linux__)
    (void)argv0;
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#elif defined(_WIN32)
    (void)argv0;
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) throw std::runtime_error("Cannot locate executable");
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#else
    return std::filesystem::canonical(argv0).parent_path();
#endif
}

int run(int argc, char** argv) {
    std::string_view selectedModuleId = "ecoenergy";
    std::string capturePath, lightingCaptureDirectory, interactionCaptureDirectory, contentCaptureDirectory, plazaCaptureDirectory;
    std::string ecoEnergyCaptureDirectory, aquaBioCaptureDirectory, ecoCycleCaptureDirectory, navigationCaptureDirectory;
    bool integralValidation=false,uncappedSmoke=false;
    bool plazaEnabled = false, moduleExplicit = false;
    bool contentEnabled = false;
    bool interactionEnabled = false;
    bool regression = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--module" && i + 1 < argc) {
            moduleExplicit = true;
            selectedModuleId = argv[++i];
        } else if (option == "--smoke" && i + 1 < argc) {
            capturePath = argv[++i];
            if (capturePath.empty()) throw std::invalid_argument("Capture path must not be empty");
        } else if (option == "--lighting-smoke" && i + 1 < argc) {
            lightingCaptureDirectory = argv[++i];
            if (lightingCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--interaction") {
            interactionEnabled = true;
        } else if (option == "--interaction-smoke" && i + 1 < argc) {
            interactionEnabled = true; interactionCaptureDirectory = argv[++i];
            if (interactionCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--content") {
            contentEnabled = true;
        } else if (option == "--content-smoke" && i + 1 < argc) {
            contentEnabled = true; contentCaptureDirectory = argv[++i];
            if (contentCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--ecoenergy-smoke" && i + 1 < argc) {
            contentEnabled = true; ecoEnergyCaptureDirectory = argv[++i];
            if (ecoEnergyCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--aquabio-smoke" && i + 1 < argc) {
            contentEnabled = true; aquaBioCaptureDirectory = argv[++i]; selectedModuleId = "aquabio";
            if (aquaBioCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if(option=="--ecocycle-smoke" && i+1<argc) {
            contentEnabled=true;ecoCycleCaptureDirectory=argv[++i];selectedModuleId="ecocycle";
            if(ecoCycleCaptureDirectory.empty())throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--plaza") {
            plazaEnabled = true;
        } else if (option == "--plaza-smoke" && i + 1 < argc) {
            plazaEnabled = true; plazaCaptureDirectory = argv[++i];
            if (plazaCaptureDirectory.empty()) throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--navigation-smoke" && i + 1 < argc) {
            plazaEnabled=true;navigationCaptureDirectory=argv[++i];
            if(navigationCaptureDirectory.empty())throw std::invalid_argument("Capture directory must not be empty");
        } else if (option == "--validation-smoke" && i+1<argc) {
            plazaEnabled=true;integralValidation=true;navigationCaptureDirectory=argv[++i];
            if(navigationCaptureDirectory.empty())throw std::invalid_argument("Capture directory must not be empty");
        } else if(option=="--benchmark-no-vsync") {
            uncappedSmoke=true;
        } else if (option == "--regression") {
            regression = true;
        } else {
            throw std::invalid_argument("Usage: ecosphere [--module ecoenergy|aquabio|ecocycle] [--smoke output.ppm] [--lighting-smoke directory] [--regression] [--interaction | --interaction-smoke directory] [--content | --content-smoke directory | --ecoenergy-smoke directory | --aquabio-smoke directory | --ecocycle-smoke directory] [--plaza | --plaza-smoke directory | --navigation-smoke directory | --validation-smoke directory] [--benchmark-no-vsync]");
        }
    }
    if (!lightingCaptureDirectory.empty() && (!capturePath.empty() || regression))
        throw std::invalid_argument("Lighting smoke requires illuminated mode without legacy smoke");
    if (interactionEnabled && (!capturePath.empty() || regression || !lightingCaptureDirectory.empty()))
        throw std::invalid_argument("Interaction demo requires its own mode, without historical smoke");
    regression = regression || !capturePath.empty();
    if (contentEnabled && (regression || interactionEnabled || !lightingCaptureDirectory.empty()))
        throw std::invalid_argument("Content demo requires its own mode, without historical demos");
    if (!ecoEnergyCaptureDirectory.empty() && (moduleExplicit || !contentCaptureDirectory.empty() || !aquaBioCaptureDirectory.empty() || !ecoCycleCaptureDirectory.empty() || plazaEnabled))
        throw std::invalid_argument("EcoEnergy smoke runs standalone on ecoenergy without other modes");
    if (!aquaBioCaptureDirectory.empty() && (moduleExplicit || !contentCaptureDirectory.empty() || !ecoEnergyCaptureDirectory.empty() || !ecoCycleCaptureDirectory.empty() || plazaEnabled))
        throw std::invalid_argument("AquaBio smoke runs standalone on aquabio without other modes");
    if(!ecoCycleCaptureDirectory.empty()&&(moduleExplicit||!contentCaptureDirectory.empty()||!ecoEnergyCaptureDirectory.empty()||!aquaBioCaptureDirectory.empty()||plazaEnabled))
        throw std::invalid_argument("EcoCycle smoke runs standalone on ecocycle without other modes");
    if (plazaEnabled && (moduleExplicit || contentEnabled || regression || interactionEnabled || !lightingCaptureDirectory.empty()))
        throw std::invalid_argument("Plaza requires its own mode, without modules or historical demos");
    if(!plazaCaptureDirectory.empty() && !navigationCaptureDirectory.empty())
        throw std::invalid_argument("Choose one plaza/navigation capture mode");
    if(uncappedSmoke && navigationCaptureDirectory.empty() && ecoEnergyCaptureDirectory.empty() && aquaBioCaptureDirectory.empty() && ecoCycleCaptureDirectory.empty())
        throw std::invalid_argument("No-VSync measurement requires navigation/validation/ecoenergy/aquabio smoke");
    if (plazaEnabled) { contentEnabled = true; selectedModuleId = "hub"; }
    auto lighting = ecosphere::demo::lighting;
    using Factory = std::unique_ptr<ecosphere::IModule> (*)();
    const std::array<Factory, 3> factories = {
        ecosphere::makeEcoEnergy, ecosphere::makeAquaBio, ecosphere::makeEcoCycle
    };
    std::unique_ptr<ecosphere::IModule> module;
    if (plazaEnabled) module = ecosphere::makePlaza();
    else for (auto factory : factories) {
        auto candidate = factory();
        if (candidate->id() == selectedModuleId) module = std::move(candidate);
    }
    if (!module) throw std::invalid_argument("Unknown module: " + std::string(selectedModuleId));

    GlfwLifetime glfw;
    // Perfil Core: no se permite usar las operaciones antiguas de OpenGL.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    const std::string title = plazaEnabled ? "EcoSphere - Plaza Central" : std::string(contentEnabled ? "EcoSphere Phase 5A - " : interactionEnabled ? "EcoSphere Phase 4 - " : "EcoSphere Phase 3 - ") + std::string(selectedModuleId);
    const bool hdWindow = plazaEnabled || !ecoEnergyCaptureDirectory.empty() || !aquaBioCaptureDirectory.empty() || !ecoCycleCaptureDirectory.empty();
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
        glfwCreateWindow(hdWindow ? ecosphere::plaza::settings.windowWidth : ecosphere::demo::windowWidth,
            hdWindow ? ecosphere::plaza::settings.windowHeight : ecosphere::demo::windowHeight, title.c_str(), nullptr, nullptr), glfwDestroyWindow);
    if (!window) throw std::runtime_error("OpenGL 3.3 Core window creation failed");
    glfwMakeContextCurrent(window.get());
    // Cargar funciones solo después de activar un contexto válido.
    const int version = gladLoadGL(glfwGetProcAddress);
    if (!version || !GLAD_GL_VERSION_3_3) {
        throw std::runtime_error("GLAD 2 could not load OpenGL 3.3");
    }
    GLint profile = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (!(profile & GL_CONTEXT_CORE_PROFILE_BIT)) {
        throw std::runtime_error("Driver did not provide Core profile");
    }
    std::cout << "Native Wayland: " << (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "yes" : "no")
              << "\nX11: " << (glfwGetPlatform() == GLFW_PLATFORM_X11 ? "yes" : "no")
              << "\nGLFW: " << glfwGetVersionString()
              << "\nOpenGL: " << glGetString(GL_VERSION)
              << "\nVendor: " << glGetString(GL_VENDOR)
              << "\nRenderer: " << glGetString(GL_RENDERER)
              << "\nGLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
              << "\nCore profile verified; selected=" << selectedModuleId << std::endl;
    glfwSwapInterval(uncappedSmoke ? 0 : 1);
    std::cout<<"Requested swap interval: "<<(uncappedSmoke ? 0 : 1)<<std::endl;
    ecosphere::Camera camera;
    if (plazaEnabled) {
        camera.position = ecosphere::plaza::settings.cameraStart;
        camera.yawDegrees = ecosphere::plaza::settings.cameraYaw;
        camera.pitchDegrees = ecosphere::plaza::settings.cameraPitch;
    } else if (contentEnabled && selectedModuleId == "aquabio") {
        camera = ecosphere::ZoneSettings{}.entries[2].make();
    } else if(contentEnabled && selectedModuleId=="ecocycle") {
        camera=ecosphere::ZoneSettings{}.entries[3].make();
    } else if (!ecoEnergyCaptureDirectory.empty()) {
        camera.position = {0.0f, 1.7f, 6.0f};
        camera.yawDegrees = -90.0f;
        camera.pitchDegrees = -4.0f;
    }
    ecosphere::FpsCounter fpsCounter;
    ecosphere::LightingControls lightControls;
    struct InputContext {
        ecosphere::FpsCounter& fps;
        ecosphere::LightingControls& lightControls;
        ecosphere::Camera& camera;
        ecosphere::LightingSettings& lighting;
        bool& regression;
        bool interactionEnabled;
        std::vector<int> interactionKeys;
        bool skipNextUpdate = true;
        bool navigationEnabled = false, enter = false, home = false;
        std::vector<std::pair<int,bool>> navigationKeys{};
    } inputContext{fpsCounter, lightControls, camera, lighting, regression, interactionEnabled || contentEnabled, {}};
    inputContext.navigationEnabled=plazaEnabled;
    glfwSetWindowUserPointer(window.get(), &inputContext);
    glfwSetKeyCallback(window.get(), [](GLFWwindow* handle, int key, int, int action, int) {
        auto& input = *static_cast<InputContext*>(glfwGetWindowUserPointer(handle));
        input.fps.key(key,action);
        if(key==GLFW_KEY_F3 && action==GLFW_PRESS)
            std::cout<<"FPS overlay: "<<(input.fps.visible ? "on" : "off")<<std::endl;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            std::cout << "ESC: close requested" << std::endl;
            glfwSetWindowShouldClose(handle, GLFW_TRUE);
        }
        if (action == GLFW_PRESS && (key == GLFW_KEY_F1 || key == GLFW_KEY_F2)) {
            input.camera.setMode(key == GLFW_KEY_F1 ? ecosphere::CameraMode::firstPerson : ecosphere::CameraMode::free);
            std::cout << "Camera mode: " << (key == GLFW_KEY_F1 ? "first-person" : "free") << std::endl;
        }
        const bool lightingChanged=input.lightControls.key(key,action,input.lighting);
        if(key==GLFW_KEY_6 && action==GLFW_PRESS)input.regression=!input.regression;
        if(lightingChanged || (key==GLFW_KEY_6 && action==GLFW_PRESS)) {
            const auto& s=input.lighting;
            std::cout<<"Lighting state: direct="<<s.directEnabled<<" directional="<<s.directional.enabled
                     <<" point="<<s.point.enabled<<" reflection="<<s.environment.reflectionEnabled
                     <<" skybox="<<s.environment.skyboxEnabled<<" regression="<<input.regression
                     <<" HUD="<<input.lightControls.visible<<" shading="<<ecosphere::shadingName(input.lightControls.shading)<<std::endl;
        }
        if (input.interactionEnabled && action == GLFW_PRESS && glfwGetWindowAttrib(handle, GLFW_FOCUSED))
            input.interactionKeys.push_back(key);
        if (input.navigationEnabled && (key==GLFW_KEY_ENTER || key==GLFW_KEY_H) &&
            (action==GLFW_PRESS || action==GLFW_RELEASE) && glfwGetWindowAttrib(handle,GLFW_FOCUSED))
            input.navigationKeys.emplace_back(key,action==GLFW_PRESS);
        if (action == GLFW_RELEASE) {
            const auto& c = input.camera;
            std::cout << "Camera state: position=" << c.position.x << ',' << c.position.y << ',' << c.position.z
                      << " yaw=" << c.yawDegrees << " pitch=" << c.pitchDegrees << std::endl;
        }
    });
    glfwSetWindowFocusCallback(window.get(), [](GLFWwindow* handle, int focused) {
        auto& input = *static_cast<InputContext*>(glfwGetWindowUserPointer(handle));
        input.skipNextUpdate = true;
        input.navigationKeys.clear();input.enter=input.home=false;
        std::cout << "Focus: " << (focused ? "active" : "paused") << std::endl;
    });
    glfwSetWindowIconifyCallback(window.get(), [](GLFWwindow* handle, int iconified) {
        auto& input = *static_cast<InputContext*>(glfwGetWindowUserPointer(handle));
        input.skipNextUpdate = true;
        std::cout << "Minimized: " << (iconified ? "yes" : "no") << std::endl;
    });
    glfwSetFramebufferSizeCallback(window.get(), [](GLFWwindow*, int width, int height) {
        std::cout << "Framebuffer: " << width << 'x' << height << std::endl;
    });
    std::cout << "Camera mode: first-person\nControls: F1 first-person; F2 free; F3 FPS; F4 lighting HUD; 0 restore zone lighting; F5 Phong/Blinn-Phong; WASD move; "
                 "Q/E descend/ascend (free); arrows look; ESC exit; 1 direct, 2 directional, 3 point, 4 reflection, 5 skybox, 6 regression. Cursor stays released." << std::endl;

    // Destrucción en orden inverso: recursos GPU y módulo antes del contexto.
    ecosphere::ContentRegistry legacyContentRegistry(executableDirectory(argv[0]) / "assets");
    const double contentLoadStart = glfwGetTime();
    std::unique_ptr<ecosphere::ZoneNavigator> navigator;
    if (plazaEnabled) navigator=std::make_unique<ecosphere::ZoneNavigator>(executableDirectory(argv[0]) / "assets");
    auto& contentRegistry=navigator ? navigator->registry() : legacyContentRegistry;
    DemoServices services;
    if (contentEnabled) services.registry = &contentRegistry;
    std::unique_ptr<ecosphere::ModuleSession> session;
    if (!navigator) session=std::make_unique<ecosphere::ModuleSession>(std::move(module),services);
    if (navigator) {
        camera=navigator->entryCamera();module.reset();
        std::cout<<"Navigation: Enter near an access to enter; H return to plaza; ESC exit. Release Enter/H before another transition.\n";
    }
    const double contentLoadSeconds = glfwGetTime() - contentLoadStart;
    if (contentEnabled) {
        lighting = contentRegistry.lighting(std::string(selectedModuleId)).value_or(lighting);
        std::cout << "Content controls: Space select; Tab next; Backspace deselect; J/L, U/O, I/K translate; Z/X rotate; C/V scale; G take/drop.\n";
    }
    lightControls.defaults(navigator ? navigator->lighting() : lighting);
    ecosphere::Scene scene;
    const auto shaders = executableDirectory(argv[0]) / "shaders";
    std::cout << "Shaders: " << shaders.string() << std::endl;
    ecosphere::SceneRenderer renderer(shaders);
    ecosphere::AssetManager assets(executableDirectory(argv[0]) / "assets");
    std::shared_ptr<const ecosphere::Model> imported;
    if (!plazaEnabled) {
        imported = assets.loadModel(ecosphere::demo::importedModel);
        for (const auto& warning : imported->warnings) std::cout << "Asset notice: " << warning << '\n';
        std::cout << "Imported model: " << imported->meshes.size() << " primitives, " << imported->nodes.size()
                  << " nodes, " << imported->textures.size() << " textures" << std::endl;
    }
    auto modelRenderer=std::make_unique<ecosphere::ModelRenderer>(shaders);
    std::unique_ptr<ecosphere::NavigationHud> navigationHud;
    if (navigator) navigationHud=std::make_unique<ecosphere::NavigationHud>(shaders);
    ecosphere::NavigationHud fpsHud(shaders),lightingHud(shaders);
    ecosphere::Environment environment(shaders);
    const auto showcase = plazaEnabled ? nullptr : ecosphere::makeLightingDemo();
    std::shared_ptr<ecosphere::Model> contentOutline;
    if (contentEnabled) {
        contentOutline = std::make_shared<ecosphere::Model>();
        auto edge = ecosphere::makeUnitCube(); edge.material.unlit = true; edge.color = {1, 0.8f, 0.02f, 1};
        contentOutline->meshes.push_back(std::move(edge));
        for (int i = 0; i < 12; ++i) { ecosphere::ModelNode node; node.meshes = {0}; contentOutline->nodes.push_back(node); }
    }
    std::unique_ptr<ecosphere::InteractionAnimationDemo> interactionDemo;
    if (interactionEnabled) {
        interactionDemo = std::make_unique<ecosphere::InteractionAnimationDemo>(*imported);
        std::cout << "Interaction controls: Space center ray; Tab next; Backspace deselect; J/L X, U/O Y, I/K Z; "
                     "Z/X rotate; C/V scale; G take/drop; P pause/resume; R restart animations. Cursor released." << std::endl;
    }
    ecosphere::verifySrgbFramebuffer();
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    double performanceStart = glfwGetTime();
    int performanceFrames = 0;
    double previousTimeSeconds = glfwGetTime();
    int renderedFrames = 0;
    while (!glfwWindowShouldClose(window.get())) {
        glfwPollEvents();
        const double currentTimeSeconds = glfwGetTime();
        const double elapsed = currentTimeSeconds - previousTimeSeconds;
        if (session) session->update(elapsed); // Los modos históricos conservan segundos reales.
        const bool focused = glfwGetWindowAttrib(window.get(), GLFW_FOCUSED) == GLFW_TRUE;
        const bool minimized = glfwGetWindowAttrib(window.get(), GLFW_ICONIFIED) == GLFW_TRUE;
        const double cameraDelta = ecosphere::cameraDeltaSeconds(elapsed,
            focused && !minimized && !inputContext.skipNextUpdate);
        const bool activeInput = focused && !minimized && !inputContext.skipNextUpdate;
        fpsCounter.sample(currentTimeSeconds,activeInput);
        inputContext.skipNextUpdate = false;
        const auto held = [&](int key) { return glfwGetKey(window.get(), key) == GLFW_PRESS ? 1.0f : 0.0f; };
        ecosphere::CameraInput input;
        if (focused && !minimized) {
            input.forward = held(GLFW_KEY_W) - held(GLFW_KEY_S);
            input.right = held(GLFW_KEY_D) - held(GLFW_KEY_A);
            input.vertical = held(GLFW_KEY_E) - held(GLFW_KEY_Q);
            input.yaw = held(GLFW_KEY_RIGHT) - held(GLFW_KEY_LEFT);
            input.pitch = held(GLFW_KEY_UP) - held(GLFW_KEY_DOWN);
        }
        camera.update(input, cameraDelta);
        previousTimeSeconds = currentTimeSeconds;
        int width = 0, height = 0;
        glfwGetFramebufferSize(window.get(), &width, &height);
        const auto projection = camera.projection(width, height);
        if (!projection || minimized) {
            inputContext.interactionKeys.clear();
            if (navigator) navigator->cancelInput();
            glfwWaitEventsTimeout(0.05);
            continue;
        }
        const auto navigate=[&](glm::vec3 position,bool enter,bool home) {
            std::unique_ptr<ecosphere::ModelRenderer> prepared;
            bool attemptedGpu=false;
            const auto prepare=[&](const auto& content,const auto& lights,const auto& entry) {
                attemptedGpu=true;
                prepared=std::make_unique<ecosphere::ModelRenderer>(shaders);
                std::vector<ecosphere::ModelPlacement> models;
                for (const auto& p:content) models.push_back({p.model,p.world});
                prepared->setShadingModel(lightControls.shading);
                prepared->drawModels(models,entry.view(),*projection,lights,entry.position,environment.texture());
                if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Zone GPU preparation failed");
            };
            const bool changed=navigator->input(position,enter,home,prepare);
            if(!changed && attemptedGpu) {
                // Los errores del upload fallido ya se reportaron; no contaminar el dibujo saliente.
                for(int i=0;i<8;++i) {
                    const GLenum diagnostic=glGetError();if(diagnostic==GL_NO_ERROR)break;
                    std::cerr<<"Rejected candidate GL diagnostic: "<<diagnostic<<std::endl;
                }
            }
            if(!changed && (enter || home) && !navigator->error().empty())
                std::cerr<<"Zone transition rejected: "<<navigator->error()<<std::endl;
            if (changed) {
                modelRenderer.swap(prepared); // Destruir caché saliente con el contexto aún activo.
                camera=navigator->entryCamera();lighting=navigator->lighting();
                lightControls.defaults(lighting);
                selectedModuleId=ecosphere::zoneId(navigator->active());
                inputContext.interactionKeys.clear();previousTimeSeconds=glfwGetTime();
                inputContext.enter=held(GLFW_KEY_ENTER)>0;inputContext.home=held(GLFW_KEY_H)>0;
                std::cout<<"Zone active: "<<selectedModuleId<<"; entities="<<navigator->placements().size()<<std::endl;
            }
            return changed;
        };
        bool transitioned=false;
        if (navigator) {
            if (!activeInput) { navigator->cancelInput();inputContext.navigationKeys.clear();inputContext.enter=inputContext.home=false; }
            if (activeInput) {
                for(const auto& event:inputContext.navigationKeys) {
                    if(event.first==GLFW_KEY_ENTER) inputContext.enter=event.second; else inputContext.home=event.second;
                    transitioned=navigate(camera.position,inputContext.enter,inputContext.home);
                    if(transitioned) break;
                }
                if(!transitioned) navigator->input(camera.position,inputContext.enter,inputContext.home);
            }
            inputContext.navigationKeys.clear();
            navigator->update(activeInput && contentCaptureDirectory.empty() ? elapsed : 0);
        }
        if (interactionDemo && !regression) {
            interactionDemo->update(activeInput && interactionCaptureDirectory.empty() ? elapsed : 0);
            if (activeInput) ecosphere::updateInteractionInput(*interactionDemo,camera,*projection,inputContext.interactionKeys,held,cameraDelta);
            inputContext.interactionKeys.clear();
            interactionDemo->synchronize();
        } else if (contentEnabled && !regression) {
            if (!navigator) contentRegistry.update(activeInput && contentCaptureDirectory.empty() && ecoEnergyCaptureDirectory.empty() && aquaBioCaptureDirectory.empty() && ecoCycleCaptureDirectory.empty() ? elapsed : 0);
            if (activeInput && !transitioned) {
                auto& objects = contentRegistry.interactions(std::string(selectedModuleId));
                ecosphere::updateContentInput(contentRegistry,std::string(selectedModuleId),camera,*projection,inputContext.interactionKeys,held,cameraDelta);
                if (!inputContext.interactionKeys.empty())
                    std::cout << "Content selection: " << selectedModuleId << '/' << objects.selected().value_or("none")
                              << " held=" << objects.holding() << std::endl;
            }
            inputContext.interactionKeys.clear();
        } else inputContext.interactionKeys.clear();
        glViewport(0, 0, width, height);
        const auto clear=navigator ? navigator->clearColor() : services.clearColor;
        glClearColor(clear.r,clear.g,clear.b,1);
        const auto renderLit = [&](const ecosphere::LightingSettings& settings, bool includeContent = true,
                                   const std::optional<std::string>& onlyEntity = std::nullopt) {
            modelRenderer->setShadingModel(lightControls.shading);
            glEnable(GL_FRAMEBUFFER_SRGB);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!contentEnabled) renderer.drawLit(*modelRenderer, scene, camera.view(), *projection, settings, camera.position, environment.texture());
            if (settings.environment.skyboxEnabled) environment.draw(camera.view(), *projection);
            std::vector<ecosphere::ModelPlacement> models;
            if (contentEnabled) {
                if (includeContent) {
                    for (const auto& entity : contentRegistry.placements())
                        if (!onlyEntity || entity.id == *onlyEntity) models.push_back({entity.model,entity.world});
                    if (!onlyEntity && contentOutline) {
                        const auto& objects = contentRegistry.interactions(std::string(selectedModuleId));
                        if (objects.selected()) {
                            const auto& box = objects.object(*objects.selected()).bounds;
                            const auto center = (box.minimum + box.maximum) * 0.5f, size = box.maximum - box.minimum;
                            int edge = 0;
                            for (int axis = 0; axis < 3; ++axis) for (int a : {-1, 1}) for (int b : {-1, 1}) {
                                ecosphere::Pose pose; pose.position = center; pose.scale = glm::vec3(0.025f);
                                pose.scale[axis] = std::max(size[axis], 0.025f);
                                pose.position[(axis + 1) % 3] += a * size[(axis + 1) % 3] * 0.5f;
                                pose.position[(axis + 2) % 3] += b * size[(axis + 2) % 3] * 0.5f;
                                contentOutline->nodes.at(edge++).local = pose.matrix();
                            }
                            models.push_back({contentOutline, objects.world(*objects.selected())});
                        }
                    }
                }
            } else {
                models.push_back({showcase,glm::mat4(1)});
                if (ecosphere::demo::importedVisible) models.push_back({imported,interactionDemo ? interactionDemo->interaction.world("imported") : ecosphere::demo::importedPlacement.matrix()});
            }
            if (interactionDemo) {
                models.push_back({interactionDemo->cubes,glm::mat4(1)});
                const auto& system = interactionDemo->interaction;
                if (system.selected()) models.push_back({interactionDemo->outline,system.world(*system.selected())});
            }
            // Un lote compartido: todos los opacos antes de ordenar BLEND entre modelos.
            modelRenderer->drawModels(models,camera.view(),*projection,settings,camera.position,environment.texture());
        };
        if (regression) {
            glDisable(GL_FRAMEBUFFER_SRGB);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.draw(scene, camera.view(), *projection);
        } else renderLit(lighting);
        if (navigationHud) navigationHud->draw(navigator->message(camera.position),width,height);
        if(fpsCounter.visible) fpsHud.draw(fpsCounter.text(),width,height,ecosphere::HudAnchor::topRight);
        if(lightControls.visible)lightingHud.draw(ecosphere::lightingHudText(lighting,regression,width,lightControls.shading),width,height,ecosphere::HudAnchor::bottomRight);
        const bool captureFrame = !capturePath.empty() && ++renderedFrames == smokeFrameLimit;
        std::vector<unsigned char> withoutImported;
        if (captureFrame) {
            withoutImported.resize(static_cast<std::size_t>(width) * height * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, withoutImported.data());
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Smoke baseline readback failed");
        }
        if (regression && imported && ecosphere::demo::importedVisible)
            modelRenderer->draw(imported, ecosphere::demo::importedPlacement.matrix(), camera.view(), *projection);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL draw failed");
        if (captureFrame) {
            captureAndCheck(capturePath, width, height, services.clearColor, withoutImported);
            glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
        }
        if (!lightingCaptureDirectory.empty() && ++renderedFrames == smokeFrameLimit) {
            ecosphere::checkLightingFramebuffer(lightingCaptureDirectory,width,height,*projection * camera.view(),lighting,*showcase,renderLit);
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!interactionCaptureDirectory.empty() && ++renderedFrames == smokeFrameLimit) {
            ecosphere::checkInteractionFramebuffer(interactionCaptureDirectory,width,height,*interactionDemo,camera,*projection,[&] { renderLit(lighting); });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!contentCaptureDirectory.empty() && ++renderedFrames == smokeFrameLimit) {
            ecosphere::checkContentFramebuffer(contentCaptureDirectory,width,height,contentRegistry,std::string(selectedModuleId),
                camera,*projection,[&](bool include) { renderLit(lighting,include); });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!ecoEnergyCaptureDirectory.empty() && ++renderedFrames == 120) {
            ecosphere::checkEcoEnergyFramebuffer(ecoEnergyCaptureDirectory,width,height,contentRegistry,camera,lighting,contentLoadSeconds,
                renderLit,[&] {
                    glfwSwapBuffers(window.get());glfwPollEvents();
                    int w=0,h=0;glfwGetFramebufferSize(window.get(),&w,&h);
                    if (w!=width || h!=height) throw std::runtime_error("Framebuffer changed during EcoEnergy measurement");
                    return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
                });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!aquaBioCaptureDirectory.empty() && ++renderedFrames == 120) {
            ecosphere::checkAquaBioFramebuffer(aquaBioCaptureDirectory,width,height,contentRegistry,camera,lighting,contentLoadSeconds,
                renderLit,[&] {
                    glfwSwapBuffers(window.get());glfwPollEvents();
                    int w=0,h=0;glfwGetFramebufferSize(window.get(),&w,&h);
                    if (w!=width || h!=height) throw std::runtime_error("Framebuffer changed during AquaBio measurement");
                    return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
                });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if(!ecoCycleCaptureDirectory.empty() && ++renderedFrames==120) {
            ecosphere::checkEcoCycleFramebuffer(ecoCycleCaptureDirectory,width,height,contentRegistry,camera,lighting,contentLoadSeconds,
                renderLit,[&] {
                    glfwSwapBuffers(window.get());glfwPollEvents();int w=0,h=0;glfwGetFramebufferSize(window.get(),&w,&h);
                    if(w!=width||h!=height)throw std::runtime_error("Framebuffer changed during EcoCycle measurement");
                    return glfwWindowShouldClose(window.get())==GLFW_TRUE;
                });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!plazaCaptureDirectory.empty() && ++renderedFrames == 120) {
            ecosphere::checkPlazaFramebuffer(plazaCaptureDirectory,width,height,contentRegistry,camera,lighting,contentLoadSeconds,
                renderLit,[&] {
                    glfwSwapBuffers(window.get());glfwPollEvents();
                    int w=0,h=0;glfwGetFramebufferSize(window.get(),&w,&h);
                    if (w!=width || h!=height) throw std::runtime_error("Framebuffer changed during plaza measurement");
                    return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
                });
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        if (!navigationCaptureDirectory.empty() && ++renderedFrames==120) {
            ecosphere::checkNavigationFramebuffer(navigationCaptureDirectory,width,height,*navigator,camera,navigate,
                [&] {
                    renderLit(lighting);navigationHud->draw(navigator->message(camera.position),width,height);
                    fpsCounter.sample(glfwGetTime(),true);
                    if(lightControls.visible)lightingHud.draw(ecosphere::lightingHudText(lighting,regression,width,lightControls.shading),width,height,ecosphere::HudAnchor::bottomRight);
                    if(fpsCounter.visible)fpsHud.draw(fpsCounter.text(),width,height,ecosphere::HudAnchor::topRight);
                },
                [&] {
                    glfwSwapBuffers(window.get());glfwPollEvents();
                    int w=0,h=0;glfwGetFramebufferSize(window.get(),&w,&h);
                    if(w!=width || h!=height || glfwWindowShouldClose(window.get()))throw std::runtime_error("Navigation smoke interrupted/resized");
                },[&](bool include,const std::string& id){renderLit(lighting,include,id);},integralValidation,uncappedSmoke ? 0 : 1);
            glfwSetWindowShouldClose(window.get(),GLFW_TRUE);
        }
        ++performanceFrames;
        const double performanceSeconds = glfwGetTime() - performanceStart;
        if (performanceSeconds >= 2.0 && aquaBioCaptureDirectory.empty() && ecoCycleCaptureDirectory.empty()) {
            std::cout << "FPS observed: " << performanceFrames / performanceSeconds << "; frames=" << performanceFrames
                      << "; seconds=" << performanceSeconds << "; framebuffer=" << width << 'x' << height
                      << "; requested swap interval=" << (uncappedSmoke ? 0 : 1) << "; regression=" << regression << std::endl;
            performanceStart = glfwGetTime(); performanceFrames = 0;
        }
        glfwSwapBuffers(window.get());
    }
    return 0;
}
}

int main(int argc, char** argv) {
    try {
        const int exitCode = run(argc, argv);
        // run ya destruyó sesión, geometría, ventana y GLFW.
        std::cout << "Clean exit" << std::endl;
        return exitCode;
    } catch (const std::exception& error) {
        std::cerr << "EcoSphere: " << error.what() << '\n';
        return 1;
    }
}
