#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float faceTint;

out float vertexTint;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vertexTint = faceTint;
    // local -> mundo -> cámara -> clip
    gl_Position = projection * view * model * vec4(position, 1.0);
}
