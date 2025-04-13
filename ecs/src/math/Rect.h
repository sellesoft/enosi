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
  s32 x, y, w, h;

  void set(vec2i pos, vec2i size)
  {
    x = pos.x; y = pos.y;
    w = size.x; h = size.y;
  }

  vec2i pos() const { return {x,y}; }
  vec2i size() const { return {w,h}; }
};

#endif
