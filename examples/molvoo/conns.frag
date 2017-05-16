#version 450

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 inCentre;
layout(location = 2) in float inRadius;
layout(location = 3) in vec3 inRayDir;
layout(location = 4) in vec3 inRayStart;

layout(location = 0) out vec4 outColour;

layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;
} u;

layout (binding = 4) uniform samplerCube cubeMap;

struct rsResult {
  vec3 pos;
  bool collides;
};

// Ray-sphere (c, r) in ray space (d)
// Intersection at td.
// (c-td)^2 = r^2
// c^2 - 2tc.d + t^2 = r^2 (as d^2 = 1)
// t^2 - t(2c.d) + (c^2-r^2) = 0
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

void main() {
  vec3 rayDir = normalize(inRayDir);
  rsResult res = raySphereCollide(rayDir, inCentre, inRadius);

  if (!res.collides) discard;

  vec3 intersection  = res.pos;
  vec3 normal = normalize(intersection - inCentre);
  vec3 lightDir = normalize(vec3(1, 1, 1));

  vec4 persp = u.worldToPerspective * vec4(inRayStart + intersection, 1);

  vec3 reflectDir = normalize(reflect(rayDir, normal));
  vec3 reflect = texture(cubeMap, reflectDir).xyz;
  outColour.xyz = reflect;

  vec3 ambient = inColour * 0.1;
  vec3 diffuse = inColour * 0.9;
  vec3 specular = vec3(0.3, 0.3, 0.3);

  float diffuseFactor = max(0.0, dot(normal, lightDir));
  //float shadowDepth = textureProj(shadowMap, inLightSpacePos).x;
  //float expectedDepth = inLightSpacePos.z / inLightSpacePos.w;
  //float shadowFactor = expectedDepth < shadowDepth ? 1.0 : 0.0;
  float shadowFactor = 1.0f;
  outColour = vec4(ambient + diffuse * diffuseFactor * shadowFactor + specular * reflect, 1);

  gl_FragDepth = persp.z / persp.w;
}


