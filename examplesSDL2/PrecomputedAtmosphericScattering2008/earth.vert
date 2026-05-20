#version 450
// Fullscreen triangle with ray direction computation for atmosphere display.

layout(set=0, binding=0) uniform DrawUBO {
    vec3  c;          float pad0;
    vec3  s;          float exposure;
    mat4  projInverse;
    mat4  viewInverse;
} u;

layout(location=0) out vec2 outCoords;
layout(location=1) out vec3 outRay;

void main() {
    vec2 uv   = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 pos  = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    outCoords = uv;
    outRay    = (u.viewInverse * vec4((u.projInverse * pos).xyz, 0.0)).xyz;
    gl_Position = pos;
}
