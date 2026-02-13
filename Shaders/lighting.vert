#version 450

// Trik membuat Fullscreen Triangle tanpa Vertex Buffer (Sangat Efisien)
// Kita generate koordinat berdasarkan index vertex (0, 1, 2)
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}