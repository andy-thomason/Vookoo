#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 normal_in;
layout (location = 1) in vec3 viewPos_in;
layout (location = 2) in vec3 viewLightPos_in;

layout (location = 0) out vec4 fragColor_out;

void main() 
{
  vec3 ambient = vec3(0.2, 0.2, 0.2);
  vec3 diffuse = vec3(1.0, 0.5, 0.5);
  vec3 specular = vec3(0.5, 0.5, 0.5);

  vec3 lightDir = normalize(viewLightPos_in - viewPos_in);
  vec3 eyeDir = normalize(-viewPos_in);
  vec3 normal = normalize(normal_in);
  vec3 reflection = reflect(eyeDir, lightDir);
  float diffuseFactor = max(dot(normal, lightDir), 0.0);
  float specularFactor = pow(max(dot(reflection, eyeDir), 0.0), 10);
  fragColor_out = vec4(
    ambient + diffuse * diffuseFactor + specular * specularFactor, 1.0
  );
}
