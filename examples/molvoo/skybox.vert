#version 450


layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 cameraToWorld;
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
  outPos = cpos[gl_VertexIndex];
  gl_Position = vec4(outPos, 1);
}
