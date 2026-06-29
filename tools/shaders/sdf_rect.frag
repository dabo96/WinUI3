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
    float d = sdRoundBox(vLocal, vHalf, vRadius);

    // mode == 1: drop shadow (brief 03). vBorderW aliases the penumbra (blur, px).
    if (vMode > 0.5) {
        float blur = vBorderW;
        float t = clamp(d / max(blur, 0.5), 0.0, 1.0);
        float falloff = exp(-3.0 * t);
        float sa = vFill.a * falloff;
        if (sa < 0.002) discard;
        FragColor = vec4(vFill.rgb, sa);
        return;
    }

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

    // Reveal highlight (brief 04): edge lights up by proximity to the cursor.
    if (vReveal > 0.0 && pc.revealRadius > 0.0) {
        vec2 cursor = vec2(pc.revealX, pc.revealY);
        vec2 fragWorld = vCenter + vLocal;
        float dCursor = length(fragWorld - cursor);
        float prox = 1.0 - clamp(dCursor / pc.revealRadius, 0.0, 1.0);
        prox = prox * prox;
        float edge = (1.0 - smoothstep(-aa, aa, d)) - (1.0 - smoothstep(-aa, aa, d + 1.5));
        edge = clamp(edge, 0.0, 1.0);
        float revealA = prox * vReveal * edge;
        rgb = mix(rgb, vec3(1.0), revealA);
        a   = max(a, revealA * 0.9);
    }

    if (a < 0.002) discard;
    FragColor = vec4(rgb, a);
}
