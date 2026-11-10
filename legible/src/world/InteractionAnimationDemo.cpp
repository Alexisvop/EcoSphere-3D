#include "InteractionAnimationDemo.hpp"
#include "InteractionAnimationDemoSettings.hpp"
#include "LightingDemo.hpp"
#include "LightingDemoSettings.hpp"
#include "DemoSceneSettings.hpp"
#include <glm/gtc/constants.hpp>
#include <limits>
#include <algorithm>
#include <cmath>

namespace ecosphere {
namespace {
// nombre de cada cubo, en el mismo orden que interactionDemo::bases
const std::array<const char*, 7> ids{{
    "simple-rotation",
    "simple-linear",
    "simple-oscillation",
    "keyframes",
    "procedural-sine",
    "procedural-orbit",
    "animated-child"
}};

const glm::vec4 simpleColor{0.1f, 0.7f, 0.8f, 1};
const glm::vec4 keyframeColor{0.85f, 0.2f, 0.6f, 1};
const glm::vec4 proceduralColor{0.4f, 0.85f, 0.1f, 1};

// caja que encierra todas las mallas del modelo
Aabb modelBounds(const Model& model) {
    const float infinity = std::numeric_limits<float>::infinity();
    Aabb bounds{glm::vec3(infinity), glm::vec3(-infinity)};
    model.forEach([&](std::size_t mesh, const glm::mat4& world) {
        for (const auto& vertex : model.meshes.at(mesh).vertices) {
            const auto position = glm::vec3(world * glm::vec4(vertex.position, 1));
            bounds.minimum = glm::min(bounds.minimum, position);
            bounds.maximum = glm::max(bounds.maximum, position);
        }
    });
    return bounds;
}
}

InteractionAnimationDemo::InteractionAnimationDemo(const Model& imported)
    : cubes(std::make_shared<Model>()), outline(std::make_shared<Model>()),
      player_(interactionDemo::duration), track_(interactionDemo::keyframes) {
    using namespace interactionDemo;
    imported.forEach([&](std::size_t, const glm::mat4& matrix) {
        importedMeshDeterminant =
            std::min(importedMeshDeterminant, std::abs(glm::determinant(glm::dmat3(matrix))));
    });
    player_.setSpeed(playbackSpeed);
    validateSeconds(sineFrequency);
    validateSeconds(orbitFrequency);

    // ---- Cubos animados ----
    for (std::size_t i = 0; i < ids.size(); ++i) {
        InteractiveObject object;
        object.id = ids[i];
        object.base = bases[i];
        object.bounds = selectionBounds;
        if (i == 6) object.parent = ids[5];
        interaction.add(object);

        auto mesh = makeUnitCube();
        mesh.material = demo::plastic;
        mesh.color = i < 3 ? simpleColor : i == 3 ? keyframeColor : proceduralColor;
        cubes->meshes.push_back(std::move(mesh));
        ModelNode node;
        node.name = ids[i];
        node.meshes = {i};
        cubes->nodes.push_back(node);
    }

    // modelo importado de Blender, también seleccionable
    InteractiveObject probe;
    probe.id = "imported";
    probe.bounds = modelBounds(imported);
    const auto& placement = demo::importedPlacement;
    probe.base = {placement.position, glm::quat(glm::radians(placement.rotationDegrees)), placement.scale};
    interaction.add(probe);

    // contorno amarillo de doce barras para el objeto seleccionado
    auto edge = makeUnitCube();
    edge.material.unlit = true;
    edge.color = {1, 0.8f, 0.02f, 1};
    outline->meshes.push_back(std::move(edge));
    for (int i = 0; i < 12; ++i) {
        ModelNode node;
        node.meshes = {0};
        outline->nodes.push_back(node);
    }
    evaluate();
    synchronize();
}

// calcula la pose animada de cada cubo en el tiempo actual
void InteractionAnimationDemo::evaluate() {
    using namespace interactionDemo;
    const double t = player_.elapsed();

    const SimpleAnimation spin{SimpleMotion::rotation, {0, 1, 0}, rotationSpeed, 0};
    const SimpleAnimation slide{SimpleMotion::periodicTranslation, {0, 1, 0}, linearAmplitude,
                                linearFrequency};
    const SimpleAnimation swing{SimpleMotion::oscillation, {0, 0, 1}, angularAmplitude, angularFrequency};
    interaction.object(ids[0]).animation = spin.sample(t);
    interaction.object(ids[1]).animation = slide.sample(t);
    interaction.object(ids[2]).animation = swing.sample(t);
    interaction.object(ids[3]).animation = track_.sample(player_.time());

    // sube, baja y se inclina con una onda senoidal
    interaction.object(ids[4]).animation = sampleProcedural([](double seconds) {
        const double cycle =
            sineFrequency == 0 ? 0 : std::fmod(seconds, 1.0 / sineFrequency) * sineFrequency;
        const float wave = static_cast<float>(std::sin(glm::two_pi<double>() * cycle));
        Pose pose;
        pose.position.y = sineAmplitude * wave;
        pose.orientation = glm::angleAxis(glm::radians(proceduralAngle * wave), glm::vec3(0, 0, 1));
        return pose;
    }, t);

    // da vueltas en círculo
    interaction.object(ids[5]).animation = sampleProcedural([](double seconds) {
        const double cycle =
            orbitFrequency == 0 ? 0 : std::fmod(seconds, 1.0 / orbitFrequency) * orbitFrequency;
        const float angle = static_cast<float>(glm::two_pi<double>() * cycle);
        Pose pose;
        pose.position = {orbitRadius * std::cos(angle), 0, orbitRadius * std::sin(angle)};
        pose.orientation = glm::angleAxis(angle, glm::vec3(0, 1, 0));
        return pose;
    }, t);

    const SimpleAnimation childSpin{SimpleMotion::rotation, {1, 0, 0}, rotationSpeed, 0};
    interaction.object(ids[6]).animation = childSpin.sample(t);
}

void InteractionAnimationDemo::update(double delta) {
    player_.update(delta);
    evaluate();
}

void InteractionAnimationDemo::restart() {
    player_.restart();
    evaluate();
}

// copia las poses a los nodos y coloca el contorno
void InteractionAnimationDemo::synchronize() {
    for (auto& node : cubes->nodes) node.local = interaction.world(node.name);
    if (!interaction.selected()) return;
    const auto& box = interaction.object(*interaction.selected()).bounds;
    const auto center = (box.minimum + box.maximum) * 0.5f;
    const auto size = box.maximum - box.minimum;
    int edge = 0;
    // cuatro barras paralelas a cada eje
    for (int axis = 0; axis < 3; ++axis) {
        const int second = (axis + 1) % 3;
        const int third = (axis + 2) % 3;
        for (int a : {-1, 1}) {
            for (int b : {-1, 1}) {
                Pose pose;
                pose.position = center;
                pose.scale = glm::vec3(0.025f);
                pose.scale[axis] = std::max(size[axis], 0.025f);
                pose.position[second] += a * size[second] * 0.5f;
                pose.position[third] += b * size[third] * 0.5f;
                outline->nodes.at(edge++).local = pose.matrix();
            }
        }
    }
}
}
