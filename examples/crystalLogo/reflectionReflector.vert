#version 460
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
 mat4 u_projection;
 mat4 u_view;
 mat4 u_world;
} u;

#define u_projection u.u_projection
#define u_view u.u_view
#define u_world u.u_world

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_z;

mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
  // from glm/gtc/matrix_transform.inl line#755
  // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluLookAt.xml
  // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glTranslate.xml
  vec3 f = normalize(center - eye);
  vec3 s = normalize(cross(f, up));
  vec3 u = cross(s, f);

  return transpose(mat4(
    vec4( s,-dot(s, eye) ),
    vec4( u,-dot(u, eye) ),
    vec4(-f, dot(f, eye) ),
    vec4( 0,0,0,1 )
  ));
}

// Camera view of each cube face
// gl_ViewIndex is 0,1...N-1 for N=6 sides of cube map
mat4 Q[] = mat4[] (
  //      eye,            center,  up
  lookAt( vec3(-2, 0, 0), vec3(0), vec3(0,-1, 0) ),
  lookAt( vec3( 2, 0, 0), vec3(0), vec3(0,-1, 0) ),
  lookAt( vec3( 0,-2, 0), vec3(0), vec3(0, 0,+1) ),
  lookAt( vec3( 0, 2, 0), vec3(0), vec3(0, 0,-1) ),
  lookAt( vec3( 0, 0,-2), vec3(0), vec3(0,-1, 0) ),
  lookAt( vec3( 0, 0, 2), vec3(0), vec3(0,-1, 0) )
);

void main() {
  v_uv = a_uv;
  //v_z = 1.0 - (mat3(u_view) * mat3(u_world) * a_position).z;
  //gl_Position = u_projection * u_view * u_world * vec4(a_position, 1);

  mat4 viewworld = Q[gl_ViewIndex] * inverse(u_world);
  vec4 viewworldpos = viewworld * vec4(a_position, 1);
  v_z = 1.0 - viewworldpos.z;
  gl_Position = u_projection * viewworldpos;
}
