#version 420 core

in vec2 in_pos;
in vec2 in_uv;
in vec4 in_col;

layout (location = 0) out vec4 out_col;
layout (location = 1) out vec2 out_uv;

uniform bool has_texture;

uniform mat3 view;
uniform mat3 proj;
uniform mat3 model;

void main()
{
  out_col = in_col;

  if (has_texture)
    out_uv = in_uv;
  else
    out_uv = vec2(-1.f, -1.f);

  vec3 pos = proj * view * model * vec3(in_pos, 1.f);

  gl_Position = vec4(pos.xy, 0.0, 1.0);
}
