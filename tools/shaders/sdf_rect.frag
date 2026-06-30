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

// --- Analytic gaussian rounded-box shadow (brief 11, Evan Wallace technique) ---
// Approximation of the error function (Abramowitz-Stegun 7.1.27), vectorized.
vec2 erf2(vec2 x) {
    vec2 s = sign(x), a = abs(x);
    vec2 r = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a*a)) * a) * a;
    r *= r;                 // ^2
    return s - s / (r*r);   // ^4
}

// Shadow coverage of a rounded box integrated in X for one row y.
float shadowX(float x, float y, float sigma, float corner, vec2 hs) {
    float delta  = min(hs.y - corner - abs(y), 0.0);
    float curved = hs.x - corner + sqrt(max(0.0, corner*corner - delta*delta));
    vec2 integral = 0.5 + 0.5 * erf2((x + vec2(-curved, curved)) * (0.70710678 / sigma));
    return integral.y - integral.x;
}

// Total coverage [0..1] of the shadow of a box [-hs,hs] at 'p' (relative to center).
float roundedBoxShadow(vec2 hs, vec2 p, float sigma, float corner) {
    float low  = p.y - hs.y;
    float high = p.y + hs.y;
    float start = clamp(-3.0 * sigma, low, high);
    float end   = clamp( 3.0 * sigma, low, high);
    float stepv = (end - start) / 4.0;
    float y = start + stepv * 0.5;
    float value = 0.0;
    const float invSqrt2pi = 0.39894228;
    for (int i = 0; i < 4; ++i) {
        float g = exp(-(y*y) / (2.0*sigma*sigma)) * (invSqrt2pi / sigma); // gaussian(y)
        value += shadowX(p.x, p.y - y, sigma, corner, hs) * g * stepv;
        y += stepv;
    }
    return clamp(value, 0.0, 1.0);
}

void main() {
    float d = sdRoundBox(vLocal, vHalf, vRadius);

    // mode == 1: drop shadow (brief 11). vBorderW aliases sigma (penumbra, px).
    // Analytic gaussian box shadow: correct contact-hardening penumbra (sharp at
    // the edge, diffuse far away) in a single pass.
    if (vMode > 0.5 && vMode < 1.5) {
        float sigma = max(vBorderW, 0.5);
        float corner = min(vRadius, min(vHalf.x, vHalf.y));
        float cov = roundedBoxShadow(vHalf, vLocal, sigma, corner);
        float sa = vFill.a * cov;
        if (sa < 0.002) discard;
        FragColor = vec4(vFill.rgb, sa);
        return;
    }

    // mode == 3: inner / inset shadow (brief 11). Darkens INSIDE the rect, stronger
    // near the inner edges. inset = (1 - coverage) clipped to the rect interior.
    if (vMode > 2.5) {
        float sigma = max(vBorderW, 0.5);
        float corner = min(vRadius, min(vHalf.x, vHalf.y));
        float aaMask = 1.0 - smoothstep(-vSoft, vSoft, d);   // 1 inside, 0 outside
        float cov = roundedBoxShadow(vHalf, vLocal, sigma, corner);
        float inset = (1.0 - cov) * aaMask;
        float sa = vFill.a * inset;
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
