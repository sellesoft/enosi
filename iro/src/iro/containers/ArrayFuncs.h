/*
 *  Common contiguous array operations that operate on raw pointers and 
 *  counters and space and what not meant to be shared between different kinds 
 *  of array ADTs.
 *
 *  These don't handle any dynamic allocation or bounds checking. It is 
 *  assumed that the caller is handling this in whatever way is necessary!
 *
 *  BE CAREFUL!
 *
 *  I don't really know if this is a good idea.. I'm implementing this 
 *  because I'm currently adding SmallArray and I don't want to duplicate
 *  the common array operations between it and Array.
 */

#ifndef _iro_arrayfuncs_h
#define _iro_arrayfuncs_h

#include "../common.h"
#include "../memory/memory.h"

#include "assert.h"

namespace iro::array
{

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline T* push(T* arr, s32* len)
{
  *len += 1;
  return new (arr + *len - 1) T;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline void push(T* arr, s32* len, const T& v)
{
  arr[*len] = v;
  *len += 1;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline void pop(T* arr, s32* len)
{
  (arr + *len)->~T();
  *len -= 1;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline T* insert(T* arr, s32* len, s32 idx)
{
  if (*len == 0)
    return push(arr, len);
    
  mem::move(arr + idx + 1, arr + idx, sizeof(T) * (*len - idx));
  *len += 1;

  return new (arr + idx) T;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline void insert(T* arr, s32* len, s32 idx, const T& v)
{
  if (*len == 0)
    return push(arr, len, v);
    
  mem::move(arr + idx + 1, arr + idx, sizeof(T) * (*len - idx));
  *len += 1;
  arr[idx] = v;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline void remove(T* arr, s32* len, s32 idx)
{
  (arr + idx)->~T();
  mem::move(arr + idx, arr + idx + 1, sizeof(T) * (*len - idx));
  *len -= 1;
}

/* ----------------------------------------------------------------------------
 */ 
template<typename T>
static inline void clear(T* arr, s32* len)
{
  for (s32 i = 0; i < *len; ++i)
    (arr + i)->~T();
  *len = 0;
}

}

#endif
