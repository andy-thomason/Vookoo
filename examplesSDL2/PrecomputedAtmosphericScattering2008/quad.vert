#version 450

// Fullscreen triangle — no VBO.  Three vertices cover [-1,1]^2 with one triangle.
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
