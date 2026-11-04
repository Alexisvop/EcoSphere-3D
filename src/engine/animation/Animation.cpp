#include "Animation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
bool finite(glm::vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}
void validateSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Time must be finite and nonnegative");
}
void validatePose(const Pose& p) {
    const auto q = p.orientation;
    const float length = glm::length(q);
    if (!finite(p.position) || !finite(p.scale) || glm::any(glm::lessThanEqual(p.scale,glm::vec3(0.0001f))) ||
        !std::isfinite(q.w) || !std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) ||
        !std::isfinite(length) || length < 0.0001f)
        throw std::invalid_argument("Pose requires finite values, positive scale and nonzero quaternion");
}
glm::mat4 Pose::matrix() const {
    validatePose(*this);
    return glm::translate(glm::mat4(1),position) * glm::mat4_cast(glm::normalize(orientation)) * glm::scale(glm::mat4(1),scale);
}
AnimationPlayer::AnimationPlayer(double duration, bool repeat) : duration_(duration), repeat_(repeat) {
    validateSeconds(duration);
    if (duration == 0) throw std::invalid_argument("Animation duration must be positive");
}
void AnimationPlayer::setSpeed(double speed) { validateSeconds(speed); speed_ = speed; }
double AnimationPlayer::time() const { return repeat_ ? std::fmod(time_,duration_) : time_; }
void AnimationPlayer::update(double delta) {
    validateSeconds(delta);
    const double step = delta * speed_;
    if (!std::isfinite(step)) throw std::invalid_argument("Animation time overflow");
    if (!playing_) return;
    if (repeat_) {
        if (!std::isfinite(time_+step)) throw std::invalid_argument("Animation elapsed time overflow");
        time_ += step;
    } else if (step >= duration_ - time_) { time_ = duration_; playing_ = false; }
    else time_ += step;
}
KeyframeTrack::KeyframeTrack(std::vector<Keyframe> frames) : frames_(std::move(frames)) {
    if (frames_.empty()) throw std::invalid_argument("Keyframe track must not be empty");
    double previous = -1;
    for (auto& frame : frames_) {
        validateSeconds(frame.time); validatePose(frame.value);
        if (frame.time <= previous) throw std::invalid_argument("Keyframe times must be strictly increasing");
        previous = frame.time;
        frame.value.orientation = glm::normalize(frame.value.orientation);
    }
}
Pose KeyframeTrack::sample(double time) const {
    if (!std::isfinite(time)) throw std::invalid_argument("Keyframe sample must be finite");
    if (time <= frames_.front().time) return frames_.front().value;
    if (time >= frames_.back().time) return frames_.back().value;
    const auto next = std::upper_bound(frames_.begin(),frames_.end(),time,
        [](double t, const Keyframe& f) { return t < f.time; });
    const auto& a = *(next-1); const auto& b = *next;
    const float alpha = static_cast<float>((time-a.time)/(b.time-a.time));
    return {glm::mix(a.value.position,b.value.position,alpha),
        glm::normalize(glm::slerp(a.value.orientation,b.value.orientation,alpha)),
        glm::mix(a.value.scale,b.value.scale,alpha)};
}
Pose SimpleAnimation::sample(double seconds) const {
    validateSeconds(seconds); validateSeconds(frequency);
    if (motion!=SimpleMotion::rotation && motion!=SimpleMotion::periodicTranslation && motion!=SimpleMotion::oscillation)
        throw std::invalid_argument("Invalid simple motion");
    const float axisLength = glm::length(axis);
    if (!finite(axis) || !std::isfinite(axisLength) || axisLength < 0.0001f || !std::isfinite(amplitude))
        throw std::invalid_argument("Simple animation requires finite amplitude and nonzero axis");
    const auto direction = axis / axisLength;
    Pose result;
    if (motion == SimpleMotion::rotation) {
        const double angle = std::fmod(seconds,360.0/std::max(std::abs(double(amplitude)),1e-30)) * amplitude;
        result.orientation = glm::angleAxis(glm::radians(static_cast<float>(angle)),direction);
    } else {
        const double phase = frequency == 0 ? 0 : std::fmod(seconds,1.0/frequency)*frequency;
        if (motion == SimpleMotion::periodicTranslation)
            result.position = direction * amplitude * static_cast<float>(1-std::abs(2*phase-1));
        else result.orientation = glm::angleAxis(glm::radians(amplitude * static_cast<float>(std::sin(phase*6.283185307179586))),direction);
    }
    validatePose(result);
    return result;
}
Pose sampleProcedural(const ProceduralAnimation& function, double seconds) {
    validateSeconds(seconds);
    if (!function) throw std::invalid_argument("Procedural animation must exist");
    auto pose = function(seconds); validatePose(pose);
    pose.orientation = glm::normalize(pose.orientation);
    return pose;
}
}
