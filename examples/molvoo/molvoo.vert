#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec3 outCentre;
layout(location = 2) out float outRadius;
layout(location = 3) out vec3 outRayDir;
layout(location = 4) out vec3 outRayStart;

layout (binding = 0) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 cameraToWorld;
  vec4 colour;
  vec2 pointScale;
} u;

out gl_PerVertex {
  vec4 gl_Position;
  float gl_PointSize;
};

struct Atom {
  vec3 pos;
  float radius;
  vec3 colour;
};

layout(std430, binding=1) buffer Atoms {
  Atom atoms[];
} a;

// 1   4 5
// 0 2   3
const vec2 verts[] = {
  {-1.0, -1.0}, {-1.0,  1.0}, { 1.0, -1.0},
  { 1.0, -1.0}, {-1.0,  1.0}, { 1.0,  1.0},
};

void main() {
  Atom atom = a.atoms[gl_VertexIndex / 6];
  vec2 vpos = verts[gl_VertexIndex % 6] * (atom.radius * 1.1);
  vec3 pos = atom.pos;
  pos.x += (gl_InstanceIndex % 20 - 10) * 100.0;
  pos.y += (gl_InstanceIndex / 20 % 20 - 10) * 100.0;
  pos.z += (gl_InstanceIndex / 400 % 20 - 10) * 100.0;
  vec3 worldCentre = vec3(u.modelToWorld * vec4(pos, 1));
  vec3 worldPos = worldCentre + u.cameraToWorld[0].xyz * vpos.x + u.cameraToWorld[1].xyz * vpos.y;
  vec3 cameraPos = u.cameraToWorld[3].xyz;
  gl_Position = u.worldToPerspective * vec4(worldPos, 1.0);
  outColour = atom.colour;
  outCentre = worldCentre - cameraPos;
  outRadius = atom.radius;
  outRayDir = normalize(worldPos - cameraPos);
  outRayStart = cameraPos;
}
