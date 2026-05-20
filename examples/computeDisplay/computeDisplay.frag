#version 460

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout (push_constant) uniform Uniform {
  float time;
  int Nx, Ny;
};

layout(std430, set = 0, binding=0) readonly buffer Buffer {
  float values[];
};

void main() {
  int id = int(uv.x*(Nx-1))+int(uv.y*(Ny-1))*Nx;
  float S = (values[id]+1.)/2.;
  outColor = vec4(vec3(S),1.);
}

