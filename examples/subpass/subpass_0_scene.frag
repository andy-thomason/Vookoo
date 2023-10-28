#version 450

layout(location = 0) in vec3 v2fCol;
layout(location = 1) in vec2 v2fUV;
layout(location = 2) in vec3 v2fWorldPos;
layout(location = 3) in vec3 v2fWorldNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 albedo = vec3(0.5, 0.5, 0.5);
    vec3 lightPos = vec3(1.0, 1.0, -1.0);
    vec3 lightCol = vec3(1.0, 1.0, 1.0);
    vec3 ambient = 0.1 * lightCol;

    vec3 wNormal = normalize(v2fWorldNormal);
    vec3 lightDir = normalize(lightPos - v2fWorldPos);
    
    float diffuse = max(dot(wNormal, lightDir), 0.0);
    
    vec3 shaded = (ambient + diffuse) * albedo; 
    outColor = vec4(shaded, 1.0);
}
