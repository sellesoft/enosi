/*
 *  A rectangle represented by a start and end point.
 */
#ifndef _ecs_rect_h
#define _ecs_rect_h

#include "math/vec.h"
#include "iro/io/IO.h"

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
  void setPos(f32 x, f32 y) { this->x = x; this->y = y; }
  void setSize(vec2f size) { w = size.x; h = size.y; }
  void setSize(f32 x, f32 y) { w = x; y = h; }

  Rect& mulWidth(f32 v) { w *= v; return *this; }
  Rect& mulHeight(f32 v) { h *= v; return *this; }
  Rect& mulSize(f32 v) { mulWidth(v); mulHeight(v); return *this; }
  Rect& mulSize(f32 x, f32 y) { mulWidth(x); mulHeight(y); return *this; }

  vec2f pos() const { return {x,y}; }
  vec2f size() const { return {w,h}; }

  void floorPos() { x = floor(x); y = floor(y); }

  void addPos(vec2f v) { x += v.x; y += v.y; }

  Rect contractedX(f32 v) const 
  {
    return {x + v, y, w - 2.f * v, h};
  }
  void contractX(f32 v) { *this = contractedX(v); }

  b8 containsPoint(vec2f p) const
  {
    return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
  }
};

namespace iro::io
{

static s64 format(IO* io, const Rect& rect)
{
  return io::formatv(io, '(', rect.pos(), ',', rect.size(), ')');
}

}

#endif
