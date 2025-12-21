#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform vec2 uRectPos;
uniform vec2 uRectSize;

out vec2 TransformedTexCoord;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TransformedTexCoord = aTexCoord;
}
