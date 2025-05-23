#version 450

//NOTE: every field is padded to 16 bytes due to std140
layout(std140, set = 0, binding = 0) uniform SceneBuffer
{
  mat4 proj;
  mat4 view;
  vec4 resolution_and_time;
}scene;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main()
{
  vec4 pos = scene.proj * scene.view * vec4(in_pos, 0.0, 1.0);
  gl_Position = vec4(pos.xy, 0.0, 1.0);
  out_uv = in_uv;
  out_color = in_color;
}
