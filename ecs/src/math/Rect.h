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

  static Rect from(vec2i pos, vec2i size)
  {
    return { pos.x, pos.y, size.x, size.y };
  }

  void set(vec2i pos, vec2i size) { setPos(pos); setSize(size); }
  void setPos(vec2i pos) { x = pos.x; y = pos.y; }
  void setSize(vec2i size) { w = size.x; h = size.y; }

  vec2i pos() const { return {x,y}; }
  vec2i size() const { return {w,h}; }
};

#endif
