#version 330 core
in vec3 direction;

uniform samplerCube environmentMap;

out vec4 fragmentColor;

void main() {
    // color del cubemap en esa dirección
    fragmentColor = vec4(texture(environmentMap, normalize(direction)).rgb, 1.0);
}
