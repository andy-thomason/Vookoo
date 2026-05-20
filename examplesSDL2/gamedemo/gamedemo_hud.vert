#version 450

layout(set=0, binding=0) uniform UBO { mat4 u_proj; };

layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;

layout(location=0) out vec2 v_uv;

void main() {
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
