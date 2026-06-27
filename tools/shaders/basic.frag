#version 450
// Basic untextured fill (quads, lines). Color comes straight from the vertex.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vColor;
}
