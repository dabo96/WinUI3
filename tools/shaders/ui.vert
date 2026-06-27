#version 450
// Shared vertex shader for all FluentUI Vulkan pipelines.
// Matches the OpenGL RenderVertex layout: vec2 pos, vec4 color, vec2 uv.
// The projection matrix is supplied via push constants (same ortho matrix the
// Renderer computes for the GL backend). Y-flip is handled by a negative-height
// viewport in the backend, so the matrix stays identical to the GL one.

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUV;

layout(push_constant) uniform PushConstants {
    mat4 projection;   // offset 0,  64 bytes
    vec4 textColor;    // offset 64, 16 bytes
    float pxRange;     // offset 80, 4 bytes
} pc;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;

void main() {
    vColor = aColor;
    vUV = aUV;
    gl_Position = pc.projection * vec4(aPos, 0.0, 1.0);
}
