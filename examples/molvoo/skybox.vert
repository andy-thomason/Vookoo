#version 450


layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;

  vec3 rayStart;
  float timeStep;
  vec3 rayDir;
  uint numAtoms;
  uint numConnections;
  uint pickIndex;
  uint pass;
} u;

const vec3 cpos[8] = {
  { -1, -1, -1 },
  {  1, -1, -1 },
  { -1,  1, -1 },
  {  1,  1, -1 },
  { -1, -1,  1 },
  {  1, -1,  1 },
  { -1,  1,  1 },
  {  1,  1,  1 }
};

/*const vec3 cnormal[6] = {
  { -1,  0,  0 },
  {  1,  0,  0 },
  {  0, -1,  0 },
  {  0,  1,  0 },
  {  0,  0, -1 },
  {  0,  0,  1 },
};*/

//   6     7
// 2 4  3  5
// 0    1 
const uint cube[] = {
  4, 6, 0, 0, 6, 2,
  1, 3, 5, 5, 3, 7,
  1, 5, 0, 0, 5, 4,
  2, 6, 3, 3, 6, 7,
  0, 2, 1, 1, 2, 3,
  5, 7, 4, 4, 7, 6,
};

layout(location = 0) out vec3 outPos;

void main() {
  outPos = cpos[cube[gl_VertexIndex]];
  gl_Position = u.worldToPerspective * vec4(u.cameraToWorld[3].xyz + outPos * 30, 1);

  // Z depth is w / w = 1
  gl_Position.z = gl_Position.w;
}
