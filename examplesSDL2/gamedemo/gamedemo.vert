#version 450

layout(set=0, binding=0) uniform UBO { mat4 u_proj; };

layout(location=0) in vec2  a_pos;     // per-vertex: local-space position
layout(location=1) in vec4  i_xform;   // per-instance: x, y, angle (rad), scale
layout(location=2) in float i_color;   // per-instance: palette index

layout(location=0) out flat float v_color;

void main() {
    float c = cos(i_xform.z), s = sin(i_xform.z);
    vec2 rotated = vec2(c * a_pos.x - s * a_pos.y,
                        s * a_pos.x + c * a_pos.y);
    gl_Position = u_proj * vec4(rotated * i_xform.w + i_xform.xy, 0.0, 1.0);
    v_color = i_color;
}
