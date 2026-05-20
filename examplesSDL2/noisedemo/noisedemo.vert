#version 450
// Modified from webgl-noise — https://github.com/ashima/webgl-noise

layout(location = 0) in vec3 a_pos;

// Unit-sphere position, doubles as the 3D noise coordinate.
// All fragment shader variants read location 0 and swizzle what they need.
layout(location = 0) out vec3 v_coord;

layout(push_constant) uniform PC {
    mat4  mvp;
    float time;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(a_pos, 1.0);
    v_coord     = a_pos;
}
