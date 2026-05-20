#version 460

layout(location = 0) out vec2 outUV;

out gl_PerVertex { vec4 gl_Position; };

// Fullscreen triangle — no vertex buffer needed.
// Vertices cover [-1,3]x[-1,3] in clip space; the rasteriser clips to [-1,1]x[-1,1].
void main() {
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}
