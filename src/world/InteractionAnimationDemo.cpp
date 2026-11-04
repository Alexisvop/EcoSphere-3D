#include "InteractionAnimationDemo.hpp"
#include "InteractionAnimationDemoSettings.hpp"
#include "LightingDemo.hpp"
#include "LightingDemoSettings.hpp"
#include "DemoSceneSettings.hpp"
#include <glm/gtc/constants.hpp>
#include <limits>

namespace ecosphere {
namespace {
const std::array<const char*,7> ids{{"simple-rotation","simple-linear","simple-oscillation","keyframes","procedural-sine","procedural-orbit","animated-child"}};
Aabb modelBounds(const Model& model) {
    Aabb bounds{glm::vec3(std::numeric_limits<float>::infinity()),glm::vec3(-std::numeric_limits<float>::infinity())};
    model.forEach([&](std::size_t mesh, const glm::mat4& world) {
        for (const auto& vertex : model.meshes.at(mesh).vertices) {
            const auto position = glm::vec3(world*glm::vec4(vertex.position,1));
            bounds.minimum = glm::min(bounds.minimum,position); bounds.maximum = glm::max(bounds.maximum,position);
        }
    });
    return bounds;
}
}
InteractionAnimationDemo::InteractionAnimationDemo(const Model& imported)
    : cubes(std::make_shared<Model>()), outline(std::make_shared<Model>()),
      player_(interactionDemo::duration), track_(interactionDemo::keyframes) {
    using namespace interactionDemo;
    player_.setSpeed(playbackSpeed);
    validateSeconds(sineFrequency); validateSeconds(orbitFrequency);
    for (std::size_t i=0; i<ids.size(); ++i) {
        InteractiveObject object; object.id = ids[i]; object.base = bases[i]; object.bounds = selectionBounds;
        if (i==6) object.parent = ids[5];
        interaction.add(object);
        auto mesh = makeUnitCube(); mesh.material = demo::plastic;
        mesh.color = i<3 ? glm::vec4(0.1f,0.7f,0.8f,1) : i==3 ? glm::vec4(0.85f,0.2f,0.6f,1) : glm::vec4(0.4f,0.85f,0.1f,1);
        cubes->meshes.push_back(std::move(mesh));
        ModelNode node; node.name = ids[i]; node.meshes = {i}; cubes->nodes.push_back(node);
    }
    InteractiveObject probe; probe.id = "imported"; probe.bounds = modelBounds(imported);
    probe.base = {demo::importedPlacement.position,glm::quat(glm::radians(demo::importedPlacement.rotationDegrees)),demo::importedPlacement.scale};
    interaction.add(probe);
    auto edge = makeUnitCube(); edge.material.unlit = true; edge.color = {1,0.8f,0.02f,1};
    outline->meshes.push_back(std::move(edge));
    // Doce barras finas rodean el volumen sin cambiar material ni shader del objeto.
    for (int i=0; i<12; ++i) { ModelNode node; node.meshes = {0}; outline->nodes.push_back(node); }
    evaluate(); synchronize();
}
void InteractionAnimationDemo::evaluate() {
    using namespace interactionDemo;
    const double t = player_.elapsed();
    interaction.object(ids[0]).animation = SimpleAnimation{SimpleMotion::rotation,{0,1,0},rotationSpeed,0}.sample(t);
    interaction.object(ids[1]).animation = SimpleAnimation{SimpleMotion::periodicTranslation,{0,1,0},linearAmplitude,linearFrequency}.sample(t);
    interaction.object(ids[2]).animation = SimpleAnimation{SimpleMotion::oscillation,{0,0,1},angularAmplitude,angularFrequency}.sample(t);
    interaction.object(ids[3]).animation = track_.sample(player_.time());
    interaction.object(ids[4]).animation = sampleProcedural([](double seconds) {
        Pose p; const float wave = static_cast<float>(std::sin(glm::two_pi<double>()*(sineFrequency == 0 ? 0 : std::fmod(seconds,1.0/sineFrequency)*sineFrequency)));
        p.position.y = sineAmplitude*wave;
        p.orientation = glm::angleAxis(glm::radians(proceduralAngle*wave),glm::vec3(0,0,1)); return p;
    },t);
    interaction.object(ids[5]).animation = sampleProcedural([](double seconds) {
        Pose p; const float angle = static_cast<float>(glm::two_pi<double>()*(orbitFrequency == 0 ? 0 : std::fmod(seconds,1.0/orbitFrequency)*orbitFrequency));
        p.position = {orbitRadius*std::cos(angle),0,orbitRadius*std::sin(angle)};
        p.orientation = glm::angleAxis(angle,glm::vec3(0,1,0)); return p;
    },t);
    interaction.object(ids[6]).animation = SimpleAnimation{SimpleMotion::rotation,{1,0,0},rotationSpeed,0}.sample(t);
}
void InteractionAnimationDemo::update(double delta) { player_.update(delta); evaluate(); }
void InteractionAnimationDemo::restart() { player_.restart(); evaluate(); }
void InteractionAnimationDemo::synchronize() {
    for (auto& node : cubes->nodes) node.local = interaction.world(node.name);
    if (!interaction.selected()) return;
    const auto& box = interaction.object(*interaction.selected()).bounds;
    const auto center = (box.minimum+box.maximum)*0.5f, size = box.maximum-box.minimum;
    int edge = 0;
    for (int axis=0; axis<3; ++axis) for (int a : {-1,1}) for (int b : {-1,1}) {
        Pose pose; pose.position = center; pose.scale = glm::vec3(0.025f);
        pose.scale[axis] = std::max(size[axis],0.025f);
        pose.position[(axis+1)%3] += a*size[(axis+1)%3]*0.5f;
        pose.position[(axis+2)%3] += b*size[(axis+2)%3]*0.5f;
        outline->nodes.at(edge++).local = pose.matrix();
    }
}
}
