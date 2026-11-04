#version 330 core
out vec3 direction;
uniform mat4 inverseSky;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    direction = (inverseSky * vec4(p, 1.0, 1.0)).xyz;
    gl_Position = vec4(p, 1.0, 1.0); // Profundidad de fondo; LEQUAL y sin escritura.
}
