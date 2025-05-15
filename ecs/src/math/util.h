/*
 *  Miscellaneous utility functions.
 *  These should probably be moved around as more appropriate places 
 *  show up.
 */

#ifndef _ecs_math_util_h
#define _ecs_math_util_h

#include "math/vec.h"
#include "math/Rect.h"

namespace math
{

const f32 pi = 3.14159265359f;

/* ----------------------------------------------------------------------------
 */
inline f32 degreesToRadians(f32 deg)
{
  return deg * pi / 180.f;
}

/* ----------------------------------------------------------------------------
 */
template<typename T>
inline b8 pointInRect(vec2<T> p, vec2<T> rect_pos, vec2<T> rect_dim)
{
  return 
    p.x >= rect_pos.x && 
    p.y >= rect_pos.y &&
    p.x <= rect_pos.x + rect_dim.x && 
    p.y <= rect_pos.y + rect_dim.y;
}

/* ----------------------------------------------------------------------------
 */
inline b8 pointInRect(vec2f p, Rect rect)
{
  return pointInRect(p, rect.pos(), rect.size());
}

/* ----------------------------------------------------------------------------
 *  Using 
 *    https://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2
 *  TODO(sushi) try the lookup table method they propose.
 */
inline u16 roundUpToPower2(u16 x)
{
  x -= 1;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  return x + 1;
}

inline u32 roundUpToPower2(u32 x)
{
  x -= 1;
  x |= x >>  1;
  x |= x >>  2;
  x |= x >>  4;
  x |= x >>  8;
  x |= x >> 16;
  return x + 1;
}

inline u64 roundUpToPower2(u64 x)
{
  x -= 1;
  x |= x >>  1;
  x |= x >>  2;
  x |= x >>  4;
  x |= x >>  8;
  x |= x >> 16;
  x |= x >> 32;
  return x + 1;
}

}

#endif
