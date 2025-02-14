// modified version of Star Nest by Pablo Roman Andrioli
// https://www.shadertoy.com/view/XlfGRj
// License: MIT
#version 420 core

#define iterations 17
#define formuparam 0.53

#define volsteps 10
#define stepsize 0.1

#define zoom   0.800
#define tile   0.850
#define speed  0.001

#define brightness 0.0015
#define distfading 0.730
#define saturation 0.150

in vec2 in_uv;

out vec4 out_color;

uniform float u_time;
uniform vec2 u_resolution;

void main()
{
  // get coords and direction
  vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
  uv.y *= u_resolution.y / u_resolution.x;
  vec3 dir = vec3(uv * zoom, 1.0);
  float time = u_time * speed + 0.25;
  vec3 from = vec3(1.0 + 2.0*time, 0.5 + time, -1.5);

  // volumetric rendering
  float s = 0.1;
  float fade = 1.0;
  vec3 v = vec3(0.0);
  for (int r = 0; r < volsteps; r++)
  {
    vec3 p = from + 0.5 * s * dir;
    p = abs(vec3(tile) - mod(p, vec3(2.0 * tile))); // tiling fold

    float a = 0.0;
    float pa = 0.0;
    for (int i = 0; i < iterations; i++)
    {
      p = abs(p) / dot(p, p) - formuparam; // the magic formula
      a += abs(length(p) - pa); // absolute sum of average change
      pa = length(p);
    }

    // add contrast
    a *= a*a;

    // coloring based on distance
    v += fade;
    v += vec3(s, s*s, s*s*s*s) * a * brightness * fade;

    // distance fading
    fade *= distfading;

    s += stepsize;
  }

  // color adjust
  v = mix(vec3(length(v)), v, saturation);

  out_color = vec4(0.01 * v, 1.0);
}