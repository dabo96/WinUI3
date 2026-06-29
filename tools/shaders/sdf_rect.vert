#version 450
// Instanced SDF rounded-rect vertex shader (briefs 01-04).
// Binding 0: per-vertex unit quad (aQuad in [-1,1]).
// Binding 1: per-instance SDFInstance fields.
// Push constants match the shared PushConstants block (projection reused; the
// reveal cursor is appended for brief 04, scalar-packed to match the C layout).

layout(location = 0) in vec2 aQuad;
layout(location = 1) in vec2 iCenter;
layout(location = 2) in vec2 iHalf;
layout(location = 3) in vec4 iParams;   // radius, borderW(/blur), softness, mode
layout(location = 4) in vec4 iFill;
layout(location = 5) in vec4 iBorder;
layout(location = 6) in float iReveal;

layout(push_constant) uniform PushConstants {
    mat4 projection;   // offset 0
    vec4 textColor;    // offset 64
    float pxRange;     // offset 80
    float revealX;     // offset 84
    float revealY;     // offset 88
    float revealRadius;// offset 92
} pc;

layout(location = 0) out vec2 vLocal;
layout(location = 1) flat out vec2 vHalf;
layout(location = 2) flat out float vRadius;
layout(location = 3) flat out float vBorderW;
layout(location = 4) flat out float vSoft;
layout(location = 5) flat out float vMode;
layout(location = 6) flat out vec4 vFill;
layout(location = 7) flat out vec4 vBorder;
layout(location = 8) flat out vec2 vCenter;
layout(location = 9) flat out float vReveal;

void main() {
    // Expand the quad to cover border + AA (and the penumbra for shadow mode:
    // iParams.y is borderWidth for fills, blur for shadows — same field).
    float pad = iParams.y + iParams.z + 2.0;
    vec2 local = aQuad * (iHalf + vec2(pad));
    vLocal   = local;
    vHalf    = iHalf;
    vRadius  = iParams.x;
    vBorderW = iParams.y;
    vSoft    = iParams.z;
    vMode    = iParams.w;
    vFill    = iFill;
    vBorder  = iBorder;
    vCenter  = iCenter;
    vReveal  = iReveal;
    gl_Position = pc.projection * vec4(iCenter + local, 0.0, 1.0);
}
