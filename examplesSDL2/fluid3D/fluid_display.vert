#version 450

layout(location = 0) out vec2 outUV;

out gl_PerVertex { vec4 gl_Position; };

// Fullscreen triangle — no vertex buffer.
// UV (0,0) = top-left, (1,1) = bottom-right of screen.
void main() {
    outUV       = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}
