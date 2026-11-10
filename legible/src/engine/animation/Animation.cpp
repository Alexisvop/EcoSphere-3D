#include "Animation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecosphere {
namespace {
constexpr double twoPi = 6.283185307179586;

bool finite(glm::vec3 vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool finite(glm::quat q) {
    return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z);
}
}

void validateSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        throw std::invalid_argument("Time must be finite and nonnegative");
    }
}

void validatePose(const Pose& pose) {
    const auto orientation = pose.orientation;
    const float length = glm::length(orientation);
    const bool scaleTooSmall = glm::any(glm::lessThanEqual(pose.scale, glm::vec3(0.0001f)));
    if (!finite(pose.position) || !finite(pose.scale) || scaleTooSmall || !finite(orientation) ||
        !std::isfinite(length) || length < 0.0001f) {
        throw std::invalid_argument("Pose requires finite values, positive scale and nonzero quaternion");
    }
}

// matriz T * R * S
glm::mat4 Pose::matrix() const {
    validatePose(*this);
    const auto translation = glm::translate(glm::mat4(1), position);
    const auto rotation = glm::mat4_cast(glm::normalize(orientation));
    const auto scaling = glm::scale(glm::mat4(1), scale);
    return translation * rotation * scaling;
}

// ---- Reproductor ----

AnimationPlayer::AnimationPlayer(double duration, bool repeat) : duration_(duration), repeat_(repeat) {
    validateSeconds(duration);
    if (duration == 0) throw std::invalid_argument("Animation duration must be positive");
}

void AnimationPlayer::setSpeed(double speed) {
    validateSeconds(speed);
    speed_ = speed;
}

double AnimationPlayer::time() const {
    return repeat_ ? std::fmod(time_, duration_) : time_;
}

void AnimationPlayer::update(double delta) {
    validateSeconds(delta);
    const double step = delta * speed_;
    if (!std::isfinite(step)) throw std::invalid_argument("Animation time overflow");
    if (!playing_) return;
    if (repeat_) {
        if (!std::isfinite(time_ + step)) throw std::invalid_argument("Animation elapsed time overflow");
        time_ += step;
        return;
    }
    // sin repetición se detiene al llegar al final
    if (step >= duration_ - time_) {
        time_ = duration_;
        playing_ = false;
    } else {
        time_ += step;
    }
}

// ---- Keyframes ----

KeyframeTrack::KeyframeTrack(std::vector<Keyframe> frames) : frames_(std::move(frames)) {
    if (frames_.empty()) throw std::invalid_argument("Keyframe track must not be empty");
    double previous = -1;
    for (auto& frame : frames_) {
        validateSeconds(frame.time);
        validatePose(frame.value);
        if (frame.time <= previous) throw std::invalid_argument("Keyframe times must be strictly increasing");
        previous = frame.time;
        frame.value.orientation = glm::normalize(frame.value.orientation);
    }
}

// interpolación entre los dos keyframes vecinos
Pose KeyframeTrack::sample(double time) const {
    if (!std::isfinite(time)) throw std::invalid_argument("Keyframe sample must be finite");
    if (time <= frames_.front().time) return frames_.front().value;
    if (time >= frames_.back().time) return frames_.back().value;
    const auto next = std::upper_bound(frames_.begin(), frames_.end(), time,
        [](double t, const Keyframe& frame) { return t < frame.time; });
    const auto& before = *(next - 1);
    const auto& after = *next;
    const float alpha = static_cast<float>((time - before.time) / (after.time - before.time));
    const auto& from = before.value;
    const auto& to = after.value;
    // posición y escala con mix (lineal), rotación con slerp
    const auto position = glm::mix(from.position, to.position, alpha);
    const auto orientation = glm::normalize(glm::slerp(from.orientation, to.orientation, alpha));
    const auto scale = glm::mix(from.scale, to.scale, alpha);
    return {position, orientation, scale};
}

// ---- Animaciones simples y procedurales ----

Pose SimpleAnimation::sample(double seconds) const {
    validateSeconds(seconds);
    validateSeconds(frequency);
    if (motion != SimpleMotion::rotation && motion != SimpleMotion::periodicTranslation &&
        motion != SimpleMotion::oscillation) {
        throw std::invalid_argument("Invalid simple motion");
    }
    const float axisLength = glm::length(axis);
    if (!finite(axis) || !std::isfinite(axisLength) || axisLength < 0.0001f || !std::isfinite(amplitude)) {
        throw std::invalid_argument("Simple animation requires finite amplitude and nonzero axis");
    }
    const auto direction = axis / axisLength;
    Pose result;
    if (motion == SimpleMotion::rotation) {
        // amplitude son grados por segundo
        const double secondsPerTurn = 360.0 / std::max(std::abs(double(amplitude)), 1e-30);
        const double angle = std::fmod(seconds, secondsPerTurn) * amplitude;
        result.orientation = glm::angleAxis(glm::radians(static_cast<float>(angle)), direction);
    } else {
        // phase va de 0 a 1 en cada ciclo
        const double phase = frequency == 0 ? 0 : std::fmod(seconds, 1.0 / frequency) * frequency;
        if (motion == SimpleMotion::periodicTranslation) {
            // onda triangular: ida y vuelta
            result.position = direction * amplitude * static_cast<float>(1 - std::abs(2 * phase - 1));
        } else {
            const float degrees = amplitude * static_cast<float>(std::sin(phase * twoPi));
            result.orientation = glm::angleAxis(glm::radians(degrees), direction);
        }
    }
    validatePose(result);
    return result;
}

Pose sampleProcedural(const ProceduralAnimation& function, double seconds) {
    validateSeconds(seconds);
    if (!function) throw std::invalid_argument("Procedural animation must exist");
    auto pose = function(seconds);
    validatePose(pose);
    pose.orientation = glm::normalize(pose.orientation);
    return pose;
}
}
