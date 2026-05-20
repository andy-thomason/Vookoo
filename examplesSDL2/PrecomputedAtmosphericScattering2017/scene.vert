#version 450
layout(set=0, binding=4, std140) uniform SceneUBO {
    mat4 model_from_view;
    mat4 view_from_clip;
    vec4 camera_exposure;
    vec4 white_point;
    vec4 earth_center;
    vec4 sun_direction;
    vec2 sun_size;
} u;
layout(location=0) out vec3 outRay;
void main() {
    vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 vertex = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    outRay      = (u.model_from_view * vec4((u.view_from_clip * vertex).xyz, 0.0)).xyz;
    gl_Position = vertex;
}
