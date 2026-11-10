#pragma once
#include <glad/gl.h>

namespace ecosphere {
class Triangle final {
public:
    Triangle();
    ~Triangle();
    Triangle(const Triangle&) = delete;
    Triangle& operator=(const Triangle&) = delete;
    void draw() const;
private:
    void release() noexcept;
    GLuint program_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
};
}
