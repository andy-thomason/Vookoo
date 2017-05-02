#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 inCentre;
layout(location = 2) in float inRadius;
layout(location = 3) in vec3 inRayDir;
layout(location = 4) in vec3 inRayStart;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 cameraToWorld;
  vec4 colour;
  vec2 pointScale;
  float timeStep;
  uint numAtoms;
} u;

// Ray-sphere (c, r) in ray space (d)
// Intersection at td.
// (c-td)^2 = r^2
// c^2 - 2tc.d + t^2 = r^2 (as d^2 = 1)
// t^2 - t(2c.d) + (c^2-r^2) = 0
void main() {
  vec3 rayDir = normalize(inRayDir);
  const float a = 1.0;
  float b = -2.0 * dot(inCentre, rayDir);
  float c = dot(inCentre, inCentre) - inRadius * inRadius;
  float q = b * b - (4.0 * a) * c;
  if (q < 0) discard;

  float t = (-b - sqrt(q)) * 0.5;
  if (t < 0) discard;

  vec3 intersection = rayDir * t;
  vec3 normal = normalize(intersection - inCentre);
  vec3 lightDir = normalize(vec3(1, 1, 1));
  vec3 ambient = inColour * 0.3;
  float dfactor = max(0.0, dot(lightDir, normal));
  outColour = vec4(ambient + inColour * dfactor, 1);

  vec4 persp = u.worldToPerspective * vec4(inRayStart + intersection, 1);

  gl_FragDepth = persp.z / persp.w;
}

