/*
 *  For drawing things to the screen.
 */

#ifndef _ecs_renderer_h
#define _ecs_renderer_h

#include "iro/common.h"

struct Window;

/* ============================================================================
 */
struct Renderer
{
    b8   init(Window* window);
    void deinit();

    b8 update(Window* window);

    b8 compileShaders();

    u32 vertex_shader;
    u32 frag_shader;
    u32 shader_program;
    u32 vao;
    u32 vbo;
};

#endif
