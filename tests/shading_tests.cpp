#include "app/FpsCounter.hpp"
#include "app/LightingControls.hpp"
#include "app/NavigationHudLayout.hpp"
#include "world/LightingDemoSettings.hpp"
#include "world/ZoneNavigator.hpp"
#include <iostream>
#include <limits>
using namespace ecosphere;
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
  try {
    f();
  } catch (const std::invalid_argument &) {
    return;
  }
  throw std::runtime_error("Invalid input accepted");
}
// CPU oracle for the independently exercised GLSL reflection convention.
float phong(glm::vec3 n, glm::vec3 l, glm::vec3 v, float roughness) {
  MaterialSettings m;
  m.roughness = roughness;
  validateMaterial(m);
  if (!finiteVector(n) || !finiteVector(l) || !finiteVector(v) ||
      glm::length(n) == 0 || glm::length(l) == 0 || glm::length(v) == 0)
    throw std::invalid_argument("Zero direction");
  n = glm::normalize(n);
  l = glm::normalize(l);
  v = glm::normalize(v);
  return glm::dot(n, l) > 0
             ? std::pow(std::max(glm::dot(glm::reflect(-l, n), v), 0.f),
                        blinnExponent(roughness) * .25f)
             : 0;
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Expected staged assets");
    check(phong({0, 0, 1}, {0, 0, 1}, {0, 0, 1}, .3f) == 1, "Frontal specular");
    check(phong({0, 0, 1}, {0, 0, -1}, {0, 0, -1}, .3f) == 0,
          "Unlit side specular");
    check(std::abs(phong({0, 0, 1}, {.8f, 0, .6f}, {-.8f, 0, .6f}, .3f) - 1) <
              1e-5f,
          "Reflection sign convention");
    const auto p = phong({0, 0, 1}, {0, 0, 1}, {1, 0, 1}, .7f);
    const auto h =
        glm::normalize(glm::vec3(0, 0, 1) + glm::normalize(glm::vec3(1, 0, 1)));
    check(std::abs(p - std::pow(h.z, blinnExponent(.7f))) > .001f,
          "Phong/Blinn must not be declared identical");
    for (int i = 0; i < 100; ++i)
      check(phong({0, 0, 1}, {0, 0, 1}, {1, 0, 1}, .7f) == p,
            "Nondeterministic reference");
    rejects([] { phong({0, 0, 1}, {0, 0, 1}, {0, 0, 1}, -1); });
    rejects([] {
      phong({0, 0, 1}, {0, 0, 1}, {0, 0, 1},
            std::numeric_limits<float>::quiet_NaN());
    });
    rejects([] { phong({}, {0, 0, 1}, {0, 0, 1}, .5f); });
    rejects([] { shadingName(static_cast<ShadingModel>(99)); });
    for (auto f0 : {glm::vec3(.04f), glm::vec3(demo::metalColor)}) {
      check(fresnelSchlick(1, f0) == f0 &&
                fresnelSchlick(0, f0) == glm::vec3(1),
            "Fresnel endpoints");
      auto previous = f0;
      for (int i = 1; i <= 100; ++i) {
        const auto f = fresnelSchlick(1 - i / 100.f, f0);
        check(glm::all(glm::greaterThanEqual(f, previous)),
              "Fresnel monotonicity");
        previous = f;
      }
    }
    ZoneNavigator nav(argv[1]);
    ZoneSettings zones;
    LightingControls controls;
    FpsCounter fps;
    auto light = nav.lighting();
    controls.defaults(light);
    for (int zone = 0; zone < 4; ++zone) {
      if (zone) {
        nav.cancelInput();
        nav.input({}, false, false);
        check(nav.input(zones.portals[zone - 1].center, true, false),
              "Enter zone");
        light = nav.lighting();
        controls.defaults(light);
      }
      check(controls.shading == ShadingModel::BlinnPhong, "Initial/reset mode");
      nav.update(.1);
      nav.update(.1);
      const auto before = nav.placements();
      controls.key(GLFW_KEY_F5, GLFW_REPEAT, light);
      controls.key(GLFW_KEY_F5, GLFW_RELEASE, light);
      check(controls.shading == ShadingModel::BlinnPhong, "Repeated F5");
      controls.key(GLFW_KEY_F5, GLFW_PRESS, light);
      check(controls.shading == ShadingModel::Phong, "F5 selection");
      controls.defaults(nav.lighting());
      check(controls.shading == ShadingModel::Phong,
            "Zone-default refresh changed global selection");
      controls.key(GLFW_KEY_F4, GLFW_PRESS, light);
      check(!controls.visible && fps.visible, "F5/F4/FPS independence");
      for (auto model : {ShadingModel::Phong, ShadingModel::BlinnPhong})
        for (int w : {1280, 640, 320}) {
          light.directEnabled = false;
          const auto text = lightingHudText(light, true, w, model);
          const auto layout = layoutNavigationHud(text, w,
                                                  w == 1280  ? 720
                                                  : w == 640 ? 360
                                                             : 240);
          check(!layout.truncated, "Shading label clipped");
          check(text.find(model == ShadingModel::Phong ? "PHONG" : "BLINN") !=
                    std::string::npos,
                "Wrong HUD model");
        }
      const auto after = nav.placements();
      check(before.size() == after.size(), "F5 changed content");
      for (std::size_t i = 0; i < before.size(); ++i)
        check(before[i].world == after[i].world &&
                  before[i].resource == after[i].resource,
              "F5 restarted animation/resource");
      controls.key(GLFW_KEY_0, GLFW_PRESS, light);
      check(controls.shading == ShadingModel::BlinnPhong &&
                light.directEnabled == nav.lighting().directEnabled,
            "0 mode reset");
      controls.key(GLFW_KEY_F4, GLFW_PRESS, light);
      if (zone) {
        nav.cancelInput();
        nav.input({}, false, false);
        check(nav.input({}, false, true), "Return zone");
        light = nav.lighting();
        controls.defaults(light);
      }
    }
    std::cout << "SHADING CPU PASS: reflection convention, back light, angular "
                 "variation, distinct lobes, validation, Fresnel "
                 "endpoints/monotonicity, F5/0/HUD and four-zone "
                 "resource/animation preservation\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
