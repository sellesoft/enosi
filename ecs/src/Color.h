/*
 *  Color type.
 */


#ifndef _ecs_Color_h
#define _ecs_Color_h

#include "iro/Common.h"
#include "iro/Platform.h"
#include "iro/io/IO.h"
#include "math/vec.h"

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

  vec4f asVec4Norm() const
  {
    return vec4f(f32(r)/255.f,f32(g)/255.f,f32(b)/255.f,f32(a)/255.f);
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
