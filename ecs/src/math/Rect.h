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
  vec2f min;
  vec2f max;
};

#endif
