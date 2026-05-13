#version 450

layout(set=0, binding=0) uniform UBO { mat4 u_proj; };

// Per-instance (binding 1, rate=instance) — same memory layout as ShapeInstance.
layout(location=1) in vec4  i_xform;  // x, y, angle, radius
layout(location=2) in float i_seed;

layout(location=0) out vec2  v_local;
layout(location=1) out float v_angle;
layout(location=2) out float v_seed;

// Axis-aligned quad: 6 verts (2 triangles), no mesh VBO.
// v_local in [-1,1]x[-1,1]; world offset = v_local * radius.
const vec2 QUAD[6] = vec2[](
    vec2(-1.0,-1.0), vec2( 1.0,-1.0), vec2( 1.0, 1.0),
    vec2(-1.0,-1.0), vec2( 1.0, 1.0), vec2(-1.0, 1.0)
);

void main() {
    vec2 local = QUAD[gl_VertexIndex];
    gl_Position = u_proj * vec4(i_xform.xy + local * i_xform.w, 0.0, 1.0);
    v_local = local;
    v_angle = i_xform.z;
    v_seed  = i_seed;
}
