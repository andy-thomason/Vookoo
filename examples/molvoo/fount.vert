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
  vec2 pos0;
  vec2 pos1;
  vec3 colour;
  int pad2;
  vec3 origin;
  int pad;
};

layout(std430, binding=1) buffer Glyphs {
  Glyph glyphs[];
} g;

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec2 outUV;

const bvec2 bverts[] = {
  {false, false}, {false, true}, {true, false},
  {true, false}, {false, true}, {true, true},
};

void main() {
  Glyph glyph = g.glyphs[gl_VertexIndex / 6];
  bvec2 vsel = bverts[gl_VertexIndex % 6];
  vec2 pos = vec2(vsel.x ? glyph.pos1.x : glyph.pos0.x, vsel.y ? glyph.pos1.y : glyph.pos0.y);
  //vec2 pos = vsel ? glyph.pos1 : glyph.pos0;
  outUV = vec2(vsel.x ? glyph.uv1.x : glyph.uv0.x, vsel.y ? glyph.uv1.y : glyph.uv0.y);
  //outUV = vsel ? glyph.uv1 : glyph.uv0;
  vec3 worldOrigin = vec3(u.modelToWorld * vec4(glyph.origin, 1));
  vec3 worldPos = worldOrigin + u.cameraToWorld[0].xyz * pos.x + u.cameraToWorld[1].xyz * pos.y;
  gl_Position = u.worldToPerspective * vec4(worldPos, 1.0);

  outColour = glyph.colour;
}

