#pragma once
#include <stdexcept>

namespace ecosphere {
// modelo especular: Blinn-Phong (vector medio) o Phong (vector reflejado)
enum class ShadingModel { BlinnPhong, Phong };

inline const char* shadingName(ShadingModel model) {
    switch (model) {
    case ShadingModel::BlinnPhong:
        return "BLINN-PHONG";
    case ShadingModel::Phong:
        return "PHONG";
    }
    throw std::invalid_argument("Invalid shading model");
}
}
