#pragma once
#include "world/DemoSceneSettings.hpp"
#include <optional>

namespace ecosphere {
enum class CameraMode { firstPerson, free };

// ejes del teclado entre -1 y 1
struct CameraInput {
    float forward = 0, right = 0, vertical = 0;
    float yaw = 0, pitch = 0;
};

double cameraDeltaSeconds(double elapsed, bool focused);

class Camera final {
public:
    glm::vec3 position = demo::cameraStart;
    float yawDegrees = demo::cameraYawDegrees;
    float pitchDegrees = demo::cameraPitchDegrees;
    float speed = demo::cameraSpeed;
    float observerHeight = demo::observerHeight;
    float fovDegrees = demo::fovDegrees;
    CameraMode mode() const { return mode_; }
    void setMode(CameraMode mode);
    void update(const CameraInput& input, double deltaSeconds);
    glm::mat4 view() const;
    // sin valor si la ventana está minimizada
    std::optional<glm::mat4> projection(int framebufferWidth, int framebufferHeight) const;
private:
    glm::vec3 forward() const;
    CameraMode mode_ = CameraMode::firstPerson;
};
}
