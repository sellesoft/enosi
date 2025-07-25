// modified version of Star Nest by Pablo Roman Andrioli
// https://www.shadertoy.com/view/XlfGRj
// License: MIT
#version 450

#define iterations 17
#define formuparam 0.53

#define volsteps 10
#define stepsize 0.05

#define zoom   0.800
#define tile   0.850
#define speed  0.00005

#define brightness 0.0015
#define distfading 0.530
#define saturation 0.050

//NOTE: every field is padded to 16 bytes due to std140
layout(std140, set = 0, binding = 0) uniform SceneBuffer
{
  mat4 proj;
  mat4 view;
  vec4 resolution_and_time;
}scene;

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

void main()
{
  // get coords and direction 
  vec2 uv = gl_FragCoord.xy / scene.resolution_and_time.xy - 0.5;
  uv.y *= scene.resolution_and_time.y / scene.resolution_and_time.x;
  vec3 dir = vec3(uv * zoom, 1.0);
  float time = scene.resolution_and_time.z * speed + 0.25;
  vec3 from = vec3(1.0 + 1.2f*time, 0.5 + 0.2f*time, -1.5);

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
