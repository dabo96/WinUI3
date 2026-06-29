#pragma once

namespace FluentUI {
namespace Shaders {

// --- Basic UI Shaders (Quads and Lines) ---

const char* const VertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 uProjection;

out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* const FragmentShader = R"(
#version 450 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

// --- Bitmap Text Shaders ---

const char* const TextVertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main()
{
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* const TextFragmentShader = R"(
#version 450 core
in vec2 vUV;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uTextColor;

void main()
{
    float alpha = texture(uTexture, vUV).r;
    FragColor = vec4(uTextColor.rgb, uTextColor.a * alpha);
}
)";

// --- MSDF Text Shaders ---

const char* const MSDFVertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* const MSDFFragmentShader = R"(
#version 450 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uTextColor;
uniform float pxRange;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd = texture(uTexture, vUV).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    
    // Calculate screen-space opacity using fwidth for pixel-perfect sharpness
    float screenPxDistance = pxRange * (sd - 0.5) * dot(vec2(1.0) / fwidth(vUV), 1.0/textureSize(uTexture, 0));
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    if (opacity < 0.01) discard;
    FragColor = vec4(uTextColor.rgb, uTextColor.a * opacity);
}
)";

// --- Image Shaders (Full RGBA texture sampling with vertex color tint) ---

const char* const ImageVertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* const ImageFragmentShader = R"(
#version 450 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vUV);
    FragColor = texColor * vColor;
}
)";

// --- SDF Rounded-Rect Shaders (instanced; fill + border) ---
// A unit quad (location 0) is instanced per widget; per-instance attributes carry
// the rounded box parameters. The fragment shader resolves the shape analytically
// (no tessellation). mode==1 (shadow) is added in brief 03; reveal in brief 04.

const char* const SDFRectVertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aQuad;     // unit quad in [-1,1]
layout (location = 1) in vec2 iCenter;   // rect center (px)
layout (location = 2) in vec2 iHalf;     // half-size (px)
layout (location = 3) in vec4 iParams;   // radius, borderW(/blur), softness, mode
layout (location = 4) in vec4 iFill;     // fill rgba
layout (location = 5) in vec4 iBorder;   // border rgba
layout (location = 6) in float iReveal;  // reveal intensity (brief 04)

uniform mat4 uProjection;

out vec2 vLocal;
flat out vec2 vHalf;
flat out float vRadius;
flat out float vBorderW;
flat out float vSoft;
flat out float vMode;
flat out vec4 vFill;
flat out vec4 vBorder;
flat out vec2 vCenter;   // for reveal world-space reconstruction (brief 04)
flat out float vReveal;

void main() {
    // Expand the quad to cover border + AA (and, for shadow mode, the penumbra:
    // iParams.y holds borderWidth for fills and blur for shadows — same field).
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
    gl_Position = uProjection * vec4(iCenter + local, 0.0, 1.0);
}
)";

const char* const SDFRectFragmentShader = R"(
#version 450 core
in vec2 vLocal;
flat in vec2 vHalf;
flat in float vRadius;
flat in float vBorderW;
flat in float vSoft;
flat in float vMode;
flat in vec4 vFill;
flat in vec4 vBorder;
flat in vec2 vCenter;
flat in float vReveal;

uniform vec3 uReveal; // cursorX, cursorY, revealRadius (px). z<=0 disables (brief 04)

out vec4 FragColor;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    float d = sdRoundBox(vLocal, vHalf, vRadius);

    // mode == 1: drop shadow (brief 03). vBorderW aliases the penumbra (blur, px).
    // Exponential falloff: peak at the element edge (d=0), fading outward with a
    // soft tail. Inside the element (d<0) the fill covers it, so only d>0 matters.
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

    // Reveal highlight (brief 04): the edge lights up by proximity to the cursor.
    if (vReveal > 0.0 && uReveal.z > 0.0) {
        vec2 fragWorld = vCenter + vLocal;
        float dCursor = length(fragWorld - uReveal.xy);
        float prox = 1.0 - clamp(dCursor / uReveal.z, 0.0, 1.0);
        prox = prox * prox;                                   // soft curve
        float edge = (1.0 - smoothstep(-aa, aa, d)) - (1.0 - smoothstep(-aa, aa, d + 1.5));
        edge = clamp(edge, 0.0, 1.0);                          // ~1.5px edge band
        float revealA = prox * vReveal * edge;
        rgb = mix(rgb, vec3(1.0), revealA);
        a   = max(a, revealA * 0.9);
    }

    if (a < 0.002) discard;
    FragColor = vec4(rgb, a);
}
)";

// --- Backdrop blur (dual Kawase) + Acrylic composite (brief 06) ---
// A fullscreen quad ([-1,1] clip space, uv in [0,1]) feeds the two blur passes.
// The composite draws the panel quad in logical px (via uProjection) and samples
// the blurred backdrop in screen space (gl_FragCoord), masked by the rounded box.

const char* const BlurVertexShader = R"(
#version 450 core
layout (location = 0) in vec2 aPos;   // clip-space [-1,1]
layout (location = 1) in vec2 aUV;    // [0,1]
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* const KawaseDownFragment = R"(
#version 450 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uHalfpixel; // 0.5 / texSize (of the SOURCE texture)
void main() {
    vec4 sum = texture(uTex, vUV) * 4.0;
    sum += texture(uTex, vUV - uHalfpixel.xy);
    sum += texture(uTex, vUV + uHalfpixel.xy);
    sum += texture(uTex, vUV + vec2(uHalfpixel.x, -uHalfpixel.y));
    sum += texture(uTex, vUV - vec2(uHalfpixel.x, -uHalfpixel.y));
    FragColor = sum / 8.0;
}
)";

const char* const KawaseUpFragment = R"(
#version 450 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec2 uHalfpixel; // 0.5 / texSize (of the SOURCE texture)
void main() {
    vec4 sum = texture(uTex, vUV + vec2(-uHalfpixel.x * 2.0, 0.0));
    sum += texture(uTex, vUV + vec2(-uHalfpixel.x, uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(0.0, uHalfpixel.y * 2.0));
    sum += texture(uTex, vUV + vec2(uHalfpixel.x, uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(uHalfpixel.x * 2.0, 0.0));
    sum += texture(uTex, vUV + vec2(uHalfpixel.x, -uHalfpixel.y)) * 2.0;
    sum += texture(uTex, vUV + vec2(0.0, -uHalfpixel.y * 2.0));
    sum += texture(uTex, vUV + vec2(-uHalfpixel.x, -uHalfpixel.y)) * 2.0;
    FragColor = sum / 12.0;
}
)";

const char* const AcrylicCompositeVertexShader = R"(
#version 450 core
// Panel quad generated from gl_VertexID (triangle strip, 4 verts) — no VBO.
uniform mat4 uProjection;
uniform vec2 uCenter;   // rect center (logical px)
uniform vec2 uHalf;     // rect half-size (logical px)
uniform float uSoft;    // AA width (px)
out vec2 vLocal;
void main() {
    vec2 c = vec2((gl_VertexID == 1 || gl_VertexID == 3) ? 1.0 : -1.0,
                  (gl_VertexID >= 2) ? 1.0 : -1.0);
    vec2 ext = uHalf + vec2(uSoft + 2.0);
    vec2 local = c * ext;
    vLocal = local;
    gl_Position = uProjection * vec4(uCenter + local, 0.0, 1.0);
}
)";

const char* const AcrylicCompositeFragmentShader = R"(
#version 450 core
in vec2 vLocal;
out vec4 FragColor;

uniform sampler2D uBlur;     // blurred backdrop (full screen, framebuffer-oriented)
uniform sampler2D uNoiseTex; // 64x64 blue noise (R channel)
uniform vec2  uScreenSize;   // framebuffer px
uniform vec2  uHalf;         // rect half-size (logical px)
uniform float uRadius;       // corner radius (logical px)
uniform float uSoft;         // AA width (px)
uniform vec3  uTint;
uniform float uTintOpacity;
uniform float uLuminosityOpacity;
uniform float uNoiseAmount;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    // Screen-space UV. gl_FragCoord and the blurred texture share the GL
    // bottom-left framebuffer orientation, so no Y flip is needed.
    vec2 screenUV = gl_FragCoord.xy / uScreenSize;
    vec3 backdrop = texture(uBlur, screenUV).rgb;
    vec3 lum = vec3(dot(backdrop, vec3(0.299, 0.587, 0.114)));
    vec3 col = mix(backdrop, lum, uLuminosityOpacity);
    col = mix(col, uTint, uTintOpacity);
    float n = texture(uNoiseTex, gl_FragCoord.xy / 64.0).r - 0.5;
    col += n * uNoiseAmount;

    float d = sdRoundBox(vLocal, uHalf, uRadius);
    float aa = max(uSoft, fwidth(d));
    float mask = 1.0 - smoothstep(-aa, aa, d);
    if (mask < 0.002) discard;
    FragColor = vec4(col, mask);
}
)";

} // namespace Shaders
} // namespace FluentUI
