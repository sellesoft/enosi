/*
 *  For drawing things to the screen.
 */

#ifndef _ecs_renderer_h
#define _ecs_renderer_h

#include "iro/common.h"
#include "iro/containers/linked_pool.h"

struct Window;

namespace gfx
{

/* ============================================================================
 */
struct Buffer
{
  u32 vbo = 0;
  u32 ibo = 0;
  u32 vao = 0;

  u64 vertex_size = 0;
  u64 index_size = 0;

  u64 num_vertexes = 0;
  u64 num_indexes = 0;

  struct
  {
    void* vp = nullptr;
    void* ip = nullptr;
  } mapped;

  b8   init(u64 vsize, u64 isize);
  void deinit();

  void setF32AttribF32(
      u32 idx, 
      u32 len, 
      b8 normalized, 
      u64 stride, 
      u64 offset);
  void setF32AttribU8(
      u32 idx, 
      u32 len, 
      b8 normalized, 
      u64 stride,
      u64 offset);

  void map();
  void unmap();
};

/* ============================================================================
 */
struct Renderer
{
  u64 frame;

  u32 vertex_shader;
  u32 frag_shader;
  u32 shader_program;

  iro::SLinkedPool<Buffer> buffers;

  b8   init(Window* window);
  void deinit();

  b8 update(
    Window* window,
    f64 time);

  b8 compileShaders();

  Buffer* createBuffer();
  void    destroyBuffer(Buffer* buffer);
};

}

#endif
