#pragma once
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <functional>
#include <vector>

namespace ecosphere {
// Pose CPU en T*R*S. No reemplaza Transform ni las matrices originales de glTF.
struct Pose {
    glm::vec3 position{0};
    glm::quat orientation{1,0,0,0};
    glm::vec3 scale{1};
    glm::mat4 matrix() const;
};
void validatePose(const Pose& pose);
void validateSeconds(double seconds);

class AnimationPlayer final {
public:
    explicit AnimationPlayer(double duration, bool repeat = true);
    void update(double deltaSeconds);
    void setSpeed(double speed);
    void setPlaying(bool playing) { playing_ = playing; }
    void restart() { time_ = 0; playing_ = true; }
    bool playing() const { return playing_; }
    double time() const;
    double elapsed() const { return time_; } // Tiempo total: las funciones continuas no saltan al repetir keyframes.
private:
    double duration_, time_ = 0, speed_ = 1;
    bool repeat_, playing_ = true;
};
struct Keyframe { double time; Pose value; };
class KeyframeTrack final {
public:
    explicit KeyframeTrack(std::vector<Keyframe> frames);
    Pose sample(double time) const;
private:
    std::vector<Keyframe> frames_;
};

enum class SimpleMotion { rotation, periodicTranslation, oscillation };
struct SimpleAnimation {
    SimpleMotion motion = SimpleMotion::rotation;
    glm::vec3 axis{0,1,0};
    float amplitude = 1; // Metros para traslación; grados para oscilación angular.
    double frequency = 0.5; // Ciclos/s; rotación usa grados/s en amplitude.
    Pose sample(double seconds) const;
};
// Procedural: el consumidor define una función pura, sin herencia ni recursos GPU.
using ProceduralAnimation = std::function<Pose(double)>;
Pose sampleProcedural(const ProceduralAnimation& function, double seconds);
}
