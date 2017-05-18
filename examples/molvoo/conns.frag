#version 450

layout(location = 0) in flat vec3 inColour;
layout(location = 1) in flat vec3 inCentre;
layout(location = 2) in flat float inRadius;
layout(location = 3) in vec3 inRayDir;
layout(location = 4) in flat vec3 inRayStart;
layout(location = 5) in flat vec3 inAxis;

layout(location = 0) out vec4 outColour;

layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;
} u;

layout (binding = 4) uniform samplerCube cubeMap;

struct rsResult {
  vec3 pos;
  vec3 normal;
  bool collides;
};

// n = cross(rayDir * t - centre, axis)
// t = solve(n^2 = radius^2, t)
//
// n = t * cross(rayDir, axis) - cross(centre, axis)
// n = t * A - B
// n * n = t^2 * A^2 - t * (2 * A.B) + B^2
rsResult rayCylinderCollide(vec3 rayDir, vec3 centre, vec3 axis, float axislen, float radius) {
  vec3 A = cross(rayDir, axis);
  vec3 B = cross(centre, axis);
  float a = dot(A, A);
  float b = -2.0 * dot(A, B);
  float c = dot(B, B) - radius * radius;
  float q = b * b - (4.0 * a) * c;
  float t = (-b - sqrt(q)) * 0.5 / a;
  rsResult res;
  res.pos = rayDir * t;
  float s = dot(res.pos - centre, axis);
  res.normal = normalize(res.pos - (centre + s * axis));
  res.collides = q >= 0 && t > 0 && s >= 0 && s <= axislen;
  return res;
}

void main() {
  vec3 rayDir = normalize(inRayDir);
  vec3 axis = normalize(inAxis);
  float axislen = length(inAxis);
  rsResult res = rayCylinderCollide(rayDir, inCentre, axis, axislen, inRadius);

  if (!res.collides) discard;

  vec3 intersection  = res.pos;
  vec3 normal = res.normal;
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


