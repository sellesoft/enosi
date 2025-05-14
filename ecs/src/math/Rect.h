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
  f32 x, y, w, h;

  static Rect from(f32 x, f32 y, f32 w, f32 h)
  {
    return { x, y, w, h };
  }

  static Rect from(vec2f pos, vec2f size)
  {
    return { pos.x, pos.y, size.x, size.y };
  }

  void set(vec2f pos, vec2f size) { setPos(pos); setSize(size); }
  void setPos(vec2f pos) { x = pos.x; y = pos.y; }
  void setSize(vec2f size) { w = size.x; h = size.y; }

  vec2f pos() const { return {x,y}; }
  vec2f size() const { return {w,h}; }

  void floorPos() { x = floor(x); y = floor(y); }

  void posAdd(vec2f v) { x += v.x; y += v.y; }

  Rect contractedX(f32 v) const 
  {
    return {x + v, y, w - 2.f * v, h};
  }
  void contractX(f32 v) { *this = contractedX(v); }

};

#endif
