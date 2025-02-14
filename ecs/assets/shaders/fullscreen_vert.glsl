#version 420 core

out vec2 out_uv;

void main()
{
  vec2 vertices[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));
  gl_Position = vec4(vertices[gl_VertexID], 0, 1);
  out_uv = 0.5 * gl_Position.xy + vec2(0.5);
}