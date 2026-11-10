#version 330 core
in float vertexTint;

uniform vec3 objectColor;

out vec4 fragmentColor;

void main() {
    // color del objeto oscurecido según la cara
    fragmentColor = vec4(objectColor * vertexTint, 1.0);
}
