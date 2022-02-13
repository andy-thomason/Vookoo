#version 460

layout (set = 0, binding = 1) uniform sampler2D tExplosion;

layout(location = 0) in float noise;

layout(location = 0) out vec4 outColour;

float random( vec3 scale, float seed ){
  return fract( sin( dot( gl_FragCoord.xyz + seed, scale ) ) * 43758.5453 + seed ) ;
}

void main() {

  // get a random offset
  float r = .01 * random( vec3( 12.9898, 78.233, 151.7182 ), 0.0 );
  // lookup vertically in the texture, using noise and offset
  // to get the right RGB colour
  vec2 tPos = vec2( 0, 1.4 * noise + r );
  vec4 color = texture( tExplosion, tPos );

  outColour = vec4( color.rgb, 1.0 );

}
