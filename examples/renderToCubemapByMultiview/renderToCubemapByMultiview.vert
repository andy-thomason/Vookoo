#version 460
#extension GL_EXT_multiview : enable

layout(location = 0) in vec4 a_position;

layout(location = 0) out vec4 v_color;

layout (push_constant) uniform PushConstant {
  vec4 color[6]; // 6 for all the cubemap faces to be rendered
} p;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
  mat4 projection;
  mat4 view;
  mat4 world;
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  v_color = p.color[gl_ViewIndex];
  gl_Position = u.projection * u.view * a_position;
}
