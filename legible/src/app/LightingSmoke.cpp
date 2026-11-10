#include "LightingSmoke.hpp"
#include "world/LightingDemoSettings.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace ecosphere {
namespace {
using Pixels = std::vector<unsigned char>;
using Shots = std::map<std::string, Pixels>;

struct Region {
    int x0, y0, x1, y1;
};

// de sRGB a lineal
float linear(unsigned char channel) {
    const float value = channel / 255.0f;
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

Pixels readBackBuffer(const std::string& name, int width, int height) {
    Pixels pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Lighting readback failed: " + name);
    return pixels;
}

// imagen PPM con las filas de arriba hacia abajo
void writePpm(const std::filesystem::path& path, const Pixels& pixels, int width, int height) {
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    const auto stride = static_cast<std::size_t>(width) * 3;
    for (int y = height - 1; y >= 0; --y) {
        const auto* row = reinterpret_cast<const char*>(pixels.data() + y * stride);
        out.write(row, static_cast<std::streamsize>(stride));
    }
    out.close();
}

// cuadro en pantalla alrededor de un punto del mundo
Region projectRegion(const glm::mat4& viewProjection, glm::vec3 center, float radius, int width, int height) {
    const auto clip = viewProjection * glm::vec4(center, 1);
    const auto edge = viewProjection * glm::vec4(center + glm::vec3(radius, 0, 0), 1);
    if (clip.w <= 0 || edge.w <= 0) throw std::runtime_error("Lighting region behind camera");
    const float x = (clip.x / clip.w + 1) * 0.5f * width;
    const float y = (clip.y / clip.w + 1) * 0.5f * height;
    const int half = std::max(1, int(std::abs(edge.x / edge.w - clip.x / clip.w) * 0.5f * width));
    return Region{std::max(0, int(x) - half), std::max(0, int(y) - half),
                  std::min(width, int(x) + half), std::min(height, int(y) + half)};
}

std::size_t countChanged(const Pixels& first, const Pixels& second, Region region, int width) {
    std::size_t count = 0;
    for (int y = region.y0; y < region.y1; ++y) {
        for (int x = region.x0; x < region.x1; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * width + x) * 3;
            int delta = 0;
            for (int c = 0; c < 3; ++c) delta += std::abs(int(first[offset + c]) - int(second[offset + c]));
            if (delta > 12) ++count;
        }
    }
    return count;
}

void checkDifference(const Shots& shots, const char* first, const char* second, Region region, int width,
    const char* label) {
    const auto count = countChanged(shots.at(first), shots.at(second), region, width);
    std::cout << "LIGHTING " << label << ": changed=" << count << '\n';
    if (count < 30) throw std::runtime_error(std::string("Insufficient visual signal: ") + label);
}

struct AlphaCheck {
    std::size_t matches = 0;
    std::size_t tested = 0;
};

// el vidrio debe mezclar fondo y frente en espacio lineal
AlphaCheck checkLinearAlpha(const Shots& shots, Region glass, int width, float alpha) {
    const auto& background = shots.at("glass-zero");
    const auto& foreground = shots.at("glass-one");
    const auto& combined = shots.at("combined");
    AlphaCheck result;
    for (int y = glass.y0; y < glass.y1; ++y) {
        for (int x = glass.x0; x < glass.x1; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * width + x) * 3;
            float error = 0;
            float signal = 0;
            for (int c = 0; c < 3; ++c) {
                const float bg = linear(background[offset + c]);
                const float fg = linear(foreground[offset + c]);
                signal += std::abs(bg - fg);
                error += std::abs(linear(combined[offset + c]) - ((1 - alpha) * bg + alpha * fg));
            }
            if (signal > 0.1f) {
                ++result.tested;
                if (error < 0.025f) ++result.matches;
            }
        }
    }
    if (result.tested < 100 || result.matches < result.tested * 0.95)
        throw std::runtime_error("Alpha blend not linear / backdrop missing");
    return result;
}

struct UnlitCount {
    std::size_t magenta = 0;
    std::size_t cyan = 0;
};

// pixeles sin luz que no cambian al apagar luces
UnlitCount countUnlit(const Shots& shots) {
    const auto& combined = shots.at("combined");
    UnlitCount result;
    for (std::size_t offset = 0; offset < combined.size(); offset += 3) {
        const auto* pixel = &combined[offset];
        const bool magenta = pixel[0] > 240 && pixel[1] < 10 && pixel[2] > 240;
        const bool cyan = pixel[0] < 10 && pixel[1] > 240 && pixel[2] > 240;
        if (!magenta && !cyan) continue;
        bool stable = true;
        for (const auto* name : {"neither", "directional", "point", "environment-only"}) {
            for (int c = 0; c < 3; ++c) stable = stable && combined[offset + c] == shots.at(name)[offset + c];
        }
        if (!stable) continue;
        if (magenta) ++result.magenta;
        if (cyan) ++result.cyan;
    }
    if (result.magenta < 10 || result.cyan < 10)
        throw std::runtime_error("Imported unlit must remain invariant under lighting toggles");
    return result;
}
}

void checkLightingFramebuffer(const std::filesystem::path& directory, int width, int height,
    const glm::mat4& viewProjection, const LightingSettings& settings, Model& showcase,
    const std::function<void(const LightingSettings&)>& render) {
    std::filesystem::create_directories(directory);
    Shots shots;
    const auto capture = [&](const std::string& name, const LightingSettings& variant) {
        render(variant);
        shots[name] = readBackBuffer(name, width, height);
        writePpm(directory / (name + ".ppm"), shots[name], width, height);
    };

    // ---- Capturas con distintas luces ----
    auto variant = settings;
    capture("combined", variant);
    variant.environment.reflectionEnabled = false;
    capture("direct-only", variant);
    variant.ambient = 0;
    variant.point.enabled = false;
    capture("directional", variant);
    variant.directional.enabled = false;
    variant.point.enabled = true;
    capture("point", variant);
    variant = settings;
    variant.directEnabled = false;
    capture("environment-only", variant);
    variant.environment.reflectionEnabled = false;
    capture("neither", variant);
    variant = settings;
    variant.environment.fresnelPower = 1;
    capture("fresnel-power-one", variant);
    variant = settings;
    variant.environment.skyboxEnabled = false;
    capture("no-skybox", variant);

    // ---- Capturas cambiando materiales ----
    const float specular = showcase.meshes.at(1).material.specular;
    showcase.meshes.at(1).material.specular = 0;
    capture("no-specular", settings);
    showcase.meshes.at(1).material.specular = specular;
    const float alpha = showcase.meshes.at(4).color.a;
    showcase.meshes.at(4).color.a = 0;
    capture("glass-zero", settings);
    showcase.meshes.at(4).color.a = 1;
    capture("glass-one", settings);
    showcase.meshes.at(4).color.a = alpha;
    showcase.meshes.at(5).visible = false;
    capture("no-mask", settings);
    showcase.meshes.at(5).visible = true;

    // ---- Comparaciones por región ----
    const auto region = [&](glm::vec3 center, float radius) {
        return projectRegion(viewProjection, center, radius, width, height);
    };
    const auto check = [&](const char* first, const char* second, Region area, const char* label) {
        checkDifference(shots, first, second, area, width, label);
    };
    const auto matte = region(demo::materialPositions[0].position, 0.42f);
    const auto plastic = region(demo::materialPositions[1].position, 0.42f);
    const auto metal = region(demo::materialPositions[2].position, 0.42f);
    check("directional", "neither", matte, "directional on matte");
    check("point", "neither", plastic, "point on plastic");
    check("directional", "point", matte, "directional versus point");
    check("environment-only", "neither", metal, "environment on metal");
    check("combined", "direct-only", metal, "reflection with direct");
    check("combined", "environment-only", matte, "direct with reflection");
    check("combined", "no-specular", plastic, "plastic specular lobe");
    check("combined", "fresnel-power-one", region(demo::materialPositions[1].position, 0.65f),
        "Fresnel angular curve");
    check("combined", "no-skybox", {0, height * 3 / 4, width, height}, "skybox background");
    const auto glass = region(demo::glassPlacement.position, 0.35f);
    check("combined", "glass-zero", glass, "glass on backdrop");
    check("combined", "glass-one", glass, "backdrop through glass");
    const auto blend = checkLinearAlpha(shots, glass, width, alpha);

    // la máscara debe dejar huecos y partes opacas
    const auto mask = region(demo::maskPlacement.position, 0.38f);
    const auto maskChanged = countChanged(shots.at("combined"), shots.at("no-mask"), mask, width);
    const auto area = static_cast<std::size_t>(mask.x1 - mask.x0) * (mask.y1 - mask.y0);
    if (maskChanged < 30 || maskChanged > area * 0.85)
        throw std::runtime_error("MASK must retain holes and opaque pixels");

    const auto unlit = countUnlit(shots);
    std::cout << "LIGHTING unlit invariant: magenta=" << unlit.magenta << " cyan=" << unlit.cyan << '\n';
    std::cout << "LIGHTING PASS: linear alpha=" << alpha
              << " matched=" << blend.matches << '/' << blend.tested
              << "; MASK changed=" << maskChanged << '/' << area << "; framebuffer=" << width << 'x' << height
              << "; real captures=" << directory.string() << '\n';
}
}
