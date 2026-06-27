#version 450
// Multi-channel signed distance field text. Mirrors the GL MSDF shader.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    vec4 textColor;
    float pxRange;
} pc;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(uTexture, vUV).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    float screenPxDistance = pc.pxRange * (sd - 0.5)
        * dot(vec2(1.0) / fwidth(vUV), 1.0 / textureSize(uTexture, 0));
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    if (opacity < 0.01) discard;
    FragColor = vec4(pc.textColor.rgb, pc.textColor.a * opacity);
}
