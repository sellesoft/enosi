#version 450

layout(std140, set = 0, binding = 0) uniform SceneBuffer
{
  mat3 proj;
  mat3 view;
  vec2 resolution;
  float time;
}scene;

layout(push_constant) uniform PushConstant
{
  mat3 transform;
}push;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main()
{
  vec3 pos = scene.proj * scene.view * push.transform * vec3(in_pos, 1.0);
  gl_Position = vec4(pos.xy, 0.0, 1.0);
  out_uv = in_uv;
  out_color = in_color;
}
