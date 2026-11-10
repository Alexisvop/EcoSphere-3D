#pragma once
#include "scene/Lighting.hpp"
#include "graphics/Shading.hpp"
#include <GLFW/glfw3.h>
#include <string>

namespace ecosphere {
class LightingControls {
public:
    bool visible = true;
    ShadingModel shading = ShadingModel::BlinnPhong;

    void defaults(const LightingSettings& original) { original_ = original; }

    // teclas de luces, regresa true si algo cambió
    bool key(int key, int action, LightingSettings& current) {
        if (action != GLFW_PRESS) return false;
        switch (key) {
        case GLFW_KEY_F4:
            visible = !visible;
            break;
        case GLFW_KEY_0:
            current = original_;
            shading = ShadingModel::BlinnPhong;
            break;
        case GLFW_KEY_F5:
            shading = shading == ShadingModel::BlinnPhong ? ShadingModel::Phong : ShadingModel::BlinnPhong;
            break;
        case GLFW_KEY_1:
            current.directEnabled = !current.directEnabled;
            break;
        case GLFW_KEY_2:
            current.directional.enabled = !current.directional.enabled;
            break;
        case GLFW_KEY_3:
            current.point.enabled = !current.point.enabled;
            break;
        case GLFW_KEY_4:
            current.environment.reflectionEnabled = !current.environment.reflectionEnabled;
            break;
        case GLFW_KEY_5:
            current.environment.skyboxEnabled = !current.environment.skyboxEnabled;
            break;
        default:
            return false;
        }
        return true;
    }

private:
    LightingSettings original_;
};

// texto del panel de luces, más corto en ventanas angostas
inline std::string lightingHudText(const LightingSettings& light, bool regression, int width,
                                   ShadingModel shading = ShadingModel::BlinnPhong) {
    const std::string model = shadingName(shading);
    const auto state = [](bool on) { return std::string(on ? "ON" : "OFF"); };
    // con "-" se marca la luz encendida que no se ve porque la directa está apagada
    const auto direct = state(light.directEnabled);
    const auto directional = state(light.directional.enabled)
        + (light.directional.enabled && !light.directEnabled ? "-" : "");
    const auto point = state(light.point.enabled) + (light.point.enabled && !light.directEnabled ? "-" : "");
    const auto reflection = state(light.environment.reflectionEnabled);
    const auto sky = state(light.environment.skyboxEnabled);

    std::string text;
    if (width < 480) {
        text = "1:" + direct + " 2:" + directional + " 3:" + point
            + "\n4:" + reflection + " 5:" + sky + " | 6 REG:" + state(regression)
            + "\nF5 " + std::string(shading == ShadingModel::Phong ? "PHONG" : "BLINN") + " | 0 RESET | F4";
    } else if (width < 800) {
        text = "LUCES | 0 RESET | F4 OCULTAR\nF5 " + model
            + "\n1 DIRECTA: " + direct
            + "\n2 DIRECCIONAL: " + directional + " | 3 PUNTUAL: " + point
            + "\n4 REFLEJOS: " + reflection + " | 5 CIELO: " + sky
            + "\n6 REGRESION: " + state(regression);
    } else {
        text = "ILUMINACION | F4 OCULTAR\nSOMBREADO: " + model + " | F5"
            + "\n1 DIRECTA: " + direct
            + "\n2 DIRECCIONAL: " + directional
            + "\n3 PUNTUAL: " + point
            + "\n4 REFLEJOS: " + reflection
            + "\n5 CIELO: " + sky
            + "\n6 REGRESION: " + state(regression)
            + "\n0 RESTAURAR ZONA";
    }
    if (!light.directEnabled && (light.directional.enabled || light.point.enabled)) text += "\n- SIN DIRECTA";
    return text;
}
}
