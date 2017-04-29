#version 450
#extension GL_ARB_separate_shader_objects : enable

//layout(location = 0) in vec3 inPosition;
//layout(location = 1) in float inRadius;
//layout(location = 2) in vec3 inColour;

layout(location = 0) out vec3 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  vec4 colour;
  float pointScale;
} u;

out gl_PerVertex {
  vec4 gl_Position;
  float gl_PointSize;
  //float gl_ClipDistance[];
};

struct Atom {
  vec3 pos;
  vec3 colour;
  float radius;
};

layout(std430, binding=1) buffer Atoms {
  Atom atoms[];
} a;

// 1   4 5
// 0 2   3
const vec2 verts[] = {
  {-0.5, -0.5}, {-0.5,  0.5}, { 0.5, -0.5},
  { 0.5, -0.5}, {-0.5,  0.5}, { 0.5,  0.5},
};

void main() {
  Atom atom = a.atoms[gl_VertexIndex / 6];
  vec2 vpos = verts[gl_VertexIndex % 6];
  //gl_Position = vec4(vpos, 0.5, 1.0);
  gl_Position = u.modelToPerspective * vec4(atom.pos.xy, atom.pos.z, 1.0);
  gl_Position.xy += vpos;
  //gl_PointSize = u.pointScale * inRadius / gl_Position.w;
  //outColour = inColour;
  outColour = vec3(1, 0, 0);
}

