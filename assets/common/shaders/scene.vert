#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float faceTint;
out float vertexTint;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    // Vectores columna: local -> mundo -> cámara -> clip.
    vertexTint = faceTint;
    gl_Position = projection * view * model * vec4(position, 1.0);
}
