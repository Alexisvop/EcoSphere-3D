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
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr int smokeFrameLimit = 30;
constexpr int measuredSmokeFrame = 120;

// ---- Opciones de línea de comandos ----

struct Options {
    std::string moduleId = "ecoenergy";
    bool moduleExplicit = false;
    std::string capturePath;
    std::string lightingCaptureDirectory;
    std::string interactionCaptureDirectory;
    std::string contentCaptureDirectory;
    std::string plazaCaptureDirectory;
    std::string ecoEnergyCaptureDirectory;
    std::string aquaBioCaptureDirectory;
    std::string ecoCycleCaptureDirectory;
    std::string navigationCaptureDirectory;
    bool integralValidation = false;
    bool uncappedSmoke = false;
    bool plazaEnabled = false;
    bool contentEnabled = false;
    bool interactionEnabled = false;
    bool regression = false;
};

const char* const usageText =
    "Usage: ecosphere [--module ecoenergy|aquabio|ecocycle] [--smoke output.ppm] "
    "[--lighting-smoke directory] [--regression][--interaction | --interaction-smoke directory] "
    "[--content | --content-smoke directory | --ecoenergy-smoke directory | --aquabio-smoke directory | "
    "--ecocycle-smoke directory] "
    "[--plaza | --plaza-smoke directory | --navigation-smoke directory | --validation-smoke directory] "
    "[--benchmark-no-vsync]";

std::string takeValue(char** argv, int& index, const char* emptyMessage) {
    std::string value = argv[++index];
    if (value.empty()) throw std::invalid_argument(emptyMessage);
    return value;
}

std::string takeDirectory(char** argv, int& index) {
    return takeValue(argv, index, "Capture directory must not be empty");
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];
        const bool hasValue = i + 1 < argc;
        if (option == "--module" && hasValue) {
            options.moduleExplicit = true;
            options.moduleId = argv[++i];
        } else if (option == "--smoke" && hasValue) {
            options.capturePath = takeValue(argv, i, "Capture path must not be empty");
        } else if (option == "--lighting-smoke" && hasValue) {
            options.lightingCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--interaction") {
            options.interactionEnabled = true;
        } else if (option == "--interaction-smoke" && hasValue) {
            options.interactionEnabled = true;
            options.interactionCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--content") {
            options.contentEnabled = true;
        } else if (option == "--content-smoke" && hasValue) {
            options.contentEnabled = true;
            options.contentCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--ecoenergy-smoke" && hasValue) {
            options.contentEnabled = true;
            options.ecoEnergyCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--aquabio-smoke" && hasValue) {
            options.contentEnabled = true;
            options.aquaBioCaptureDirectory = takeDirectory(argv, i);
            options.moduleId = "aquabio";
        } else if (option == "--ecocycle-smoke" && hasValue) {
            options.contentEnabled = true;
            options.ecoCycleCaptureDirectory = takeDirectory(argv, i);
            options.moduleId = "ecocycle";
        } else if (option == "--plaza") {
            options.plazaEnabled = true;
        } else if (option == "--plaza-smoke" && hasValue) {
            options.plazaEnabled = true;
            options.plazaCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--navigation-smoke" && hasValue) {
            options.plazaEnabled = true;
            options.navigationCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--validation-smoke" && hasValue) {
            options.plazaEnabled = true;
            options.integralValidation = true;
            options.navigationCaptureDirectory = takeDirectory(argv, i);
        } else if (option == "--benchmark-no-vsync") {
            options.uncappedSmoke = true;
        } else if (option == "--regression") {
            options.regression = true;
        } else {
            throw std::invalid_argument(usageText);
        }
    }
    return options;
}

// modos que no se pueden combinar
void validateOptions(Options& options) {
    const bool legacySmoke = !options.capturePath.empty();
    const bool lightingSmoke = !options.lightingCaptureDirectory.empty();
    const bool contentSmoke = !options.contentCaptureDirectory.empty();
    const bool ecoEnergySmoke = !options.ecoEnergyCaptureDirectory.empty();
    const bool aquaBioSmoke = !options.aquaBioCaptureDirectory.empty();
    const bool ecoCycleSmoke = !options.ecoCycleCaptureDirectory.empty();
    const bool plazaSmoke = !options.plazaCaptureDirectory.empty();
    const bool navigationSmoke = !options.navigationCaptureDirectory.empty();

    if (lightingSmoke && (legacySmoke || options.regression))
        throw std::invalid_argument("Lighting smoke requires illuminated mode without legacy smoke");
    if (options.interactionEnabled && (legacySmoke || options.regression || lightingSmoke))
        throw std::invalid_argument("Interaction demo requires its own mode, without historical smoke");

    // --smoke siempre dibuja en modo regresión
    options.regression = options.regression || legacySmoke;

    if (options.contentEnabled && (options.regression || options.interactionEnabled || lightingSmoke))
        throw std::invalid_argument("Content demo requires its own mode, without historical demos");
    if (ecoEnergySmoke && (options.moduleExplicit || contentSmoke || aquaBioSmoke || ecoCycleSmoke
                           || options.plazaEnabled))
        throw std::invalid_argument("EcoEnergy smoke runs standalone on ecoenergy without other modes");
    if (aquaBioSmoke && (options.moduleExplicit || contentSmoke || ecoEnergySmoke || ecoCycleSmoke
                         || options.plazaEnabled))
        throw std::invalid_argument("AquaBio smoke runs standalone on aquabio without other modes");
    if (ecoCycleSmoke && (options.moduleExplicit || contentSmoke || ecoEnergySmoke || aquaBioSmoke
                          || options.plazaEnabled))
        throw std::invalid_argument("EcoCycle smoke runs standalone on ecocycle without other modes");
    if (options.plazaEnabled && (options.moduleExplicit || options.contentEnabled || options.regression
                                 || options.interactionEnabled || lightingSmoke))
        throw std::invalid_argument("Plaza requires its own mode, without modules or historical demos");
    if (plazaSmoke && navigationSmoke)
        throw std::invalid_argument("Choose one plaza/navigation capture mode");
    if (options.uncappedSmoke && !navigationSmoke && !ecoEnergySmoke && !aquaBioSmoke && !ecoCycleSmoke)
        throw std::invalid_argument(
            "No-VSync measurement requires navigation/validation/ecoenergy/aquabio smoke");

    // la plaza carga el contenido del hub
    if (options.plazaEnabled) {
        options.contentEnabled = true;
        options.moduleId = "hub";
    }
}

std::unique_ptr<ecosphere::IModule> createModule(const Options& options) {
    if (options.plazaEnabled) return ecosphere::makePlaza();

    using Factory = std::unique_ptr<ecosphere::IModule> (*)();
    const std::array<Factory, 3> factories = {
        ecosphere::makeEcoEnergy, ecosphere::makeAquaBio, ecosphere::makeEcoCycle
    };
    std::unique_ptr<ecosphere::IModule> module;
    for (auto factory : factories) {
        auto candidate = factory();
        if (candidate->id() == options.moduleId) module = std::move(candidate);
    }
    if (!module) throw std::invalid_argument("Unknown module: " + options.moduleId);
    return module;
}

// ---- Ventana y OpenGL ----

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
    GlfwLifetime(const GlfwLifetime&) = delete;
    GlfwLifetime& operator=(const GlfwLifetime&) = delete;
};

using WindowHandle = std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)>;

std::string windowTitle(const Options& options) {
    if (options.plazaEnabled) return "EcoSphere - Plaza Central";
    std::string prefix = "EcoSphere Phase 3 - ";
    if (options.contentEnabled) {
        prefix = "EcoSphere Phase 5A - ";
    } else if (options.interactionEnabled) {
        prefix = "EcoSphere Phase 4 - ";
    }
    return prefix + options.moduleId;
}

WindowHandle createWindow(const Options& options) {
    // OpenGL 3.3 perfil Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    const std::string title = windowTitle(options);
    const bool hdWindow = options.plazaEnabled || !options.ecoEnergyCaptureDirectory.empty()
        || !options.aquaBioCaptureDirectory.empty() || !options.ecoCycleCaptureDirectory.empty();
    const int width = hdWindow ? ecosphere::plaza::settings.windowWidth : ecosphere::demo::windowWidth;
    const int height = hdWindow ? ecosphere::plaza::settings.windowHeight : ecosphere::demo::windowHeight;

    WindowHandle window(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr), glfwDestroyWindow);
    if (!window) throw std::runtime_error("OpenGL 3.3 Core window creation failed");
    return window;
}

void loadOpenGl(GLFWwindow* window) {
    glfwMakeContextCurrent(window);
    const int version = gladLoadGL(glfwGetProcAddress);
    if (!version || !GLAD_GL_VERSION_3_3) {
        throw std::runtime_error("GLAD 2 could not load OpenGL 3.3");
    }
    GLint profile = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (!(profile & GL_CONTEXT_CORE_PROFILE_BIT)) {
        throw std::runtime_error("Driver did not provide Core profile");
    }
}

void printGraphicsInfo(const std::string& moduleId) {
    std::cout << "Native Wayland: " << (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "yes" : "no")
              << "\nX11: " << (glfwGetPlatform() == GLFW_PLATFORM_X11 ? "yes" : "no")
              << "\nGLFW: " << glfwGetVersionString()
              << "\nOpenGL: " << glGetString(GL_VERSION)
              << "\nVendor: " << glGetString(GL_VENDOR)
              << "\nRenderer: " << glGetString(GL_RENDERER)
              << "\nGLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
              << "\nCore profile verified; selected=" << moduleId << std::endl;
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

// ---- Smoke clásico (--smoke) ----

std::vector<unsigned char> readBackBufferRgb(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

bool closeTo(unsigned char value, float expected) {
    return std::abs(static_cast<int>(value) - static_cast<int>(std::lround(expected))) <= 3;
}

// píxel con el color de un objeto en alguna de sus caras
bool matchesObjectColor(const unsigned char* pixel, glm::vec3 color) {
    bool matchesFace = false;
    for (float tint : {ecosphere::demo::frontBackTint, ecosphere::demo::leftRightTint,
                       ecosphere::demo::topBottomTint}) {
        bool matches = true;
        for (int channel = 0; channel < 3; ++channel) {
            matches &= closeTo(pixel[channel], color[channel] * tint * 255);
        }
        matchesFace |= matches;
    }
    return matchesFace;
}

void writePpm(const std::string& path, int width, int height, const std::vector<unsigned char>& pixels) {
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    const auto stride = static_cast<std::size_t>(width) * 3;
    // OpenGL guarda las filas de abajo hacia arriba
    for (int y = height - 1; y >= 0; --y) {
        output.write(reinterpret_cast<const char*>(pixels.data() + y * stride),
                     static_cast<std::streamsize>(stride));
    }
    output.close();
}

void checkImportedPixels(const std::vector<unsigned char>& pixels, const std::vector<float>& depth,
                         const std::vector<unsigned char>& withoutImported) {
    if (withoutImported.size() != pixels.size()) throw std::runtime_error("Smoke missing imported baseline");
    std::size_t changed = 0;
    std::size_t magenta = 0;
    std::size_t cyan = 0;
    for (std::size_t pixel = 0; pixel < depth.size(); ++pixel) {
        const auto red = pixels[pixel * 3];
        const auto green = pixels[pixel * 3 + 1];
        const auto blue = pixels[pixel * 3 + 2];
        const bool same = red == withoutImported[pixel * 3] && green == withoutImported[pixel * 3 + 1]
            && blue == withoutImported[pixel * 3 + 2];
        if (same) continue;
        if (depth[pixel] >= 1.0f) throw std::runtime_error("Imported pixels lack depth");
        ++changed;
        if (red > 240 && blue > 240 && green < 10) ++magenta;
        if (green > 240 && blue > 240 && red < 10) ++cyan;
    }
    if (changed < 100 || magenta < 10 || cyan < 10)
        throw std::runtime_error("Smoke failed: imported geometry/UV checker missing");
    std::cout << "IMPORTED SMOKE PASS: changed=" << changed << "; magenta=" << magenta << "; cyan=" << cyan
              << '\n';
}

void captureAndCheck(const std::string& path, int width, int height, glm::vec3 clearColor,
                     const std::vector<unsigned char>& withoutImported) {
    const auto pixels = readBackBufferRgb(width, height);
    if (glGetError() != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL readback failed");
    }
    std::vector<float> depth(static_cast<std::size_t>(width) * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    if (glGetError() != GL_NO_ERROR || !glIsEnabled(GL_DEPTH_TEST)) {
        throw std::runtime_error("Smoke failed: depth readback/test unavailable");
    }

    // fondo en la esquina superior izquierda
    const auto sky = static_cast<std::size_t>(height - 1) * width;
    for (int channel = 0; channel < 3; ++channel) {
        if (!closeTo(pixels[sky * 3 + channel], clearColor[channel] * 255)) {
            throw std::runtime_error("Smoke failed: module background missing");
        }
    }

    // píxeles de cada objeto y rango de profundidad
    const std::array<glm::vec3, 5> colors{ecosphere::demo::groundColor, ecosphere::demo::redColor,
        ecosphere::demo::blueColor, ecosphere::demo::parentColor, ecosphere::demo::childColor};
    std::array<std::size_t, 5> counts{};
    float nearest = 1.0f;
    float farthest = 0.0f;
    for (std::size_t pixel = 0; pixel < depth.size(); ++pixel) {
        if (!std::isfinite(depth[pixel]) || depth[pixel] < 0 || depth[pixel] > 1) {
            throw std::runtime_error("Smoke failed: invalid depth value");
        }
        if (depth[pixel] >= 1.0f) continue;
        nearest = std::min(nearest, depth[pixel]);
        farthest = std::max(farthest, depth[pixel]);
        for (std::size_t object = 0; object < colors.size(); ++object) {
            if (matchesObjectColor(&pixels[pixel * 3], colors[object])) ++counts[object];
        }
    }
    for (const auto count : counts) {
        if (count < 10) throw std::runtime_error("Smoke failed: a demo object is missing");
    }
    if (depth[sky] != 1.0f || farthest - nearest < 0.001f) {
        throw std::runtime_error("Smoke failed: foreground/background depth separation missing");
    }

    checkImportedPixels(pixels, depth, withoutImported);
    writePpm(path, width, height, pixels);
    std::cout << "SMOKE PASS: object pixels (ground/red/blue/parent/child)=";
    for (const auto count : counts) std::cout << count << ' ';
    std::cout << "; depth range=" << nearest << ".." << farthest << "; capture=" << path << '\n';
}

// ---- Aplicación ----

class App {
public:
    App(const Options& parsedOptions, const char* argv0);
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void runFrameLoop();

private:
    // datos de un cuadro ya listo para dibujar
    struct Frame {
        int width;
        int height;
        glm::mat4 projection;
        double elapsed;
        double cameraDelta;
        bool activeInput;
    };

    static App& from(GLFWwindow* handle) { return *static_cast<App*>(glfwGetWindowUserPointer(handle)); }
    static void onKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
    static void onFocus(GLFWwindow* handle, int focused);
    static void onIconify(GLFWwindow* handle, int iconified);
    static void onFramebufferSize(GLFWwindow* handle, int width, int height);

    void placeStartCamera();
    void installCallbacks();
    void loadContent(const std::filesystem::path& exeDirectory);
    void createRenderers(const std::filesystem::path& exeDirectory);
    void createContentOutline();

    void handleKey(GLFWwindow* handle, int key, int action);
    void printLightingState() const;

    void runFrame();
    float held(int key) const;
    ecosphere::CameraInput readCameraInput() const;
    bool handleZoneNavigation(const Frame& frame);
    bool navigate(const glm::mat4& projection, glm::vec3 position, bool enter, bool home);
    void updateInteractionDemo(const Frame& frame);
    void updateContent(const Frame& frame, bool transitioned);

    void renderFrame(const Frame& frame);
    void renderLit(const glm::mat4& projection, const ecosphere::LightingSettings& settings,
                   bool includeContent = true, const std::optional<std::string>& onlyEntity = std::nullopt);
    void addContentModels(std::vector<ecosphere::ModelPlacement>& models,
                          const std::optional<std::string>& onlyEntity);
    void drawHuds(int width, int height);
    void drawImportedForRegression(const Frame& frame);

    bool reachedFrame(const std::string& directory, int frameNumber);
    std::function<bool()> measurementPresenter(int width, int height, const char* resizeMessage);
    void runSmokeCaptures(const Frame& frame);
    void reportFps(const Frame& frame);
    int swapInterval() const { return options.uncappedSmoke ? 0 : 1; }

    Options options;
    std::string selectedModuleId;
    bool regression;
    ecosphere::LightingSettings lighting = ecosphere::demo::lighting;
    std::unique_ptr<ecosphere::IModule> module;
    GlfwLifetime glfw;
    WindowHandle window;
    ecosphere::Camera camera;
    ecosphere::FpsCounter fpsCounter;
    ecosphere::LightingControls lightControls;

    // teclas que juntan los callbacks de GLFW
    bool collectObjectKeys;
    bool collectNavigationKeys;
    std::vector<int> objectKeys;
    bool skipNextUpdate = true;
    bool enterHeld = false;
    bool homeHeld = false;
    std::vector<std::pair<int, bool>> navigationKeys;

    std::unique_ptr<ecosphere::ContentRegistry> legacyContentRegistry;
    std::unique_ptr<ecosphere::ZoneNavigator> navigator;
    ecosphere::ContentRegistry* contentRegistry = nullptr;
    DemoServices services;
    std::unique_ptr<ecosphere::ModuleSession> session;
    double contentLoadSeconds = 0;

    ecosphere::Scene scene;
    std::filesystem::path shaders;
    std::unique_ptr<ecosphere::SceneRenderer> renderer;
    std::unique_ptr<ecosphere::AssetManager> assets;
    std::shared_ptr<const ecosphere::Model> imported;
    std::unique_ptr<ecosphere::ModelRenderer> modelRenderer;
    std::unique_ptr<ecosphere::NavigationHud> navigationHud;
    std::unique_ptr<ecosphere::NavigationHud> fpsHud;
    std::unique_ptr<ecosphere::NavigationHud> lightingHud;
    std::unique_ptr<ecosphere::Environment> environment;
    std::shared_ptr<ecosphere::Model> showcase;
    std::shared_ptr<ecosphere::Model> contentOutline;
    std::unique_ptr<ecosphere::InteractionAnimationDemo> interactionDemo;

    double performanceStart = 0;
    int performanceFrames = 0;
    double previousTimeSeconds = 0;
    int renderedFrames = 0;
};

App::App(const Options& parsedOptions, const char* argv0)
    : options(parsedOptions),
      selectedModuleId(parsedOptions.moduleId),
      regression(parsedOptions.regression),
      module(createModule(parsedOptions)),
      window(createWindow(parsedOptions)),
      collectObjectKeys(parsedOptions.interactionEnabled || parsedOptions.contentEnabled),
      collectNavigationKeys(parsedOptions.plazaEnabled) {
    loadOpenGl(window.get());
    printGraphicsInfo(selectedModuleId);
    glfwSwapInterval(swapInterval());
    std::cout << "Requested swap interval: " << swapInterval() << std::endl;

    placeStartCamera();
    installCallbacks();
    std::cout << "Camera mode: first-person\n"
                 "Controls: F1 first-person; F2 free; F3 FPS; F4 lighting HUD; 0 restore zone lighting; "
                 "F5 Phong/Blinn-Phong; WASD move; Q/E descend/ascend (free); arrows look; ESC exit; "
                 "1 direct, 2 directional, 3 point, 4 reflection, 5 skybox, 6 regression. "
                 "Cursor stays released."
              << std::endl;

    const auto exeDirectory = executableDirectory(argv0);
    loadContent(exeDirectory);
    createRenderers(exeDirectory);

    ecosphere::verifySrgbFramebuffer();
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    performanceStart = glfwGetTime();
    performanceFrames = 0;
    previousTimeSeconds = glfwGetTime();
    renderedFrames = 0;
}

void App::placeStartCamera() {
    if (options.plazaEnabled) {
        camera.position = ecosphere::plaza::settings.cameraStart;
        camera.yawDegrees = ecosphere::plaza::settings.cameraYaw;
        camera.pitchDegrees = ecosphere::plaza::settings.cameraPitch;
    } else if (options.contentEnabled && selectedModuleId == "aquabio") {
        camera = ecosphere::ZoneSettings{}.entries[2].make();
    } else if (options.contentEnabled && selectedModuleId == "ecocycle") {
        camera = ecosphere::ZoneSettings{}.entries[3].make();
    } else if (!options.ecoEnergyCaptureDirectory.empty()) {
        camera.position = {0.0f, 1.7f, 6.0f};
        camera.yawDegrees = -90.0f;
        camera.pitchDegrees = -4.0f;
    }
}

void App::installCallbacks() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetKeyCallback(window.get(), onKey);
    glfwSetWindowFocusCallback(window.get(), onFocus);
    glfwSetWindowIconifyCallback(window.get(), onIconify);
    glfwSetFramebufferSizeCallback(window.get(), onFramebufferSize);
}

void App::loadContent(const std::filesystem::path& exeDirectory) {
    legacyContentRegistry = std::make_unique<ecosphere::ContentRegistry>(exeDirectory / "assets");
    const double contentLoadStart = glfwGetTime();
    if (options.plazaEnabled) navigator = std::make_unique<ecosphere::ZoneNavigator>(exeDirectory / "assets");
    contentRegistry = navigator ? &navigator->registry() : legacyContentRegistry.get();
    if (options.contentEnabled) services.registry = contentRegistry;
    if (!navigator) session = std::make_unique<ecosphere::ModuleSession>(std::move(module), services);
    if (navigator) {
        camera = navigator->entryCamera();
        module.reset();
        std::cout << "Navigation: Enter near an access to enter; H return to plaza; ESC exit. "
                     "Release Enter/H before another transition.\n";
    }
    contentLoadSeconds = glfwGetTime() - contentLoadStart;

    if (options.contentEnabled) {
        lighting = contentRegistry->lighting(selectedModuleId).value_or(lighting);
        std::cout << "Content controls: Space select; Tab next; Backspace deselect; J/L, U/O, I/K translate; "
                     "Z/X rotate; C/V scale; G take/drop.\n";
    }
    lightControls.defaults(navigator ? navigator->lighting() : lighting);
}

void App::createRenderers(const std::filesystem::path& exeDirectory) {
    shaders = exeDirectory / "shaders";
    std::cout << "Shaders: " << shaders.string() << std::endl;
    renderer = std::make_unique<ecosphere::SceneRenderer>(shaders);
    assets = std::make_unique<ecosphere::AssetManager>(exeDirectory / "assets");
    if (!options.plazaEnabled) {
        imported = assets->loadModel(ecosphere::demo::importedModel);
        for (const auto& warning : imported->warnings) std::cout << "Asset notice: " << warning << '\n';
        std::cout << "Imported model: " << imported->meshes.size() << " primitives, "
                  << imported->nodes.size() << " nodes, " << imported->textures.size() << " textures"
                  << std::endl;
    }
    modelRenderer = std::make_unique<ecosphere::ModelRenderer>(shaders);
    if (navigator) navigationHud = std::make_unique<ecosphere::NavigationHud>(shaders);
    fpsHud = std::make_unique<ecosphere::NavigationHud>(shaders);
    lightingHud = std::make_unique<ecosphere::NavigationHud>(shaders);
    environment = std::make_unique<ecosphere::Environment>(shaders);
    if (!options.plazaEnabled) showcase = ecosphere::makeLightingDemo();
    if (options.contentEnabled) createContentOutline();
    if (options.interactionEnabled) {
        interactionDemo = std::make_unique<ecosphere::InteractionAnimationDemo>(*imported);
        std::cout << "Interaction controls: Space center ray; Tab next; Backspace deselect; "
                     "J/L X, U/O Y, I/K Z; Z/X rotate; C/V scale; G take/drop; P pause/resume; "
                     "R restart animations. Cursor released."
                  << std::endl;
    }
}

// cubo amarillo para las 12 aristas del objeto seleccionado
void App::createContentOutline() {
    contentOutline = std::make_shared<ecosphere::Model>();
    auto edge = ecosphere::makeUnitCube();
    edge.material.unlit = true;
    edge.color = {1, 0.8f, 0.02f, 1};
    contentOutline->meshes.push_back(std::move(edge));
    for (int i = 0; i < 12; ++i) {
        ecosphere::ModelNode node;
        node.meshes = {0};
        contentOutline->nodes.push_back(node);
    }
}

// ---- Teclado y ventana ----

void App::onKey(GLFWwindow* handle, int key, int, int action, int) {
    from(handle).handleKey(handle, key, action);
}

void App::handleKey(GLFWwindow* handle, int key, int action) {
    const bool pressed = action == GLFW_PRESS;

    fpsCounter.key(key, action);
    if (key == GLFW_KEY_F3 && pressed)
        std::cout << "FPS overlay: " << (fpsCounter.visible ? "on" : "off") << std::endl;

    if (key == GLFW_KEY_ESCAPE && pressed) {
        std::cout << "ESC: close requested" << std::endl;
        glfwSetWindowShouldClose(handle, GLFW_TRUE);
    }

    // F1 primera persona, F2 cámara libre
    if (pressed && (key == GLFW_KEY_F1 || key == GLFW_KEY_F2)) {
        const bool firstPerson = key == GLFW_KEY_F1;
        camera.setMode(firstPerson ? ecosphere::CameraMode::firstPerson : ecosphere::CameraMode::free);
        std::cout << "Camera mode: " << (firstPerson ? "first-person" : "free") << std::endl;
    }

    // luces (F4, F5, 0 a 5) y modo regresión (6)
    const bool lightingChanged = lightControls.key(key, action, lighting);
    const bool regressionToggled = key == GLFW_KEY_6 && pressed;
    if (regressionToggled) regression = !regression;
    if (lightingChanged || regressionToggled) printLightingState();

    // teclas que se procesan en el siguiente cuadro
    const bool focused = glfwGetWindowAttrib(handle, GLFW_FOCUSED) != GLFW_FALSE;
    if (collectObjectKeys && pressed && focused) objectKeys.push_back(key);
    const bool zoneKey = key == GLFW_KEY_ENTER || key == GLFW_KEY_H;
    if (collectNavigationKeys && zoneKey && (pressed || action == GLFW_RELEASE) && focused)
        navigationKeys.emplace_back(key, pressed);

    if (action == GLFW_RELEASE) {
        std::cout << "Camera state: position=" << camera.position.x << ',' << camera.position.y << ','
                  << camera.position.z << " yaw=" << camera.yawDegrees << " pitch=" << camera.pitchDegrees
                  << std::endl;
    }
}

void App::printLightingState() const {
    const auto& light = lighting;
    std::cout << "Lighting state: direct=" << light.directEnabled
              << " directional=" << light.directional.enabled
              << " point=" << light.point.enabled
              << " reflection=" << light.environment.reflectionEnabled
              << " skybox=" << light.environment.skyboxEnabled
              << " regression=" << regression
              << " HUD=" << lightControls.visible
              << " shading=" << ecosphere::shadingName(lightControls.shading) << std::endl;
}

void App::onFocus(GLFWwindow* handle, int focused) {
    auto& app = from(handle);
    app.skipNextUpdate = true;
    app.navigationKeys.clear();
    app.enterHeld = false;
    app.homeHeld = false;
    std::cout << "Focus: " << (focused ? "active" : "paused") << std::endl;
}

void App::onIconify(GLFWwindow* handle, int iconified) {
    from(handle).skipNextUpdate = true;
    std::cout << "Minimized: " << (iconified ? "yes" : "no") << std::endl;
}

void App::onFramebufferSize(GLFWwindow*, int width, int height) {
    std::cout << "Framebuffer: " << width << 'x' << height << std::endl;
}

// ---- Ciclo principal ----

void App::runFrameLoop() {
    while (!glfwWindowShouldClose(window.get())) runFrame();
}

void App::runFrame() {
    glfwPollEvents();
    const double currentTimeSeconds = glfwGetTime();
    const double elapsed = currentTimeSeconds - previousTimeSeconds;
    if (session) session->update(elapsed);

    const bool focused = glfwGetWindowAttrib(window.get(), GLFW_FOCUSED) == GLFW_TRUE;
    const bool minimized = glfwGetWindowAttrib(window.get(), GLFW_ICONIFIED) == GLFW_TRUE;
    const bool activeInput = focused && !minimized && !skipNextUpdate;
    const double cameraDelta = ecosphere::cameraDeltaSeconds(elapsed, activeInput);
    fpsCounter.sample(currentTimeSeconds, activeInput);
    skipNextUpdate = false;

    // mover la cámara con el teclado
    ecosphere::CameraInput input;
    if (focused && !minimized) input = readCameraInput();
    camera.update(input, cameraDelta);
    previousTimeSeconds = currentTimeSeconds;

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window.get(), &width, &height);
    const auto projection = camera.projection(width, height);
    if (!projection || minimized) {
        objectKeys.clear();
        if (navigator) navigator->cancelInput();
        glfwWaitEventsTimeout(0.05);
        return;
    }
    const Frame frame{width, height, *projection, elapsed, cameraDelta, activeInput};

    const bool transitioned = handleZoneNavigation(frame);
    if (interactionDemo && !regression) {
        updateInteractionDemo(frame);
    } else if (options.contentEnabled && !regression) {
        updateContent(frame, transitioned);
    } else {
        objectKeys.clear();
    }

    renderFrame(frame);
    drawImportedForRegression(frame);
    runSmokeCaptures(frame);
    reportFps(frame);
    glfwSwapBuffers(window.get());
}

float App::held(int key) const {
    return glfwGetKey(window.get(), key) == GLFW_PRESS ? 1.0f : 0.0f;
}

// WASD mueve, Q/E baja y sube, flechas giran
ecosphere::CameraInput App::readCameraInput() const {
    ecosphere::CameraInput input;
    input.forward = held(GLFW_KEY_W) - held(GLFW_KEY_S);
    input.right = held(GLFW_KEY_D) - held(GLFW_KEY_A);
    input.vertical = held(GLFW_KEY_E) - held(GLFW_KEY_Q);
    input.yaw = held(GLFW_KEY_RIGHT) - held(GLFW_KEY_LEFT);
    input.pitch = held(GLFW_KEY_UP) - held(GLFW_KEY_DOWN);
    return input;
}

// Enter entra a una zona, H regresa a la plaza
bool App::handleZoneNavigation(const Frame& frame) {
    if (!navigator) return false;
    bool transitioned = false;
    if (!frame.activeInput) {
        navigator->cancelInput();
        navigationKeys.clear();
        enterHeld = false;
        homeHeld = false;
    }
    if (frame.activeInput) {
        for (const auto& [key, isPressed] : navigationKeys) {
            if (key == GLFW_KEY_ENTER) {
                enterHeld = isPressed;
            } else {
                homeHeld = isPressed;
            }
            transitioned = navigate(frame.projection, camera.position, enterHeld, homeHeld);
            if (transitioned) break;
        }
        if (!transitioned) navigator->input(camera.position, enterHeld, homeHeld);
    }
    navigationKeys.clear();
    navigator->update(frame.activeInput && options.contentCaptureDirectory.empty() ? frame.elapsed : 0);
    return transitioned;
}

bool App::navigate(const glm::mat4& projection, glm::vec3 position, bool enter, bool home) {
    std::unique_ptr<ecosphere::ModelRenderer> prepared;
    bool attemptedGpu = false;
    // sube la zona nueva a la GPU antes de cambiar
    const auto prepare = [&](const std::vector<ecosphere::ContentPlacement>& content,
                             const ecosphere::LightingSettings& lights, const ecosphere::Camera& entry) {
        attemptedGpu = true;
        prepared = std::make_unique<ecosphere::ModelRenderer>(shaders);
        std::vector<ecosphere::ModelPlacement> models;
        for (const auto& placement : content) models.push_back({placement.model, placement.world});
        prepared->setShadingModel(lightControls.shading);
        prepared->drawModels(models, entry.view(), projection, lights, entry.position,
                             environment->texture());
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Zone GPU preparation failed");
    };
    const bool changed = navigator->input(position, enter, home, prepare);

    if (!changed && attemptedGpu) {
        // vaciar los errores de GL que dejó la zona rechazada
        for (int i = 0; i < 8; ++i) {
            const GLenum diagnostic = glGetError();
            if (diagnostic == GL_NO_ERROR) break;
            std::cerr << "Rejected candidate GL diagnostic: " << diagnostic << std::endl;
        }
    }
    if (!changed && (enter || home) && !navigator->error().empty())
        std::cerr << "Zone transition rejected: " << navigator->error() << std::endl;
    if (!changed) return false;

    modelRenderer.swap(prepared);
    camera = navigator->entryCamera();
    lighting = navigator->lighting();
    lightControls.defaults(lighting);
    selectedModuleId = std::string(ecosphere::zoneId(navigator->active()));
    objectKeys.clear();
    previousTimeSeconds = glfwGetTime();
    enterHeld = held(GLFW_KEY_ENTER) > 0;
    homeHeld = held(GLFW_KEY_H) > 0;
    std::cout << "Zone active: " << selectedModuleId << "; entities=" << navigator->placements().size()
              << std::endl;
    return true;
}

void App::updateInteractionDemo(const Frame& frame) {
    const bool measuring = !options.interactionCaptureDirectory.empty();
    interactionDemo->update(frame.activeInput && !measuring ? frame.elapsed : 0);
    if (frame.activeInput) {
        ecosphere::updateInteractionInput(*interactionDemo, camera, frame.projection, objectKeys,
                                          [this](int key) { return held(key); }, frame.cameraDelta);
    }
    objectKeys.clear();
    interactionDemo->synchronize();
}

void App::updateContent(const Frame& frame, bool transitioned) {
    if (!navigator) {
        const bool measuring = !options.contentCaptureDirectory.empty()
            || !options.ecoEnergyCaptureDirectory.empty() || !options.aquaBioCaptureDirectory.empty()
            || !options.ecoCycleCaptureDirectory.empty();
        contentRegistry->update(frame.activeInput && !measuring ? frame.elapsed : 0);
    }
    if (frame.activeInput && !transitioned) {
        auto& objects = contentRegistry->interactions(selectedModuleId);
        ecosphere::updateContentInput(*contentRegistry, selectedModuleId, camera, frame.projection,
                                      objectKeys, [this](int key) { return held(key); }, frame.cameraDelta);
        if (!objectKeys.empty()) {
            std::cout << "Content selection: " << selectedModuleId << '/'
                      << objects.selected().value_or("none") << " held=" << objects.holding() << std::endl;
        }
    }
    objectKeys.clear();
}

// ---- Dibujo ----

void App::renderFrame(const Frame& frame) {
    glViewport(0, 0, frame.width, frame.height);
    const auto clear = navigator ? navigator->clearColor() : services.clearColor;
    glClearColor(clear.r, clear.g, clear.b, 1);
    if (regression) {
        glDisable(GL_FRAMEBUFFER_SRGB);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer->draw(scene, camera.view(), frame.projection);
    } else {
        renderLit(frame.projection, lighting);
    }
    drawHuds(frame.width, frame.height);
}

void App::renderLit(const glm::mat4& projection, const ecosphere::LightingSettings& settings,
                    bool includeContent, const std::optional<std::string>& onlyEntity) {
    modelRenderer->setShadingModel(lightControls.shading);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!options.contentEnabled) {
        renderer->drawLit(*modelRenderer, scene, camera.view(), projection, settings, camera.position,
                          environment->texture());
    }
    if (settings.environment.skyboxEnabled) environment->draw(camera.view(), projection);

    std::vector<ecosphere::ModelPlacement> models;
    if (options.contentEnabled) {
        if (includeContent) addContentModels(models, onlyEntity);
    } else {
        models.push_back({showcase, glm::mat4(1)});
        if (ecosphere::demo::importedVisible) {
            const glm::mat4 placement = interactionDemo ? interactionDemo->interaction.world("imported")
                                                        : ecosphere::demo::importedPlacement.matrix();
            models.push_back({imported, placement});
        }
    }
    if (interactionDemo) {
        models.push_back({interactionDemo->cubes, glm::mat4(1)});
        const auto& system = interactionDemo->interaction;
        if (system.selected()) models.push_back({interactionDemo->outline, system.world(*system.selected())});
    }
    // todos los modelos en un solo lote
    modelRenderer->drawModels(models, camera.view(), projection, settings, camera.position,
                              environment->texture());
}

void App::addContentModels(std::vector<ecosphere::ModelPlacement>& models,
                           const std::optional<std::string>& onlyEntity) {
    for (const auto& entity : contentRegistry->placements())
        if (!onlyEntity || entity.id == *onlyEntity) models.push_back({entity.model, entity.world});
    if (onlyEntity || !contentOutline) return;

    const auto& objects = contentRegistry->interactions(selectedModuleId);
    if (!objects.selected()) return;

    // acomodar las 12 aristas alrededor de la caja del objeto
    const auto& box = objects.object(*objects.selected()).bounds;
    const auto center = (box.minimum + box.maximum) * 0.5f;
    const auto size = box.maximum - box.minimum;
    int edge = 0;
    for (int axis = 0; axis < 3; ++axis) {
        for (int a : {-1, 1}) {
            for (int b : {-1, 1}) {
                ecosphere::Pose pose;
                pose.position = center;
                pose.scale = glm::vec3(0.025f);
                pose.scale[axis] = std::max(size[axis], 0.025f);
                pose.position[(axis + 1) % 3] += a * size[(axis + 1) % 3] * 0.5f;
                pose.position[(axis + 2) % 3] += b * size[(axis + 2) % 3] * 0.5f;
                contentOutline->nodes.at(edge++).local = pose.matrix();
            }
        }
    }
    models.push_back({contentOutline, objects.world(*objects.selected())});
}

void App::drawHuds(int width, int height) {
    if (navigationHud) navigationHud->draw(navigator->message(camera.position), width, height);
    if (fpsCounter.visible) fpsHud->draw(fpsCounter.text(), width, height, ecosphere::HudAnchor::topRight);
    if (lightControls.visible) {
        const auto text = ecosphere::lightingHudText(lighting, regression, width, lightControls.shading);
        lightingHud->draw(text, width, height, ecosphere::HudAnchor::bottomRight);
    }
}

// en modo regresión el modelo importado va encima, y --smoke compara antes y después
void App::drawImportedForRegression(const Frame& frame) {
    const bool captureFrame = reachedFrame(options.capturePath, smokeFrameLimit);
    std::vector<unsigned char> withoutImported;
    if (captureFrame) {
        withoutImported = readBackBufferRgb(frame.width, frame.height);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Smoke baseline readback failed");
    }
    if (regression && imported && ecosphere::demo::importedVisible)
        modelRenderer->draw(imported, ecosphere::demo::importedPlacement.matrix(), camera.view(),
                            frame.projection);
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL draw failed");
    if (captureFrame) {
        captureAndCheck(options.capturePath, frame.width, frame.height, services.clearColor, withoutImported);
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }
}

// ---- Capturas automáticas (--*-smoke) ----

bool App::reachedFrame(const std::string& directory, int frameNumber) {
    return !directory.empty() && ++renderedFrames == frameNumber;
}

std::function<bool()> App::measurementPresenter(int width, int height, const char* resizeMessage) {
    return [this, width, height, resizeMessage] {
        glfwSwapBuffers(window.get());
        glfwPollEvents();
        int currentWidth = 0;
        int currentHeight = 0;
        glfwGetFramebufferSize(window.get(), &currentWidth, &currentHeight);
        if (currentWidth != width || currentHeight != height) throw std::runtime_error(resizeMessage);
        return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
    };
}

void App::runSmokeCaptures(const Frame& frame) {
    const int width = frame.width;
    const int height = frame.height;
    const auto& projection = frame.projection;
    const ecosphere::PlazaRender render = [&](const ecosphere::LightingSettings& settings,
                                              bool includeContent,
                                              const std::optional<std::string>& onlyEntity) {
        renderLit(projection, settings, includeContent, onlyEntity);
    };

    if (reachedFrame(options.lightingCaptureDirectory, smokeFrameLimit)) {
        ecosphere::checkLightingFramebuffer(options.lightingCaptureDirectory, width, height,
            projection * camera.view(), lighting, *showcase,
            [&](const ecosphere::LightingSettings& settings) { renderLit(projection, settings); });
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }
    if (reachedFrame(options.interactionCaptureDirectory, smokeFrameLimit)) {
        ecosphere::checkInteractionFramebuffer(options.interactionCaptureDirectory, width, height,
            *interactionDemo, camera, projection, [&] { renderLit(projection, lighting); });
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }
    if (reachedFrame(options.contentCaptureDirectory, smokeFrameLimit)) {
        ecosphere::checkContentFramebuffer(options.contentCaptureDirectory, width, height, *contentRegistry,
            selectedModuleId, camera, projection,
            [&](bool includeContent) { renderLit(projection, lighting, includeContent); });
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }

    // mediciones de cada zona en el cuadro 120
    using MeasuredCheck = decltype(&ecosphere::checkPlazaFramebuffer);
    struct MeasuredSmoke {
        const std::string& directory;
        MeasuredCheck check;
        const char* resizeMessage;
    };
    const MeasuredSmoke measuredSmokes[] = {
        {options.ecoEnergyCaptureDirectory, ecosphere::checkEcoEnergyFramebuffer,
         "Framebuffer changed during EcoEnergy measurement"},
        {options.aquaBioCaptureDirectory, ecosphere::checkAquaBioFramebuffer,
         "Framebuffer changed during AquaBio measurement"},
        {options.ecoCycleCaptureDirectory, ecosphere::checkEcoCycleFramebuffer,
         "Framebuffer changed during EcoCycle measurement"},
        {options.plazaCaptureDirectory, ecosphere::checkPlazaFramebuffer,
         "Framebuffer changed during plaza measurement"},
    };
    for (const auto& smoke : measuredSmokes) {
        if (!reachedFrame(smoke.directory, measuredSmokeFrame)) continue;
        smoke.check(smoke.directory, width, height, *contentRegistry, camera, lighting, contentLoadSeconds,
                    render, measurementPresenter(width, height, smoke.resizeMessage));
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }

    if (reachedFrame(options.navigationCaptureDirectory, measuredSmokeFrame)) {
        const auto navigateTo = [&](glm::vec3 position, bool enter, bool home) {
            return navigate(projection, position, enter, home);
        };
        const auto renderWithHuds = [&] {
            renderLit(projection, lighting);
            navigationHud->draw(navigator->message(camera.position), width, height);
            fpsCounter.sample(glfwGetTime(), true);
            if (lightControls.visible) {
                const auto text =
                    ecosphere::lightingHudText(lighting, regression, width, lightControls.shading);
                lightingHud->draw(text, width, height, ecosphere::HudAnchor::bottomRight);
            }
            if (fpsCounter.visible) {
                fpsHud->draw(fpsCounter.text(), width, height, ecosphere::HudAnchor::topRight);
            }
        };
        const auto present = [&] {
            glfwSwapBuffers(window.get());
            glfwPollEvents();
            int currentWidth = 0;
            int currentHeight = 0;
            glfwGetFramebufferSize(window.get(), &currentWidth, &currentHeight);
            if (currentWidth != width || currentHeight != height || glfwWindowShouldClose(window.get()))
                throw std::runtime_error("Navigation smoke interrupted/resized");
        };
        const auto renderContent = [&](bool includeContent, const std::string& id) {
            renderLit(projection, lighting, includeContent, id);
        };
        ecosphere::checkNavigationFramebuffer(options.navigationCaptureDirectory, width, height, *navigator,
            camera, navigateTo, renderWithHuds, present, renderContent, options.integralValidation,
            swapInterval());
        glfwSetWindowShouldClose(window.get(), GLFW_TRUE);
    }
}

// FPS en consola cada 2 segundos
void App::reportFps(const Frame& frame) {
    ++performanceFrames;
    const double performanceSeconds = glfwGetTime() - performanceStart;
    if (performanceSeconds < 2.0) return;
    if (!options.aquaBioCaptureDirectory.empty() || !options.ecoCycleCaptureDirectory.empty()) return;
    std::cout << "FPS observed: " << performanceFrames / performanceSeconds
              << "; frames=" << performanceFrames
              << "; seconds=" << performanceSeconds
              << "; framebuffer=" << frame.width << 'x' << frame.height
              << "; requested swap interval=" << swapInterval()
              << "; regression=" << regression << std::endl;
    performanceStart = glfwGetTime();
    performanceFrames = 0;
}

int run(int argc, char** argv) {
    Options options = parseOptions(argc, argv);
    validateOptions(options);
    App app(options, argv[0]);
    app.runFrameLoop();
    return 0;
}
}

int main(int argc, char** argv) {
    try {
        const int exitCode = run(argc, argv);
        std::cout << "Clean exit" << std::endl;
        return exitCode;
    } catch (const std::exception& error) {
        std::cerr << "EcoSphere: " << error.what() << '\n';
        return 1;
    }
}
