#version 450 core
in vec2 TransformedTexCoord;

out vec4 FragColor;

uniform sampler2D uImageTexture;
uniform vec4 uTintColor;
uniform float uOpacity;

void main() {
    vec4 texColor = texture(uImageTexture, TransformedTexCoord);
    FragColor = texColor * uTintColor * vec4(1.0, 1.0, 1.0, uOpacity);
}
