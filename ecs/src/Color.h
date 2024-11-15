/*
 *  Color type.
 */


#ifndef _ecs_color_h
#define _ecs_color_h

#include "iro/common.h"
#include "iro/platform.h"
#include "iro/io/io.h"

struct Color;

static Color operator*(const Color& lhs, f32 rhs);
static Color operator*(f32 lhs, const Color& rhs);

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
    this->rgba = iro::platform::byteSwap(rgba);
  }

  void operator*= (f32 rhs)
  {
    *this = *this * rhs;
  }
};

// TODO(sushi) this is probably horrible 
static Color operator*(const Color& lhs, f32 rhs)
{
  return 
  {
    u8(max(0.f, min(255.f, (f32(lhs.r) * rhs)))),
    u8(max(0.f, min(255.f, (f32(lhs.g) * rhs)))),
    u8(max(0.f, min(255.f, (f32(lhs.b) * rhs)))),
    lhs.a,
  };
}

// TODO(sushi) this is probably horrible 
static Color operator*(f32 lhs, const Color& rhs)
{
  return 
  {
    u8(max(0.f, min(255.f, (f32(rhs.r) * lhs)))),
    u8(max(0.f, min(255.f, (f32(rhs.g) * lhs)))),
    u8(max(0.f, min(255.f, (f32(rhs.b) * lhs)))),
    rhs.a,
  };
}

namespace iro::io
{
static s64 format(IO* io, const Color& c)
{
  return formatv(io, "(",c.r,",",c.g,",",c.b,",",c.a,")");
}
}

#endif
