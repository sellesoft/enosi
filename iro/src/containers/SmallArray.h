/*
 *  An array optimized to first store N values on the stack before 
 *  resorting to dynamic allocation of its contents. Used in cases where
 *  we can assume a likely upper limit on the number of elements stored
 *  but cannot guarantee it.
 *
 *  Currently does not handle moving back to the stack if we remove enough
 *  elements to be below the threshold again unless if we do a clear().
 */

#ifndef _iro_SmallArray_h
#define _iro_SmallArray_h

#include "Slice.h"

#include "../Common.h"
#include "../memory/Memory.h"
#include "../memory/Allocator.h"

#include "../traits/Nil.h"
#include "../traits/Move.h"
#include "../traits/Container.h"
#include "../traits/Iterable.h"

#include "ArrayFuncs.h"

namespace iro
{

/* ============================================================================
 */
template<typename T, s32 N>
struct SmallArray
{
  T small_arr[N];

  // Actual pointer to the data we are working with.
  T* arr = small_arr;

  mem::Allocator* big_allocator = nullptr;

  s32 len = 0;
  s32 space = N;

  SmallArray() { init(); }
  ~SmallArray() { deinit(); }

  /* --------------------------------------------------------------------------
   */ 
  b8 init(mem::Allocator* big_allocator = &mem::stl_allocator)
  {
    this->big_allocator = big_allocator;
    arr = small_arr;
    return true;
  }

  /* --------------------------------------------------------------------------
   */ 
  b8 isInit() const
  {
    return big_allocator != nullptr;
  }

  /* --------------------------------------------------------------------------
   */ 
  b8 isEmpty() const
  {
      return len == 0;
  }

  /* --------------------------------------------------------------------------
   */ 
  void deinit()
  {
    clear();
    big_allocator = nullptr;
  }

  /* --------------------------------------------------------------------------
   */
  void move(SmallArray* dst)
  {
    dst->deinit();
    dst->len = len;
    dst->space = space;
    dst->big_allocator = big_allocator;
    if (isSmall())
    {
      mem::copy(dst->small_arr, small_arr, sizeof(T) * len);
      dst->arr = dst->small_arr;
    }
    else
    {
      dst->arr = arr;
    }
    arr = small_arr;
    len = space = 0;
  }

  /* --------------------------------------------------------------------------
   */
  b8 isSmall() const
  {
    return arr == small_arr;
  }

  /* --------------------------------------------------------------------------
   */ 
  void growIfNeeded()
  {
    if (isSmall())
    {
      if (len == N)
      {
        // Move to heap.
        arr = big_allocator->allocateType<T>(N + 1);
        mem::copy(arr, small_arr, sizeof(T) * N);
      }
    }
    else
    {
      if (len + 1 > space)
      {
        space *= 2;
        arr = big_allocator->reallocateType<T>(arr, space);
      }
    }
  }

  /* --------------------------------------------------------------------------
   */ 
  T* push()
  {
    growIfNeeded();
    return array::push(arr, &len);
  }

  /* --------------------------------------------------------------------------
   */ 
  void push(const T& x)
  {
    growIfNeeded();
    array::push(arr, &len, x);
  }

  /* --------------------------------------------------------------------------
   */ 
  void pop()
  {
    assert(len != 0);
    array::pop(arr, &len);
  }

  /* --------------------------------------------------------------------------
   */ 
  void insert(s32 idx, const T& x)
  {
    assert(idx >= 0 && idx <= len);
    growIfNeeded();

    array::insert(arr, &len, idx, x);
  }

  /* --------------------------------------------------------------------------
   */ 
  T* insert(s32 idx)
  {
    assert(idx >= 0 && idx <= len);
    growIfNeeded();

    return array::insert(arr, &len, idx);
  }

  /* --------------------------------------------------------------------------
   */ 
  void remove(T* x)
  {
    remove(x - arr);
  }

  /* --------------------------------------------------------------------------
   */
  void remove(s32 idx)
  {
    assert(idx >= 0 && idx < len);
    array::remove(arr, &len, idx);
  }

  /* --------------------------------------------------------------------------
   */
  void clear()
  {
    array::clear(arr, &len);
    if (!isSmall())
    {
      big_allocator->free(arr);
      arr = small_arr;
      space = N;
      // I do not THINK I need to call deconstructors here because the 
      // elements originally in the small array will have had them called
      // via the clear on arr.
      mem::zero(small_arr, sizeof(T) * N);
    }
  }
  
  /* --------------------------------------------------------------------------
   */
  T* begin() { return arr; }
  T* end() { return arr + len; }

  T* first() { return arr; }
  T* last() { return arr + len; }

  /* --------------------------------------------------------------------------
   */ 
  T& operator[](s32 idx) const
  {
    return arr[idx];
  }

  /* --------------------------------------------------------------------------
   */ 
  Slice<T> asSlice() const
  {
    return Slice<T>::from(arr, len);
  }
};

}

#endif
