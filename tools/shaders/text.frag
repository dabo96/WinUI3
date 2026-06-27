#version 450
// Bitmap text: single-channel (R8) coverage atlas tinted by push-constant color.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    vec4 textColor;
    float pxRange;
} pc;

void main() {
    float alpha = texture(uTexture, vUV).r;
    FragColor = vec4(pc.textColor.rgb, pc.textColor.a * alpha);
}
