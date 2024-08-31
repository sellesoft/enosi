/*
 *  Miscellaneous utility functions.
 *  These should probably be moved around as more appropriate places 
 *  show up.
 */

#ifndef _ecs_math_util_h
#define _ecs_math_util_h

#include "vec.h"

namespace math
{

/* ----------------------------------------------------------------------------
 */
inline static b8 pointInRect(vec2i p, vec2i rect_pos, vec2i rect_dim)
{
  return 
    p.x >= rect_pos.x && 
    p.y >= rect_pos.y &&
    p.x <= rect_pos.x + rect_dim.x && 
    p.y <= rect_pos.y + rect_dim.y;
}


}

#endif
