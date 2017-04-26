#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inColour;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  vec4 colour;
  float pointScale;
} u;

void main() {
  /*vec2 diff = gl_PointCoord - vec2(0.5, 0.5);
  float r2 = dot(diff, diff);
  float val = (0.25 - r2)*4.0;
  outColor = vec4(inColour * val, 1);
  if (val < 0.1) discard;*/
  outColour = vec4(1, 0, 0, 1);
}

