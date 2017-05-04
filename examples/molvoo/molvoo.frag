#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 inCentre;
layout(location = 2) in float inRadius;
layout(location = 3) in vec3 inRayDir;
layout(location = 4) in vec3 inRayStart;

layout(location = 0) out vec4 outColour;

layout (binding = 1) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 cameraToWorld;
  vec4 colour;
} u;

struct rsResult {
  vec3 pos;
  bool collides;
};

rsResult raySphereCollide(vec3 rayDir, vec3 centre, float radius) {
  const float a = 1.0;
  float b = -2.0 * dot(centre, rayDir);
  float c = dot(centre, centre) - radius * radius;
  float q = b * b - (4.0 * a) * c;
  float t = (-b - sqrt(q)) * 0.5;
  rsResult res;
  res.pos = rayDir * t;
  res.collides = q >= 0 && t > 0;
  return res;
}

// Ray-sphere (c, r) in ray space (d)
// Intersection at td.
// (c-td)^2 = r^2
// c^2 - 2tc.d + t^2 = r^2 (as d^2 = 1)
// t^2 - t(2c.d) + (c^2-r^2) = 0
void main() {
  rsResult res = raySphereCollide(normalize(inRayDir), inCentre, inRadius);

  if (!res.collides) discard;

  vec3 intersection  = res.pos;
  vec3 normal = normalize(intersection - inCentre);
  vec3 lightDir = normalize(vec3(1, 1, 1));
  vec3 ambient = inColour * 0.3;
  float dfactor = max(0.0, dot(lightDir, normal));
  outColour = vec4(ambient + inColour * dfactor, 1);

  vec4 persp = u.worldToPerspective * vec4(inRayStart + intersection, 1);

  gl_FragDepth = persp.z / persp.w;
}

