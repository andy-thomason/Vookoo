#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
 mat4 u_projection;
 mat4 u_view;
 mat4 u_world;
} u;

#define u_projection u.u_projection
#define u_view u.u_view
#define u_world u.u_world

layout(location = 0) out vec2 v_uv;

void main() {
  v_uv = a_uv;

  gl_Position = u_projection * u_view * u_world * vec4(a_position, 1);
}
