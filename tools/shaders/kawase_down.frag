#version 450
// Dual-Kawase downsample (brief 06). 5 taps. uHalfpixel = 0.5 / sourceTexSize.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(push_constant) uniform PC {
    vec2 uHalfpixel;
} pc;

void main() {
    vec4 sum = texture(uTex, vUV) * 4.0;
    sum += texture(uTex, vUV - pc.uHalfpixel.xy);
    sum += texture(uTex, vUV + pc.uHalfpixel.xy);
    sum += texture(uTex, vUV + vec2(pc.uHalfpixel.x, -pc.uHalfpixel.y));
    sum += texture(uTex, vUV - vec2(pc.uHalfpixel.x, -pc.uHalfpixel.y));
    FragColor = sum / 8.0;
}
