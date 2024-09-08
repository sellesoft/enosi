/*
 *  Color type.
 */


#ifndef _ecs_color_h
#define _ecs_color_h

#include "iro/common.h"

struct Color
{
  union
  {
    u32 rgba = 0;
    struct { u8 r, g, b, a; };
  };

  Color() {}

  Color(u8 r, u8 g, u8 b, u8 a = 255) 
  {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }

  Color(u32 rgba)
  {
    this->rgba = rgba;
  }

  void operator*= (f32 rhs)
  {
    r = u8(f32(r) * rhs);
    g = u8(f32(g) * rhs);
    b = u8(f32(b) * rhs);
    a = u8(f32(a) * rhs);
  }
};

#endif
