#version 460

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 1) uniform Uniform {
 vec2 u_resolution;
 float u_tick;
 float u_borderWidth;
 float u_displacementLength;
 float u_reflectionOpacity;
 int u_scene;
} u;

layout (push_constant) uniform namedPushConstants {
 int face;
 int typeId;
} p;

layout (binding = 2) uniform sampler2D u_texture;
layout (binding = 3) uniform samplerCube u_reflection;

#define u_resolution u.u_resolution
#define u_face p.face
#define u_typeId p.typeId
#define u_tick u.u_tick
#define u_borderWidth u.u_borderWidth
#define u_displacementLength u.u_displacementLength
#define u_reflectionOpacity u.u_reflectionOpacity 
#define u_scene u.u_scene

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_center;
layout(location = 2) in vec3 v_point;
layout(location = 3) in vec2 v_uv;
layout(location = 4) in vec3 v_color;
layout(location = 5) in float v_depth;

layout(location = 0) out vec4 FragColor;

const float PI2 = 6.283185307179586;

float borders(vec2 uv, float strokeWidth) {
  vec2 borderBottomLeft = smoothstep(vec2(0.0), vec2(strokeWidth), uv);

  vec2 borderTopRight = smoothstep(vec2(0.0), vec2(strokeWidth), 1.0 - uv);

  return 1.0 - borderBottomLeft.x * borderBottomLeft.y * borderTopRight.x * borderTopRight.y;
}

//const float PI2 = 6.28318530718;

vec4 radialRainbow(vec2 st, float tick) {
  vec2 toCenter = vec2(0.5) - st;
  float angle = mod((atan(toCenter.y, toCenter.x) / PI2) + 0.5 + sin(tick * 0.002), 1.0);

  // colors
  vec4 a = vec4(0.15, 0.58, 0.96, 1.0);
  vec4 b = vec4(0.29, 1.00, 0.55, 1.0);
  vec4 c = vec4(1.00, 0.0, 0.85, 1.0);
  vec4 d = vec4(0.92, 0.20, 0.14, 1.0);
  vec4 e = vec4(1.00, 0.96, 0.32, 1.0);

  float step = 1.0 / 10.0;

  vec4 color = a;

  color = mix(color, b, smoothstep(step * 1.0, step * 2.0, angle));
  color = mix(color, a, smoothstep(step * 2.0, step * 3.0, angle));
  color = mix(color, b, smoothstep(step * 3.0, step * 4.0, angle));
  color = mix(color, c, smoothstep(step * 4.0, step * 5.0, angle));
  color = mix(color, d, smoothstep(step * 5.0, step * 6.0, angle));
  color = mix(color, c, smoothstep(step * 6.0, step * 7.0, angle));
  color = mix(color, d, smoothstep(step * 7.0, step * 8.0, angle));
  color = mix(color, e, smoothstep(step * 8.0, step * 9.0, angle));
  color = mix(color, a, smoothstep(step * 9.0, step * 10.0, angle));

  return color;
}

mat2 scale(vec2 value){
  return mat2(value.x, 0.0, 0.0, value.y);
}

mat2 rotate2d(float value){
  return mat2(cos(value), -sin(value), sin(value), cos(value));
}

vec2 rotateUV(vec2 uv, float rotation) {
  float mid = 0.5;
  return vec2(
    cos(rotation) * (uv.x - mid) + sin(rotation) * (uv.y - mid) + mid,
    cos(rotation) * (uv.y - mid) - sin(rotation) * (uv.x - mid) + mid
  );
}

vec4 type1() {
  vec2 toCenter = v_center.xy - v_point.xy;
  float angle = (atan(toCenter.y, toCenter.x) / PI2) + 0.5;
  float displacement = borders(v_uv, u_displacementLength) + borders(v_uv, u_displacementLength * 2.143) * 0.3;

  return vec4(angle, displacement, 0.0, 1.0);
}

vec4 type2() {
  return vec4(v_color, 1.0);
}

vec4 type3() {
  vec2 st = gl_FragCoord.xy / u_resolution;

  vec4 strokeColor = radialRainbow(st, u_tick);
  float depth = clamp(smoothstep(-1.0, 1.0, v_depth), 0.6, 0.9);
  vec4 stroke = strokeColor * vec4(borders(v_uv, u_borderWidth)) * depth;

  vec4 vtexture;

  if (u_face == -1) {
    vec3 normal = normalize(v_normal);
    vtexture = texture(u_reflection, normalize(v_normal));

    vtexture.a *= u_reflectionOpacity * depth;
  }  else {
    vtexture = texture(u_texture, st);
  }

  if (stroke.a > 0.0) {
    return stroke - vtexture.a;
  } else {
    return vtexture;
  }
}

vec4 switchScene(int id) {
  if (id == 1) {
    return type1();
  } else if (id == 2) {
    return type2();
  } else if (id == 3) {
    return type3();
  }
}

void main() {
  if (u_scene == 3) {
    FragColor = switchScene(u_typeId);
  } else {
    FragColor = switchScene(u_scene);
  }
}
