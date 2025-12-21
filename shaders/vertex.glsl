#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform vec2 uRectPos;
uniform vec2 uRectSize;
uniform float uCornerRadius;

out vec4 vColor;
out vec2 vPos;
out vec2 vRectPos;
out vec2 vRectSize;
out float vCornerRadius;

void main() {
    vColor = aColor;
    vPos = aPos;
    vRectPos = uRectPos;
    vRectSize = uRectSize;
    vCornerRadius = uCornerRadius;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}