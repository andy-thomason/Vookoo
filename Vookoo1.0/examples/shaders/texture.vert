#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 pos_in;
layout (location = 1) in vec3 normal_in;
layout (location = 2) in vec2 uv_in;

layout (binding = 0) uniform UBO 
{
  mat4 viewToProjection;
  mat4 modelToWorld;
  mat4 worldToView;
  mat4 normalToWorld;
  vec4 worldLightPosition;
} ubo;

layout (location = 0) out vec2 uv_out;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  uv_out = uv_in;
  mat4 modelToView = ubo.worldToView * ubo.modelToWorld;
  vec4 viewPos = modelToView * pos_in;
  gl_Position = ubo.viewToProjection * viewPos;
}


