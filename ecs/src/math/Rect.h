/*
 *  A rectangle represented by a start and end point.
 */
#ifndef _ecs_rect_h
#define _ecs_rect_h

#include "math/vec.h"

/* ============================================================================
 */
struct Rect
{
  union{
    struct{ s32 x; s32 y; };
    vec2i pos;
  };
  union{
    struct{ s32 w; s32 h; };
    struct{ s32 width; s32 height; };
    vec2i size;
  };
};

#endif
