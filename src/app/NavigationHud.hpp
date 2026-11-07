#pragma once
#include "ModelRenderer.hpp"
#include "NavigationHudLayout.hpp"

namespace ecosphere {
enum class HudAnchor { navigation, topRight, bottomRight };
// Texto geométrico propio; usa el renderer y material unlit existentes.
class NavigationHud final {
public:
    explicit NavigationHud(const std::filesystem::path& shaders) : renderer_(shaders) {}
    void draw(const std::string& message,int width,int height,HudAnchor anchor=HudAnchor::navigation);
private:
    ModelRenderer renderer_;
    std::string text_;
    std::shared_ptr<const Model> model_;
    NavigationHudLayout layout_;
    int width_=0,height_=0;
    HudAnchor anchor_=HudAnchor::navigation;
};
}
