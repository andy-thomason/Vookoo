#version 460 core
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified to be compatible with vulkan

layout(location = 0) in flat vec3 gFacetNormal; // flat required, otherwise coloring problems similar looking to z-fighting
layout(location = 1) in vec3 gTriDistance;
layout(location = 2) in vec4 gPatchDistance;
layout(location = 3) in flat int gPrimitiveID;

layout(location = 0) out vec4 FragColor;

vec3 LightPosition = vec3(0.25f, 0.25f, 1.f);
vec3 DiffuseMaterial = vec3(0.f, 0.75f, 0.75f);
vec3 AmbientMaterial = vec3(0.04f, 0.04f, 0.04f);
vec3 SpecularMaterial = vec3(0.5f, 0.5f, 0.5f);
float Shininess = 50.f;

const vec3 InnerLineColor = vec3(1, 1, 1);
const bool DrawLines = true;

layout (binding = 0) uniform Uniform0 {
  mat4 Projection;
  mat4 View;
  mat4 GeometryTransform;
  float TessLevelInner;
  float TessLevelOuter;
  float PatchID;
  float padding;
} u0;

float amplify(float d, float scale, float offset)
{
    d = scale * d + offset;
    d = clamp(d, 0, 1);
    d = 1 - exp2(-2*d*d);
    return d;
}

void main()
{
    vec3 N = normalize(gFacetNormal);
    vec3 L = LightPosition;
    vec3 E = vec3(0, 0, 1);
    vec3 H = normalize(L + E);

    float df = abs(dot(N, L));
    float sf = abs(dot(N, H));
    sf = pow(sf, Shininess);
    vec3 color = AmbientMaterial + df * DiffuseMaterial + sf * SpecularMaterial;
    
    if (gPrimitiveID==int(u0.PatchID)) {
        color = vec3(1.f,0.f,0.f);
    }
    
    if (DrawLines) {
        float d1 = min(min(gTriDistance.x, gTriDistance.y), gTriDistance.z);
        float d2 = min(min(min(gPatchDistance.x, gPatchDistance.y), gPatchDistance.z), gPatchDistance.w);
        d1 = 1 - amplify(d1, 50, -0.5);
        d2 = amplify(d2, 50, -0.5);
        color = d2 * color + d1 * d2 * InnerLineColor;
    }

    FragColor = vec4(color, 1.0f);
}
