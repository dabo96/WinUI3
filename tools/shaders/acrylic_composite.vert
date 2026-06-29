#version 450
// Acrylic/Mica composite (brief 06). Draws the panel quad in logical px from the
// push-constant center/half (triangle strip, draw 4 vertices — no vertex buffer).

layout(push_constant) uniform PC {
    mat4  projection;   // 0   (vertex)
    vec4  tint;         // 64
    vec2  center;       // 80
    vec2  halfSize;     // 88
    vec2  screenSize;   // 96
    float radius;       // 104
    float soft;         // 108
    float tintOpacity;  // 112
    float lumOpacity;   // 116
    float noiseAmount;  // 120
} pc;

layout(location = 0) out vec2 vLocal;

void main() {
    vec2 c = vec2((gl_VertexIndex == 1 || gl_VertexIndex == 3) ? 1.0 : -1.0,
                  (gl_VertexIndex >= 2) ? 1.0 : -1.0);
    vec2 ext = pc.halfSize + vec2(pc.soft + 2.0);
    vec2 local = c * ext;
    vLocal = local;
    gl_Position = pc.projection * vec4(pc.center + local, 0.0, 1.0);
}
