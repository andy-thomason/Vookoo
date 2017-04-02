#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 outNormal;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat3 normalToWorld;
  vec4 colour;
} u;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  gl_Position = u.modelToPerspective * vec4(inPosition.xyz, 1.0);
  outNormal = inNormal;
}

