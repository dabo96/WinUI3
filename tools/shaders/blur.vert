#version 450
// Fullscreen triangle for the dual-Kawase blur passes (brief 06). No vertex
// buffer: positions/uv are derived from gl_VertexIndex (draw 3 vertices).

layout(location = 0) out vec2 vUV;

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
