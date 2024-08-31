/*
 *  Types for dealing with vectors.
 */

#ifndef _ecs_vec_h
#define _ecs_vec_h

#include "iro/common.h"

// Not a fan of doing this, but whatever.
#include "cmath"

/* ============================================================================
 */
template<typename T>
struct vec2T
{
  typedef vec2T<T> vec2;

  union
  {
    T arr[2];
    struct { T x; T y; };
    struct { T w; T h; };
    struct { T u; T v; };
  };

  vec2T<T>() { x = 0; y = 0; }
  vec2T<T>(T x, T y) { this->x = x; this->y = y; }

  /* --------------------------------------------------------------------------
   */
  inline vec2 operator+ (const vec2& rhs) const 
  {
    return { x + rhs.x, y + rhs.y };
  }

  /* --------------------------------------------------------------------------
   */
  inline void operator+= (const vec2& rhs) const
  {
    x += rhs.x;
    y += rhs.y;
  }


  /* --------------------------------------------------------------------------
   */
  inline vec2 operator- (const vec2& rhs) const
  {
    return { x - rhs.x, y - rhs.x };
  }

  /* --------------------------------------------------------------------------
   */
  inline void operator-= (const vec2& rhs) const
  {
    x -= rhs.x;
    y -= rhs.y;
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 operator* (T rhs) const
  {
    return { x * rhs, y * rhs };
  }

  /* --------------------------------------------------------------------------
   */
  inline void operator*= (T rhs) const
  {
    x *= rhs;
    y *= rhs;
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 operator/ (T rhs) const
  {
    return { x / rhs, y / rhs };
  }

  /* --------------------------------------------------------------------------
   */
  inline void operator/= (T rhs) const
  {
    x /= rhs;
    y /= rhs;
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 operator- () const
  {
    return { -x, -y };
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2& set(T x, T y)
  {
    this->x = x;
    this->y = y;
    return *this;
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2& setX(T x)
  {
    this->x = x;
    return *this;
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2& setY(T y)
  {
    this->y = y;
    return *this;
  }

  /* --------------------------------------------------------------------------
   */
  inline T dot(const vec2& rhs) const
  {
    return (x * rhs.x) + (y * rhs.y);
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 perp() const
  {
    return { -y, x };
  }

  /* --------------------------------------------------------------------------
   */
  inline T mag() const
  {
    return sqrt(magSq());
  }

  /* --------------------------------------------------------------------------
   */
  inline T magSq() const
  {
    return x * x + y * y;
  }

  /* --------------------------------------------------------------------------
   */
  inline void normalize()
  {
    *this = normalized();
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 normalized() const
  {
    if (x || y)
      return *this /= mag();
    else
      return *this;
  }

  /* --------------------------------------------------------------------------
   */
  inline void clampMag(T min, T max)
  {
    *this = clampedMag(min, max);
  }

  /* --------------------------------------------------------------------------
   */
  inline vec2 clampedMag(T min, T max) const
  {
    T m = mag();
    if (m < min)
      return min * normalized();
    else if (m > max)
      return max * normalized();
    else 
      return *this;
  }
};

typedef vec2T<s32> vec2i;
typedef vec2T<f32> vec2;

#endif
