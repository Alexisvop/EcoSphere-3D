#include "Environment.hpp"
#include "ModelRenderer.hpp"
#include "Shader.hpp"
#include "app/LightingControls.hpp"
#include "app/NavigationHud.hpp"
#include "app/SrgbFramebuffer.hpp"
#include "world/LightingDemo.hpp"
#include "world/LightingDemoSettings.hpp"
#include "world/ZoneNavigator.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <numeric>
using namespace ecosphere;
using Pixels = std::vector<unsigned char>;
void check(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
void look(Camera &c, glm::vec3 eye, glm::vec3 target) {
  c.position = eye;
  const auto d = target - eye;
  c.yawDegrees = glm::degrees(std::atan2(d.z, d.x));
  c.pitchDegrees =
      glm::degrees(std::atan2(d.y, glm::length(glm::vec2(d.x, d.z))));
}
struct Difference {
  std::size_t changed = 0;
  int maximum = 0;
};
Difference difference(const Pixels &a, const Pixels &b) {
  check(a.size() == b.size(), "Mismatched captures");
  Difference d;
  for (std::size_t i = 0; i < a.size(); i += 3) {
    int sum = 0;
    for (int c = 0; c < 3; ++c) {
      const int v = std::abs(int(a[i + c]) - int(b[i + c]));
      sum += v;
      d.maximum = std::max(d.maximum, v);
    }
    d.changed += sum > 12;
  }
  return d;
}
float linear(unsigned char code) {
  float v = code / 255.f;
  return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}
int main(int argc, char **argv) {
  GLFWwindow *window = nullptr;
  try {
    check(argc == 4, "Expected binary directory, output directory, original "
                     "fragment shader");
    const std::filesystem::path binary = argv[1], out = argv[2];
    std::filesystem::create_directories(out);
    // The reference equations are unmodified. This guard only keeps new
    // uniforms active.
    const auto ref = out / "reference-shaders";
    std::filesystem::create_directories(ref);
    for (const char *file : {"model.vert", "skybox.vert", "skybox.frag"})
      std::filesystem::copy_file(
          binary / "shaders" / file, ref / file,
          std::filesystem::copy_options::overwrite_existing);
    auto original = readShaderFile(argv[3]);
    auto pos = original.find("out vec4 fragmentColor;");
    check(pos != std::string::npos, "Invalid reference shader");
    original.insert(pos, "uniform bool phongShading, fresnelEnabled;\n");
    pos = original.find("void main() {");
    check(pos != std::string::npos, "Missing reference main");
    original.insert(pos + 13,
                    "\n    if(phongShading || !fresnelEnabled) discard;\n");
    {
      std::ofstream file(ref / "model.frag");
      file << original;
    }
    check(glfwInit(), "GLFW init");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(1280, 720, "EcoSphere Phong Fresnel validation",
                              nullptr, nullptr);
    check(window, "Window creation");
    glfwMakeContextCurrent(window);
    check(gladLoadGL(glfwGetProcAddress), "GLAD");
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    check(w == 1280 && h == 720, "Exact framebuffer not granted");
    std::cout << "GPU=" << glGetString(GL_RENDERER)
              << " vendor=" << glGetString(GL_VENDOR)
              << " GL=" << glGetString(GL_VERSION)
              << " GLFW=" << glfwGetVersionString() << std::endl;
    verifySrgbFramebuffer();
    {
      ModelRenderer renderer(binary / "shaders"), reference(ref);
      Environment environment(binary / "shaders");
      NavigationHud hud(binary / "shaders"), navigation(binary / "shaders");
      bool invalid = false;
      try {
        renderer.setShadingModel(static_cast<ShadingModel>(99));
      } catch (const std::invalid_argument &) {
        invalid = true;
      }
      check(invalid, "Invalid selector accepted");
      auto demoModel = makeLightingDemo();
      Camera camera;
      LightingSettings lights = demo::lighting;
      GLuint cube = environment.texture();
      std::vector<ModelPlacement> models{{demoModel, glm::mat4(1)}};
      const auto draw = [&](ModelRenderer &r) {
        glViewport(0, 0, w, h);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glClearColor(.05f, .08f, .12f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (lights.environment.skyboxEnabled)
          environment.draw(camera.view(), *camera.projection(w, h));
        r.drawModels(models, camera.view(), *camera.projection(w, h), lights,
                     camera.position, cube);
      };
      const auto pixels = [&] {
        Pixels p(w * h * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, p.data());
        check(glGetError() == GL_NO_ERROR, "Framebuffer readback");
        return p;
      };
      std::ofstream views(out / "captures.csv");
      views << "capture,width,height,camera_x,camera_y,camera_z,yaw,pitch,"
               "direct,reflection,skybox,cubemap\n";
      const auto capture = [&](const std::string &name) {
        views << name << ',' << w << ',' << h << ',' << camera.position.x << ','
              << camera.position.y << ',' << camera.position.z << ','
              << camera.yawDegrees << ',' << camera.pitchDegrees << ','
              << lights.directEnabled << ','
              << lights.environment.reflectionEnabled << ','
              << lights.environment.skyboxEnabled << ',' << (cube != 0) << '\n';
        const auto p = pixels();
        std::ofstream file(out / (name + ".ppm"), std::ios::binary);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file << "P6\n" << w << ' ' << h << "\n255\n";
        for (int y = h - 1; y >= 0; --y)
          file.write(reinterpret_cast<const char *>(p.data() + y * w * 3),
                     w * 3);
        return p;
      };
      std::ofstream comparisons(out / "differences.csv");
      comparisons
          << "comparison,changed_pixels_rgb_sum_gt12,max_channel_delta\n";
      const auto compare = [&](const std::string &name, const Pixels &a,
                               const Pixels &b, bool visible) {
        const auto d = difference(a, b);
        comparisons << name << ',' << d.changed << ',' << d.maximum << '\n';
        if (visible)
          check(d.changed >= 30, "Insufficient comparison signal");
        std::cout << name << " changed=" << d.changed << " max=" << d.maximum
                  << std::endl;
        return d;
      };
      const auto select = [&](std::initializer_list<std::size_t> indices) {
        auto m = std::make_shared<Model>(*demoModel);
        for (auto &n : m->nodes)
          n.visible = false;
        for (auto i : indices)
          m->nodes.at(i).visible = true;
        return m;
      };
      look(camera, {0, 3, 8}, {0, 2, -1});
      draw(reference);
      const auto oldDemo = pixels();
      draw(renderer);
      check(compare("reference-demo", oldDemo, pixels(), false).maximum <= 1,
            "Blinn baseline changed");
      // Analytical flat panel (existing demo geometry), normalized vectors,
      // linear-space readback.
      auto panel = std::make_shared<Model>();
      panel->meshes.push_back(demoModel->meshes[4]);
      panel->nodes.push_back({});
      panel->nodes[0].meshes = {0};
      auto &mesh = panel->meshes[0];
      mesh.material = demo::plastic;
      mesh.material.roughness = .4f;
      mesh.material.specular = .2f;
      mesh.color = {.2f, .3f, .4f, 1};
      models = {{panel, glm::scale(glm::mat4(1), glm::vec3(2.8f, 2.8f, 1))}};
      lights = demo::lighting;
      lights.environment.reflectionEnabled = lights.environment.skyboxEnabled =
          false;
      lights.ambient = 0;
      lights.point.enabled = false;
      lights.directional.vector = {0, 0, -1};
      lights.directional.color = {1, 1, 1};
      lights.directional.intensity = .3f;
      for (auto mode : {ShadingModel::BlinnPhong, ShadingModel::Phong})
        for (float angle : {0.f, 30.f, 60.f}) {
          look(camera,
               {4 * std::sin(glm::radians(angle)), 0,
                4 * std::cos(glm::radians(angle))},
               {0, 0, 0});
          renderer.setShadingModel(mode);
          draw(renderer);
          auto p = pixels();
          float depth = 0;
          glReadPixels(w / 2, h / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT,
                       &depth);
          const auto world = glm::unProject(
              glm::vec3(w / 2 + .5f, h / 2 + .5f, depth), camera.view(),
              *camera.projection(w, h), glm::vec4(0, 0, w, h));
          const auto v = glm::normalize(camera.position - world);
          const float exponent = blinnExponent(.4f);
          float spec = mode == ShadingModel::Phong
                           ? std::pow(std::max(v.z, 0.f), exponent * .25f)
                           : std::pow(glm::normalize(glm::vec3(0, 0, 1) + v).z,
                                      exponent);
          for (int c = 0; c < 3; ++c)
            check(std::abs(linear(p[(h / 2 * w + w / 2) * 3 + c]) -
                           .3f * (mesh.color[c] + .2f * spec)) < .006f,
                  "Analytical specular mismatch");
          lights.directional.vector = {0, 0, 1};
          draw(renderer);
          const auto dark = pixels();
          check(dark[(h / 2 * w + w / 2) * 3] == 0, "Specular on unlit side");
          lights.directional.vector = {0, 0, -1};
        }
      look(camera, {0, 0, 4}, {0, 0, 0});
      renderer.setShadingModel(ShadingModel::BlinnPhong);
      mesh.material.unlit = true;
      draw(renderer);
      const auto unlit = pixels();
      renderer.setShadingModel(ShadingModel::Phong);
      draw(renderer);
      check(pixels() == unlit, "Unlit changed by shading");
      mesh.material.unlit = false;
      mesh.material.alphaMode = AlphaMode::mask;
      mesh.color.a = .25f;
      draw(renderer);
      const auto masked = pixels();
      mesh.visible = false;
      draw(renderer);
      check(pixels() == masked, "MASK discard");
      mesh.visible = true;
      mesh.material.alphaMode = AlphaMode::blend;
      mesh.color.a = .32f;
      draw(renderer);
      const auto blended = pixels();
      for (int c = 0; c < 3; ++c) {
        const float foreground = .3f * (mesh.color[c] + .2f);
        const float bg = glm::vec3(.05f, .08f, .12f)[c];
        check(std::abs(linear(blended[(h / 2 * w + w / 2) * 3 + c]) -
                       (.32f * foreground + .68f * bg)) < .006f,
              "Linear alpha blending");
      }
      mesh.material.alphaMode = AlphaMode::opaque;
      mesh.color.a = 1;
      draw(renderer);
      const auto withNormals = pixels();
      for (auto &v : mesh.vertices)
        v.normal = {};
      renderer.clear();
      draw(renderer);
      check(difference(pixels(), withNormals).maximum <= 1,
            "Geometric-normal fallback changed plane lighting");
      mesh.material.doubleSided = true;
      lights.directional.vector = {0, 0, 1};
      look(camera, {0, 0, -4}, {0, 0, 0});
      draw(renderer);
      const auto back = pixels();
      check(back[(h / 2 * w + w / 2) * 3] > 0,
            "Double-sided back face missing");
      mesh.material.doubleSided = false;
      draw(renderer);
      check(difference(back, pixels()).changed > 30, "Backface culling failed");
      // Same opaque material/camera/lights; only the direct specular technique
      // changes.
      models = {{select({1}), glm::mat4(1)}};
      lights = demo::lighting;
      lights.environment.reflectionEnabled = false;
      look(camera, {0, 2.65f, 2}, {0, 2.65f, -1.5f});
      renderer.setShadingModel(ShadingModel::BlinnPhong);
      draw(renderer);
      const auto blinn = capture("opaque-blinn");
      renderer.setShadingModel(ShadingModel::Phong);
      draw(renderer);
      compare("opaque-phong-vs-blinn", blinn, capture("opaque-phong"), true);
      renderer.setShadingModel(ShadingModel::BlinnPhong);
      lights = demo::lighting;
      for (bool glass : {false, true}) {
        models = {{glass ? select({3, 4}) : select({2}), glm::mat4(1)}};
        const auto center = glass ? demo::glassPlacement.position
                                  : demo::materialPositions[2].position;
        for (bool grazing : {false, true}) {
          look(camera,
               center + (grazing ? glm::vec3(3.45f, 0, .45f)
                                 : glm::vec3(0, 0, 3.5f)),
               center);
          const auto name = std::string(glass ? "glass" : "metal") +
                            (grazing ? "-grazing" : "-normal");
          renderer.setFresnelEnabled(true);
          draw(renderer);
          const auto on = capture(name + "-fresnel-on");
          renderer.setFresnelEnabled(false);
          draw(renderer);
          const auto off = capture(name + "-fresnel-off");
          const auto angular =
              compare(name + "-schlick-vs-F0", on, off, glass ? grazing : true);
          if (glass && !grazing)
            check(angular.maximum <= 1,
                  "Normal-incidence glass must approach F0");
          renderer.setFresnelEnabled(true);
          lights.environment.reflectionEnabled = false;
          draw(renderer);
          compare(name + "-reflection-off", on,
                  capture(name + "-reflection-off"), glass ? grazing : true);
          lights.environment.reflectionEnabled = true;
        }
      }
      models = {{demoModel, glm::mat4(1)}};
      look(camera, {0, 3, 8}, {0, 2, -1});
      draw(renderer);
      const auto mapOn = capture("cubemap-on");
      cube = 0;
      lights.environment.skyboxEnabled = false;
      draw(renderer);
      compare("cubemap-all-off", mapOn, capture("cubemap-off"), true);
      cube = environment.texture();
      draw(renderer);
      compare("skybox-only-off", mapOn, capture("skybox-off-reflections-on"),
              true);
      lights.environment.skyboxEnabled = true;
      // Each model/camera/fresnel state is measured with completed frames, not
      // queued GPU work.
      std::ofstream perf(out / "performance.csv");
      perf << "model,fresnel,swap_interval,pass,width,height,frames,seconds,"
              "fps_mean,frame_mean_ms,frame_p95_ms,gpu_mean_ms,gpu_p95_ms\n";
      for (int swap : {0, 1}) {
        glfwSwapInterval(swap);
        for (int pass = 0; pass < (swap == 0 ? 3 : 1); ++pass)
          for (auto mode :
               (pass % 2 ? std::array<ShadingModel, 2>{ShadingModel::Phong,
                                                       ShadingModel::BlinnPhong}
                         : std::array<ShadingModel, 2>{ShadingModel::BlinnPhong,
                                                       ShadingModel::Phong}))
            for (bool fresnel : {true, false}) {
              renderer.setShadingModel(mode);
              renderer.setFresnelEnabled(fresnel);
              for (int i = 0; i < 30; ++i) {
                draw(renderer);
                glfwSwapBuffers(window);
                glFinish();
              }
              std::vector<double> times, gpuTimes;
              GLuint query = 0;
              glGenQueries(1, &query);
              const auto begin = glfwGetTime();
              for (int i = 0; i < 300; ++i) {
                auto t = glfwGetTime();
                glBeginQuery(GL_TIME_ELAPSED, query);
                draw(renderer);
                glEndQuery(GL_TIME_ELAPSED);
                glfwSwapBuffers(window);
                glFinish();
                GLuint64 ns = 0;
                glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
                gpuTimes.push_back(ns / 1e6);
                times.push_back(glfwGetTime() - t);
              }
              const auto elapsed = glfwGetTime() - begin;
              glDeleteQueries(1, &query);
              std::sort(times.begin(), times.end());
              std::sort(gpuTimes.begin(), gpuTimes.end());
              perf << shadingName(mode) << ',' << fresnel << ',' << swap << ','
                   << pass << ',' << w << ',' << h << ",300," << elapsed << ','
                   << 300 / elapsed << ','
                   << std::accumulate(times.begin(), times.end(), 0.) / 300 *
                          1000
                   << ',' << times[284] * 1000 << ','
                   << std::accumulate(gpuTimes.begin(), gpuTimes.end(), 0.) /
                          300
                   << ',' << gpuTimes[284] << '\n';
            }
      }
      renderer.setFresnelEnabled(true);
      renderer.setShadingModel(ShadingModel::BlinnPhong);
      ZoneNavigator nav(binary / "assets");
      ZoneSettings zones;
      LightingControls controls;
      for (int zone = 0; zone < 4; ++zone) {
        if (zone) {
          nav.cancelInput();
          nav.input({}, false, false);
          check(nav.input(zones.portals[zone - 1].center, true, false),
                "Zone entry");
        }
        camera = nav.entryCamera();
        lights = nav.lighting();
        models.clear();
        renderer.clear();
        reference.clear();
        for (const auto &p : nav.placements())
          models.push_back({p.model, p.world});
        draw(reference);
        const auto before = pixels();
        renderer.setShadingModel(ShadingModel::BlinnPhong);
        draw(renderer);
        const auto actual = pixels();
        check(compare(std::string("reference-") +
                          std::string(zoneId(nav.active())),
                      before, actual, false)
                      .maximum <= 1,
              "Zone Blinn regression");
        for (auto mode : {ShadingModel::BlinnPhong, ShadingModel::Phong}) {
          renderer.setShadingModel(mode);
          draw(renderer);
          navigation.draw(nav.message(camera.position), w, h);
          hud.draw(lightingHudText(lights, false, w, mode), w, h,
                   HudAnchor::bottomRight);
          capture(std::string(zoneId(nav.active())) + "-" + shadingName(mode));
        }
        if (zone == 2)
          for (auto size : {std::pair<int, int>{640, 360}, {320, 240}}) {
            glfwSetWindowSize(window, size.first, size.second);
            const auto begin = glfwGetTime();
            do {
              glfwPollEvents();
              glfwGetFramebufferSize(window, &w, &h);
              if (w == size.first && h == size.second)
                break;
              glfwWaitEventsTimeout(.01);
            } while (glfwGetTime() - begin < 2);
            check(w == size.first && h == size.second, "Resize failed");
            for (int i = 0; i < 3; ++i) {
              draw(renderer);
              glfwSwapBuffers(window);
            }
            for (auto mode : {ShadingModel::BlinnPhong, ShadingModel::Phong}) {
              renderer.setShadingModel(mode);
              draw(renderer);
              navigation.draw(nav.message(camera.position), w, h);
              hud.draw(lightingHudText(lights, false, w, mode), w, h,
                       HudAnchor::bottomRight);
              capture("hud-" + std::to_string(w) + "-" + shadingName(mode));
            }
          }
        if (zone == 2) {
          glfwSetWindowSize(window, 1280, 720);
          const auto restoreStart = glfwGetTime();
          do {
            glfwPollEvents();
            glfwGetFramebufferSize(window, &w, &h);
            if (w == 1280 && h == 720)
              break;
            glfwWaitEventsTimeout(.01);
          } while (glfwGetTime() - restoreStart < 2);
          check(w == 1280 && h == 720, "Restore framebuffer size failed");
          for (int i = 0; i < 3; ++i) {
            draw(renderer);
            glfwSwapBuffers(window);
          }
        }
        if (zone) {
          nav.cancelInput();
          nav.input({}, false, false);
          check(nav.input({}, false, true), "Zone return");
        }
        renderer.setShadingModel(ShadingModel::BlinnPhong);
      }
      check(glGetError() == GL_NO_ERROR, "Final OpenGL error");
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "SHADING GPU PASS" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    if (window)
      glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
}
