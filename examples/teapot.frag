#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inCameraDir;
layout(location = 3) in vec4 inLightSpacePos;

layout(location = 0) out vec4 outColour;

layout (binding = 0) uniform Uniform {
  mat4 modelToPerspective;
  mat4 modelToWorld;
  mat4 normalToWorld;
  mat4 modelToLight;
  vec4 cameraPos;
} u;

layout (binding = 1) uniform samplerCube cubeMap;
layout (binding = 2) uniform sampler2D shadowMap;

void main() {
  vec3 cameraDir = normalize(inCameraDir);
  vec3 normal = normalize(inNormal);

  vec3 reflectDir = normalize(reflect(cameraDir, normal));
  vec4 specular = texture(cubeMap, reflectDir);

  vec4 shadow = textureProj(shadowMap, inLightSpacePos);
  outColour = vec4(inLightSpacePos.z / inLightSpacePos.w < shadow.x + 0.01 ? 1 : 0, 0, 0, 1);

  //vec3 ambient = shadow.x > 0.99 ? vec3(0.2, 0.2, 0.3) : vec3(0.0, 0.0, 0.0);
  //if (inLightSpacePos.z > 10) ambient.x = 1;
  //if (inLightSpacePos.w > 10) ambient.y = 1;
  //if (inLightSpacePos.z / inLightSpacePos.w > shadow.x + 0.01) ambient.y = 1;
  //ambient.x = inLightSpacePos.w  * 0.1;
  //outColour = vec4(ambient + specular.xyz * 0.3, 1);
}

