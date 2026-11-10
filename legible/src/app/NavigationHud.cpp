#include "NavigationHud.hpp"
#include "RenderState.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace ecosphere {
namespace {
// acomoda el panel según la esquina pedida
NavigationHudLayout placeLayout(const std::string& message, int width, int height, HudAnchor anchor) {
    NavigationHudLayout layout = layoutNavigationHud(message, width, height);
    if (anchor == HudAnchor::navigation && height >= 160) layout.y -= 40;
    if (anchor == HudAnchor::topRight) {
        layout.x = width - 10 - layout.width;
        layout.y = height - 10 - layout.height;
    }
    if (anchor == HudAnchor::bottomRight) {
        layout.x = width - 10 - layout.width;
        layout.y = 10;
    }
    return layout;
}

// limpia la profundidad solo en el rectángulo del panel
void clearPanelDepth(const NavigationHudLayout& layout) {
    GLint box[4];
    glGetIntegerv(GL_SCISSOR_BOX, box);
    const auto scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLdouble oldClear;
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &oldClear);

    glEnable(GL_SCISSOR_TEST);
    glScissor(int(layout.x), int(layout.y), int(std::ceil(layout.width)), int(std::ceil(layout.height)));
    glDepthMask(GL_TRUE);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    glClearDepth(oldClear);

    glScissor(box[0], box[1], box[2], box[3]);
    if (!scissor) glDisable(GL_SCISSOR_TEST);
}
}

void NavigationHud::draw(const std::string& message, int width, int height, HudAnchor anchor) {
    // solo se rehace la malla del texto si algo cambió
    const bool changed = message != text_ || width != width_ || height != height_ || anchor != anchor_;
    if (!model_ || changed) {
        layout_ = placeLayout(message, width, height, anchor);
        renderer_.clear();
        model_.reset();
        text_ = message;
        width_ = width;
        height_ = height;
        anchor_ = anchor;
        if (!layout_.lines.empty()) model_ = makeNavigationHudModel(layout_);
    }
    if (!model_) return;

    RenderState restore;
    clearPanelDepth(layout_);
    // proyección ortográfica en píxeles
    const auto placement = glm::translate(glm::mat4(1), glm::vec3(layout_.x, layout_.y, 0));
    LightingSettings flat;
    flat.legacyUnlit = false;
    const auto projection = glm::ortho(0.f, float(width), 0.f, float(height), -1.f, 1.f);
    renderer_.draw(model_, placement, glm::mat4(1), projection, flat);
}
}
