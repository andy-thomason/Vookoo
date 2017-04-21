#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 size;
layout(location = 2) in int sprite;
layout(location = 3) in float angle;
layout(location = 4) in vec4 colour;

layout(location = 0) out vec2 uv;

layout (binding = 0) uniform UBO {
  vec4 pixelsToScreen;
} ubo;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  float vtxRot = gl_VertexIndex * (3.14159/2) + 3.14159/4;
  vec2 local = vec2(cos(vtxRot), sin(vtxRot));
  mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
  vec2 world = pos + rot * (local * size * 0.5);
  vec2 p = world * ubo.pixelsToScreen.xy + ubo.pixelsToScreen.zw;
  gl_Position = vec4(p, 0.0, 1.0);
  uv = local * 0.25 + 0.25 + vec2((sprite&1) * 0.5, (sprite>>1) * 0.5);
}
