#include "Triangle.hpp"
#include "Shader.hpp"
#include <stdexcept>

namespace {
constexpr const char* vertexSource = R"(#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
out vec3 vertexColor;
void main() {
    // sin cámara, la posición ya está en coordenadas de clip
    gl_Position = vec4(position, 0.0, 1.0);
    vertexColor = color;
}
)";
constexpr const char* fragmentSource = R"(#version 330 core
in vec3 vertexColor;
out vec4 fragmentColor;
void main() {
    // el color llega interpolado entre los tres vértices
    fragmentColor = vec4(vertexColor, 1.0);
}
)";
}

namespace ecosphere {
Triangle::Triangle() {
    try {
        Shader vertex(GL_VERTEX_SHADER, vertexSource);
        Shader fragment(GL_FRAGMENT_SHADER, fragmentSource);
        program_ = linkProgram(vertex.handle, fragment.handle);
        // cada fila es posición x, y y color r, g, b
        constexpr float vertices[] = {
            -0.65f, -0.55f, 1.0f, 0.2f, 0.1f,
             0.65f, -0.55f, 0.1f, 1.0f, 0.2f,
             0.0f,   0.65f, 0.2f, 0.3f, 1.0f
        };
        constexpr GLsizei vertexStrideBytes = 5 * sizeof(float);
        // el VBO guarda los datos y el VAO cómo leerlos
        glGenVertexArrays(1, &vertexArray_);
        glGenBuffers(1, &vertexBuffer_);
        glBindVertexArray(vertexArray_);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertexStrideBytes, nullptr);
        glEnableVertexAttribArray(0);
        // el color empieza después de los dos floats de posición
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexStrideBytes,
                              reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("OpenGL failed to allocate triangle resources");
        }
    } catch (...) {
        release();
        throw;
    }
}

Triangle::~Triangle() { release(); }

void Triangle::release() noexcept {
    glDeleteBuffers(1, &vertexBuffer_);
    glDeleteVertexArrays(1, &vertexArray_);
    glDeleteProgram(program_);
}

void Triangle::draw() const {
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
}
