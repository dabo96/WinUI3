#version 450
// Dual-Kawase upsample (brief 06). 8 taps. uHalfpixel = 0.5 / sourceTexSize.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(push_constant) uniform PC {
    vec2 uHalfpixel;
} pc;

void main() {
    vec4 sum = texture(uTex, vUV + vec2(-pc.uHalfpixel.x * 2.0, 0.0));
    sum += texture(uTex, vUV + vec2(-pc.uHalfpixel.x, pc.uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(0.0, pc.uHalfpixel.y * 2.0));
    sum += texture(uTex, vUV + vec2(pc.uHalfpixel.x, pc.uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(pc.uHalfpixel.x * 2.0, 0.0));
    sum += texture(uTex, vUV + vec2(pc.uHalfpixel.x, -pc.uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(0.0, -pc.uHalfpixel.y * 2.0));
    sum += texture(uTex, vUV + vec2(-pc.uHalfpixel.x, -pc.uHalfpixel.y)) * 2.0;
    FragColor = sum / 12.0;
}
