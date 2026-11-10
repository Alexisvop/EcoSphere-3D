#include "Camera.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ecosphere {
double cameraDeltaSeconds(double elapsed, bool focused) {
    if (!std::isfinite(elapsed) || elapsed < 0) {
        throw std::invalid_argument("Camera deltaSeconds must be finite and nonnegative");
    }
    return focused ? std::min(elapsed, demo::maxDeltaSeconds) : 0.0;
}

void Camera::setMode(CameraMode mode) {
    mode_ = mode;
    if (mode_ == CameraMode::firstPerson) position.y = observerHeight;
}

// dirección de la mirada a partir de yaw y pitch
glm::vec3 Camera::forward() const {
    const float yaw = glm::radians(yawDegrees);
    const float pitch = glm::radians(pitchDegrees);
    return {std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
}

void Camera::update(const CameraInput& input, double deltaSeconds) {
    const float seconds = static_cast<float>(cameraDeltaSeconds(deltaSeconds, true));
    yawDegrees = std::remainder(yawDegrees + input.yaw * demo::lookSpeed * seconds, 360.0f);
    pitchDegrees = std::clamp(pitchDegrees + input.pitch * demo::lookSpeed * seconds,
                              -demo::pitchLimitDegrees, demo::pitchLimitDegrees);
    glm::vec3 direction = forward();
    const glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0)));
    // en primera persona se camina sobre el plano XZ
    if (mode_ == CameraMode::firstPerson) {
        direction.y = 0;
        direction = glm::normalize(direction);
    }
    glm::vec3 motion = direction * input.forward + right * input.right;
    if (mode_ == CameraMode::free) motion.y += input.vertical;
    // misma velocidad también en diagonal
    const float length = glm::length(motion);
    if (length > 1.0f) motion /= length;
    position += motion * speed * seconds;
    if (mode_ == CameraMode::firstPerson) position.y = observerHeight;
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0));
}

std::optional<glm::mat4> Camera::projection(int width, int height) const {
    if (width <= 0 || height <= 0) return std::nullopt;
    if (!std::isfinite(fovDegrees) || fovDegrees <= 0 || fovDegrees >= 180 ||
        demo::nearPlane <= 0 || demo::farPlane <= demo::nearPlane) {
        throw std::invalid_argument("Perspective requires 0 < FOV < 180 and 0 < near < far");
    }
    const float aspect = static_cast<float>(width) / height;
    return glm::perspective(glm::radians(fovDegrees), aspect, demo::nearPlane, demo::farPlane);
}
}
