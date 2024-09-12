#version 420 core

layout (location = 0) in vec4 col;

out vec4 color;

void main()
{
    color = col;
} 
