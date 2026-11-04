#version 330 core
in float vertexTint;
uniform vec3 objectColor;
out vec4 fragmentColor;
void main() {
    fragmentColor = vec4(objectColor * vertexTint, 1.0);
}
