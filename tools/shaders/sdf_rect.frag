#version 450
// Instanced SDF rounded-rect fragment shader (brief 01: fill + border).
// mode==1 (shadow) added in brief 03; reveal highlight added in brief 04.

layout(location = 0) in vec2 vLocal;
layout(location = 1) flat in vec2 vHalf;
layout(location = 2) flat in float vRadius;
layout(location = 3) flat in float vBorderW;
layout(location = 4) flat in float vSoft;
layout(location = 5) flat in float vMode;
layout(location = 6) flat in vec4 vFill;
layout(location = 7) flat in vec4 vBorder;
layout(location = 8) flat in vec2 vCenter;
layout(location = 9) flat in float vReveal;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    vec4 textColor;
    float pxRange;
    float revealX;
    float revealY;
    float revealRadius;
} pc;

layout(location = 0) out vec4 FragColor;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    float d  = sdRoundBox(vLocal, vHalf, vRadius);
    float aa = max(vSoft, fwidth(d));
    float fillCov = 1.0 - smoothstep(-aa, aa, d);
    vec3  rgb = vFill.rgb;
    float a   = vFill.a * fillCov;
    if (vBorderW > 0.0) {
        float inner = 1.0 - smoothstep(-aa, aa, d + vBorderW);
        float borderCov = clamp(fillCov - inner, 0.0, 1.0);
        rgb = mix(rgb, vBorder.rgb, borderCov);
        a   = max(a, vBorder.a * borderCov);
    }
    if (a < 0.002) discard;
    FragColor = vec4(rgb, a);
}
