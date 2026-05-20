#version 450

layout(location=0) out vec2 v_uv;

// Fullscreen triangle — no VBO needed.
// Vertices: (-1,-1),(3,-1),(-1,3) with UVs (0,0),(2,0),(0,2).
// The oversized triangle is clipped to the viewport, covering the full screen.
void main() {
    vec2 pos = vec2(
        (gl_VertexIndex == 1) ? 3.0 : -1.0,
        (gl_VertexIndex == 2) ? 3.0 : -1.0
    );
    v_uv        = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
