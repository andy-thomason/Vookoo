#version 450

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec3 outCentre;
layout(location = 2) out float outRadius;
layout(location = 3) out vec3 outRayDir;
layout(location = 4) out vec3 outRayStart;

layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;
} u;

out gl_PerVertex {
  vec4 gl_Position;
  float gl_PointSize;
};

struct Atom {
  vec3 pos;
  float radius;
  vec3 colour;
  float mass;
  vec3 prevPos;
  int selected;
  vec3 acc;
  int connections[5];
};

struct Instance {
  mat4 modelToWorld;
};

layout(std430, binding=0) buffer Atoms {
  Atom atoms[];
} a;

layout(std430, binding=6) buffer Instances {
  Instance instances[];
} i;

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
  mat4 imat = i.instances[gl_InstanceIndex].modelToWorld;
  mat4 modelToWorld = u.modelToWorld * imat;
  vec3 worldCentre = vec3(modelToWorld * vec4(pos, 1));
  vec3 worldPos = worldCentre + u.cameraToWorld[0].xyz * vpos.x + u.cameraToWorld[1].xyz * vpos.y;
  vec3 cameraPos = u.cameraToWorld[3].xyz;
  gl_Position = u.worldToPerspective * vec4(worldPos, 1.0);
  outColour = atom.selected != 0 ? vec3(1, 1, 1) : atom.colour;
  outCentre = worldCentre - cameraPos;
  outRadius = atom.radius;
  outRayDir = normalize(worldPos - cameraPos);
  outRayStart = cameraPos;
}
