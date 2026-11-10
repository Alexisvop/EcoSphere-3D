#include "Scene.hpp"
#include "world/DemoSceneSettings.hpp"

namespace ecosphere {
Scene::Scene() {
    root_.addChild(demo::ground, demo::groundColor);
    root_.addChild(demo::redCube, demo::redColor);
    root_.addChild(demo::blueCube, demo::blueColor);
    auto& parent = root_.addChild(demo::parent, demo::parentColor);
    parent.addChild(demo::child, demo::childColor);
}
}
