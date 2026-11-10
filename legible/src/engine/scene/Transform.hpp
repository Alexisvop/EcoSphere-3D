#pragma once
#include <glm/gtc/matrix_transform.hpp>

namespace ecosphere {
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};

    // matriz de modelo: T * Rz * Ry * Rx * S
    glm::mat4 matrix() const {
        glm::mat4 local = glm::translate(glm::mat4(1.0f), position);
        local = glm::rotate(local, glm::radians(rotationDegrees.z), glm::vec3(0, 0, 1));
        local = glm::rotate(local, glm::radians(rotationDegrees.y), glm::vec3(0, 1, 0));
        local = glm::rotate(local, glm::radians(rotationDegrees.x), glm::vec3(1, 0, 0));
        return glm::scale(local, scale);
    }
};
}
