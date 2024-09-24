#version 420 core

layout (location = 0) in vec4 in_col;
layout (location = 1) in vec2 in_uv;

out vec4 color;

uniform sampler2D tex;

void main()
{
    color = in_col * texture(tex, in_uv);
} 
