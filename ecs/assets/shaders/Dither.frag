/*
 *  Dither shader that dithers whatever texture is tossed into it.
 *  This is a "port" of a shader I (sushi) wrote YEARS ago and so its probably
 *  NOT very performant. I just want to see what the stars bg we have rn 
 *  looks like dithered.
 */

#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

// TODO(sushi) this should be a push constant but I don't think our current
//             api allows setting up push constants other than whatever 
//             we have hardcoded rn.
layout(std140, set = 0, binding = 0) uniform SceneBuffer
{
  mat4 proj;
  mat4 view;
  vec4 resolution_and_time;
}scene;

float quant(float v, float res)
{
  return floor(v * res) / res;
}

float rand(vec2 v)
{
  return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec4 dither(sampler2D tex, vec2 uv)
{
  float time = 1.f;

  vec2 tex_size = textureSize(tex, 0);
  vec2 tex_inc = vec2(1 / tex_size.x, 1 / tex_size.y);

  vec2 tc = uv;

  if (tc.x == 0 || tc.y == 0 || tc.x == 1 || tc.y == 1)
    return texture(tex, tc);

  float seed_float = rand(vec2(time, time));

  tc = vec2(quant(tc.x, tex_size.x / 4.f), quant(tc.y, tex_size.y / 4.f));

  vec2 rand_control = 
    vec2(tc.x + quant(time, 5), tc.y + quant(time, 5));

  float random = rand(rand_control);

  vec2 next = tc;
  int len = 4;

  if (random <= 0.25)
    next.x += tex_inc.x * int(rand(tc) * len);
  else if (random > 0.25 && random <= 0.5)
    next.y += tex_inc.y * int(rand(tc) * len);
  else if (random > 0.5 && random <= 0.75)
    next.x -= tex_inc.x * int(rand(tc) * len);
  else
    next.y -= tex_inc.y * int(rand(tc) * len);

  vec4 curr_col = texture(tex, tc);
  vec4 next_col = texture(tex, next);

  if (next_col != curr_col)
    return next_col;
  else
    return curr_col;

}

void main()
{
  out_color = dither(tex, in_uv);
}
