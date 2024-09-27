#version 420 core

in vec2 in_pos;
in vec2 in_uv;
in vec4 in_col;

layout (location = 0) out vec4 out_col;
layout (location = 1) out vec2 out_uv;

uniform vec2  scale;
uniform vec2  translation;
uniform float rotation;
uniform bool  has_texture;

void main()
{
  out_col = in_col;

  if (has_texture)
    out_uv = in_uv;
  else
    out_uv = vec2(-1.f, -1.f );

  vec2 pos = scale * in_pos + translation;

  pos.x = pos.x * cos(rotation) - pos.y * sin(rotation);
  pos.y = pos.y * sin(rotation) + pos.y * cos(rotation);

  gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
