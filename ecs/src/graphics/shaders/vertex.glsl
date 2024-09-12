#version 420 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec4 in_col;

layout (location = 0) out vec4 out_col;

uniform vec2  scale;
uniform vec2  translation;
uniform float rotation;

void main()
{
  out_col = in_col;

  vec2 pos = scale * in_pos + translation;

  pos.x = pos.x * cos(rotation) - pos.y * sin(rotation);
  pos.y = pos.y * sin(rotation) + pos.y * cos(rotation);

  gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
