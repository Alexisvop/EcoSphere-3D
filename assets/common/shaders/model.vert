#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
out vec2 textureUV;
out vec3 worldPosition;
out vec3 worldNormal;
uniform mat3 normalTransform;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    textureUV = uv;
    worldPosition = (model * vec4(position, 1.0)).xyz;
    worldNormal = normalTransform * normal;
    gl_Position = projection * view * model * vec4(position, 1.0);
}
