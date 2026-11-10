#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <filesystem>

namespace ecosphere {
// cielo procedural: cubemap de 6 caras y su skybox
class Environment final {
public:
    explicit Environment(const std::filesystem::path& shaders);
    ~Environment();
    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    GLuint texture() const { return texture_; }
    void draw(const glm::mat4& view, const glm::mat4& projection) const;
private:
    void release() noexcept;
    GLuint texture_ = 0, program_ = 0, vao_ = 0;
    GLint inverseSky_ = -1, sampler_ = -1;
};
}
