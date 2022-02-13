#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 0) out vec3 fragColour;

layout (push_constant) uniform Uniform {
  vec4 colour;
  mat4 rotation;
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  // Rotate and copy 2D position to 3D + depth
  gl_Position = u.rotation * vec4(inPosition, 0.0, 1.0);

  // Copy colour to the fragment shader.
  fragColour = inColour;
}
