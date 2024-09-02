#version 330 core

layout (location = 0) in vec2 in_pos;

uniform vec2  scale;
uniform vec2  translation;
uniform float rotation;

void main()
{
  vec2 pos = scale * in_pos + translation;

  pos.x = pos.x * cos(rotation) - pos.y * sin(rotation);
  pos.y = pos.y * sin(rotation) + pos.y * cos(rotation);

  gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
