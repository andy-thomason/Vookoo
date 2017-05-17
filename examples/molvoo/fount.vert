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

struct Glyph {
  vec2 uv0;
  vec2 uv1;
  vec3 pos;
  int pad;
  vec3 colour;
  int pad2;
  vec2 size;
  int pad3[2];
};

layout(std430, binding=1) buffer Glyphs {
  Glyph glyphs[];
} g;

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec2 outUV;

// 1   4 5
// 0 2   3
const vec2 verts[] = {
  {-1.0, -1.0}, {-1.0,  1.0}, { 1.0, -1.0},
  { 1.0, -1.0}, {-1.0,  1.0}, { 1.0,  1.0},
};

void main() {
  Glyph glyph = g.glyphs[gl_VertexIndex / 6];
  vec2 vpos = verts[gl_VertexIndex % 6] * glyph.size;
  vec3 pos = glyph.pos;
  vec3 worldCentre = vec3(u.modelToWorld * vec4(pos, 1));
  vec3 worldPos = worldCentre + u.cameraToWorld[0].xyz * vpos.x + u.cameraToWorld[1].xyz * vpos.y;
  vec3 cameraPos = u.cameraToWorld[3].xyz;
  gl_Position = u.worldToPerspective * vec4(worldPos, 1.0);

  outColour = glyph.colour;

  outUV.x = vpos.x < 0 ? glyph.uv0.x : glyph.uv1.x;
  outUV.y = vpos.y < 0 ? glyph.uv0.y : glyph.uv1.y;
}

