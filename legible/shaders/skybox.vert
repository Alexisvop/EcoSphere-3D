#version 330 core
out vec3 direction;

uniform mat4 inverseSky;

void main() {
    // un triángulo grande que cubre la pantalla, sin VBO
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    // dirección de mundo que se ve en esa esquina
    direction = (inverseSky * vec4(corner, 1.0, 1.0)).xyz;
    // z = 1 lo deja al fondo
    gl_Position = vec4(corner, 1.0, 1.0);
}
