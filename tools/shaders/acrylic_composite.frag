#version 450
// Acrylic/Mica composite (brief 06): tint + luminosity + noise over the blurred
// backdrop, masked to the rounded rect. uBlur is the dual-Kawase result (screen
// oriented); uNoise is the 64x64 blue-noise texture.

layout(location = 0) in vec2 vLocal;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uBlur;
layout(set = 0, binding = 1) uniform sampler2D uNoise;

layout(push_constant) uniform PC {
    mat4  projection;
    vec4  tint;
    vec2  center;
    vec2  halfSize;
    vec2  screenSize;
    float radius;
    float soft;
    float tintOpacity;
    float lumOpacity;
    float noiseAmount;
} pc;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    vec2 screenUV = gl_FragCoord.xy / pc.screenSize;
    vec3 backdrop = texture(uBlur, screenUV).rgb;
    vec3 lum = vec3(dot(backdrop, vec3(0.299, 0.587, 0.114)));
    vec3 col = mix(backdrop, lum, pc.lumOpacity);
    col = mix(col, pc.tint.rgb, pc.tintOpacity);
    float n = texture(uNoise, gl_FragCoord.xy / 64.0).r - 0.5;
    col += n * pc.noiseAmount;

    float d = sdRoundBox(vLocal, pc.halfSize, pc.radius);
    float aa = max(pc.soft, fwidth(d));
    float mask = 1.0 - smoothstep(-aa, aa, d);
    if (mask < 0.002) discard;
    FragColor = vec4(col, mask);
}
