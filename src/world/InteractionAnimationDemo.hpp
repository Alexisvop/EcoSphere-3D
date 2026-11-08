#pragma once
#include "animation/Animation.hpp"
#include "interaction/Interaction.hpp"
#include "assets/Model.hpp"
#include <memory>

namespace ecosphere {
// Dueño CPU de la demo. La aplicación presta matrices/modelos al renderer existente.
class InteractionAnimationDemo final {
public:
    explicit InteractionAnimationDemo(const Model& imported);
    void update(double deltaSeconds);
    void restart();
    void togglePause() { player_.setPlaying(!player_.playing()); }
    double time() const { return player_.elapsed(); }
    bool playing() const { return player_.playing(); }
    void synchronize();
    InteractionSystem interaction;
    std::shared_ptr<Model> cubes, outline;
    double importedMeshDeterminant = 1;
private:
    AnimationPlayer player_;
    KeyframeTrack track_;
    void evaluate();
};
}
