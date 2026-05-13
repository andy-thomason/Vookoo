#version 450

struct Particle {
    vec2  pos;
    vec2  vel;
    float life;
    float size;
    float pad0;
    float pad1;
};

layout(std430, set=1, binding=0) readonly buffer Particles { Particle p[]; };
layout(set=0, binding=0) uniform UBO { mat4 proj; } ubo;

layout(location=0) out float v_life;
layout(location=1) out vec2  v_uv;
layout(location=2) out vec3  v_color;

// Viridis colormap — quintic polynomial fit (Matplotlib, public domain).
// t=0 → dark purple, t=1 → bright yellow.
vec3 viridis(float t) {
    vec3 c0 = vec3( 0.2777, 0.0054, 0.3341);
    vec3 c1 = vec3( 0.1051, 1.4046, 1.3846);
    vec3 c2 = vec3(-0.3309, 0.2148, 0.0951);
    vec3 c3 = vec3(-4.6342,-5.7991,-19.332);
    vec3 c4 = vec3( 6.2283,14.1799, 56.691);
    vec3 c5 = vec3( 4.7764,-13.745,-65.353);
    vec3 c6 = vec3(-5.4355, 4.6459, 26.312);
    return clamp(c0 + t*(c1 + t*(c2 + t*(c3 + t*(c4 + t*(c5 + t*c6))))), 0.0, 1.0);
}

void main() {
    int pid = gl_VertexIndex / 6;
    int vid = gl_VertexIndex % 6;

    // Two CCW triangles: corners (-1,-1)(1,-1)(1,1) and (-1,-1)(1,1)(-1,1).
    const vec2 corners[6] = vec2[6](
        vec2(-1,-1), vec2( 1,-1), vec2( 1, 1),
        vec2(-1,-1), vec2( 1, 1), vec2(-1, 1));

    Particle pt = p[pid];
    v_life      = pt.life;
    v_uv        = corners[vid] * 0.5 + 0.5;
    v_color     = viridis(pt.life);  // young=yellow, old=dark purple
    gl_Position = ubo.proj * vec4(pt.pos + corners[vid] * pt.size, 0.0, 1.0);
}
