#version 460

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 1) uniform Uniform {
 vec2 u_resolution;
 float u_tick;
} u;

layout (push_constant) uniform namedPushConstant {
  int maskId;
  int typeId;
} p;


#define u_resolution u.u_resolution
#define u_maskId p.maskId
#define u_typeId p.typeId
#define u_tick u.u_tick

layout (binding = 2) uniform sampler2D u_texture;
layout (binding = 3) uniform sampler2D u_displacement;
layout (binding = 4) uniform sampler2D u_mask;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 FragColor;

const float PI = 3.141592653589793;
const float PI2 = 6.28318530718;

mat2 scale(vec2 value) {
  return mat2(value.x, 0.0, 0.0, value.y);
}

mat2 rotate2d(float value){
  return mat2(cos(value), -sin(value), sin(value), cos(value));
}

vec3 gradient1(vec2 st, float tick) {
  vec3 c1 = vec3(0.98, 0.71, 0.0);
  vec3 c2 = vec3(0.95, 0.20, 0.14);
  vec3 c3 = vec3(0.89, 0.12, 0.78);
  vec3 c4 = vec3(0.30, 0.24, 0.96);

  st.y = 1.0 - st.y;

  vec2 toCenter = vec2(0.55, 0.58) - st;
  float angle = atan(toCenter.y, toCenter.x) / PI;

  vec3 colorA = mix(c1, c2, smoothstep(0.0, 0.5, angle));

  st -= vec2(0.5);
  st *= scale(vec2(1.4));
  st *= rotate2d(-0.44);
  st += vec2(0.5);

  vec3 colorB = mix(c2, c3, smoothstep(0.3, 0.8, st.x));
  colorB = mix(colorB, c4, smoothstep(0.55, 1.0, st.x));

  return mix(colorA, colorB, smoothstep(0.28, 0.65, st.x));
}

vec3 gradient2(vec2 st, float tick) {
  vec3 c1 = vec3(1.0, 0.8, 0.2);
  vec3 c2 = vec3(0.92, 0.20, 0.14);

  st -= vec2(0.5);
  st *= scale(vec2(3.8));
  st *= rotate2d(tick * PI);
  st += vec2(0.5);

  return mix(c1, c2, st.x);
}

vec3 gradient3(vec2 st, float tick) {
  vec3 c1 = vec3(0.89, 0.12, 0.78);
  vec3 c2 = vec3(0.29, 0.68, 0.95);

  st -= vec2(0.5);
  st *= scale(vec2(3.8));
  st *= rotate2d(tick * PI);
  st += vec2(0.5);

  return mix(c1, c2, st.x);
}

vec3 gradients(int type, vec2 st, float tick) {
  if (type == 1) {
    return gradient1(st, tick);
  } else if (type == 2) {
    return gradient2(st, tick);
  } else if (type == 3) {
    return gradient3(st, tick);
  }
}

void main() {
  vec2 st = gl_FragCoord.xy / u_resolution;

  vec4 displacement = texture(u_displacement, st);
  
  vec2 direction = vec2(cos(displacement.r * PI2), sin(displacement.r * PI2));
  float length = displacement.g;

  vec2 newUv = v_uv;

  newUv.x += (length * 0.07) * direction.x;
  newUv.y += (length * 0.07) * direction.y;

  vec4 vtexture = texture(u_texture, newUv);
  float tick = u_tick * 0.009;

  vec3 color = gradients(u_typeId, v_uv, tick);

  vtexture.rgb = color + (vtexture.rgb * color);

  vec4 mask = texture(u_mask, st);

  int maskId = int(mask.r * 4.0 + mask.g * 2.0 + mask.b * 1.0);

  if (maskId == u_maskId) {
    FragColor = vec4(vtexture.rgb, vtexture.a * mask.a);
  } else {
    discard;
  }
}
