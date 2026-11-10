#version 330 core
// atributos del VBO, mismas location que en GpuMesh
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

out vec2 textureUV;
out vec3 worldPosition;
out vec3 worldNormal;

// matriz de normales: inversa transpuesta de la parte 3x3 de model
uniform mat3 normalTransform;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    textureUV = uv;
    worldPosition = (model * vec4(position, 1.0)).xyz;
    worldNormal = normalTransform * normal;
    // local -> mundo -> cámara -> clip
    gl_Position = projection * view * model * vec4(position, 1.0);
}
