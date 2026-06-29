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
    if (a < 0.002) discard;
    FragColor = vec4(rgb, a);
}
)";

} // namespace Shaders
} // namespace FluentUI
