#version 330 core
in vec3 direction;
uniform samplerCube environmentMap;
out vec4 fragmentColor;
void main() {
    fragmentColor = vec4(texture(environmentMap, normalize(direction)).rgb, 1.0);
    // Salida lineal: GL_FRAMEBUFFER_SRGB codifica una sola vez, después de mezclar.
}
