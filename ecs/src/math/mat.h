/*
 *  Types for dealing with matrices.
 */

#ifndef _ecs_math_mat_h
#define _ecs_math_mat_h

#include "iro/common.h"
#include "math/vec.h"

struct mat3x2
{
  f32 arr[3*2];

  void set(u32 x, u32 y, f32 v)
  {
    arr[x + y * 3] = v;
  }

  f32 get(u32 x, u32 y) const
  {
    return arr[x + y * 3];
  }

  static mat3x2 identity()
  {
    return 
    {
      1.f, 0.f, 0.f,
      0.f, 1.f, 0.f,
    };
  }

  static void calcScreenMatrices(
      vec2f screen_size, 
      mat3x2* proj,
      mat3x2* view);

  static mat3x2 createTransform(
      vec2f pos,
      f32   rotation,
      vec2f scale = {1.f,1.f})
  {
    f32 s = sin(rotation);
    f32 c = cos(rotation);

    return 
    {
      c * scale.x, -s * scale.y, pos.x,
      s * scale.x,  c * scale.y, pos.y,
    };
  }

  static mat3x2 createInverseTransform(
      vec2f pos,
      f32   rotation,
      vec2f scale = {1.f,1.f})
  {
    f32 s = sin(rotation);
    f32 c = cos(rotation);

    return
    {
       c / scale.x, s / scale.x, -(pos.x * c + pos.y * s) / scale.x,
      -s / scale.x, c / scale.y,  (pos.x * s - pos.y * c) / scale.y,
    };
  }
};

#endif
