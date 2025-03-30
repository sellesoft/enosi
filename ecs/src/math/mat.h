/*
 *  Types for dealing with matrices.
 *
 *  Currently this just defines a 3x2 matrix type used to deal with proj/view
 *  matrix stuff. If we ever need other kinds of matrices they should be put
 *  here.
 *
 *  TODO(sushi) it would be nice to convert this to an lpp file that generates
 *              matrices of whatever dimensions and element type we want.
 */

#ifndef _ecs_math_mat_h
#define _ecs_math_mat_h

#include "iro/Common.h"
#include "math/vec.h"

struct mat3x2
{
  f32 arr[3*2];

  /* --------------------------------------------------------------------------
   */
  void set(u32 x, u32 y, f32 v)
  {
    arr[x + y * 3] = v;
  }

  /* --------------------------------------------------------------------------
   */
  f32 get(u32 x, u32 y) const
  {
    return arr[x + y * 3];
  }

  /* --------------------------------------------------------------------------
   */
  static mat3x2 identity()
  {
    return
    {
      1.f, 0.f, 0.f,
      0.f, 1.f, 0.f,
    };
  }

  /* --------------------------------------------------------------------------
   */
  static mat3x2 createTransform(
      vec2f pos,
      f32   rotation,
      vec2f scale = {1.f,1.f})
  {
    f32 s = sinf(rotation);
    f32 c = cosf(rotation);

    return
    {
      c * scale.x, -s * scale.y, pos.x,
      s * scale.x,  c * scale.y, pos.y,
    };
  }

  /* --------------------------------------------------------------------------
   */
  vec2f getTranslation() const
  {
    return vec2f(get(2, 0), get(2, 1));
  }

  /* --------------------------------------------------------------------------
   */
  static mat3x2 createInverseTransform(
      vec2f pos,
      f32   rotation,
      vec2f scale = {1.f,1.f})
  {
    f32 s = sinf(rotation);
    f32 c = cosf(rotation);

    return
    {
       c / scale.x, s / scale.x, -(pos.x * c + pos.y * s) / scale.x,
      -s / scale.x, c / scale.y,  (pos.x * s - pos.y * c) / scale.y,
    };
  }

  /* --------------------------------------------------------------------------
   */
  vec2f transformVec(const vec2f& v) const
  {
    return
    {
      get(0, 0) * v.x + get(1, 0) * v.y + get(2, 0),
      get(0, 1) * v.x + get(1, 1) * v.y + get(2, 1),
    };
  }

  /* --------------------------------------------------------------------------
   */
  mat3x2 mul(const mat3x2& rhs) const
  {
    return
    {
      get(0,0) * rhs.get(0,0) + get(0,1) * rhs.get(1,0),
      get(1,0) * rhs.get(0,0) + get(1,1) * rhs.get(1,0),
      get(2,0) * rhs.get(0,0) + get(2,1) * rhs.get(1,0) + rhs.get(2,0),
      get(0,0) * rhs.get(0,1) + get(0,1) * rhs.get(1,1),
      get(1,0) * rhs.get(0,1) + get(1,1) * rhs.get(1,1),
      get(2,0) * rhs.get(0,1) + get(2,1) * rhs.get(1,1) + rhs.get(2,1),
    };
  }

  /* --------------------------------------------------------------------------
   */
  void toMat3(f32 out[9]) const
  {
    out[0] = arr[0]; out[1] = arr[1]; out[2] = arr[2];
    out[3] = arr[3]; out[4] = arr[4]; out[5] = arr[5];
    out[6] = 0.f;    out[7] = 0.f;    out[8] = 1.f;
  }
  
  /* --------------------------------------------------------------------------
   */
  void toMat4(f32 out[16]) const
  {
    out[0] = arr[0]; out[1] = arr[1]; out[2] = arr[2]; out[3] = 0.f;
    out[4] = arr[3]; out[5] = arr[4]; out[6] = arr[5]; out[7] = 0.f;
    out[8] = 0.f;    out[9] = 0.f;    out[10] = 1.f;   out[11] = 0.f;
    out[12] = 0.f;   out[13] = 0.f;   out[14] = 0.f;   out[15] = 1.f;
  }
};

#endif
