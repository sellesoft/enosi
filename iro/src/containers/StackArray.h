/*
 *  A wrapper around C-arrays that just adds a count and some helper functions.
 */

#ifndef _iro_StackArray_h
#define _iro_StackArray_h

#include "../Common.h"
#include "Slice.h"

#include <new>

namespace iro
{

/* ============================================================================
 */
template<typename T, s64 Capacity>
struct StackArray
{
  T arr[Capacity] = {};
  u64 len = 0;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  /* --------------------------------------------------------------------------
   */
  u64 capacity() { return Capacity; }

  /* --------------------------------------------------------------------------
   */
  T* push()
  {
    if (len == Capacity)
      return nullptr;
    return new (arr + len++) T;
  }
  
  /* --------------------------------------------------------------------------
   */
  void pop()
  {
    if (!len)
      return;
    len -= 1;
  }

  /* --------------------------------------------------------------------------
   */
  Slice<T> asSlice()
  {
    return {arr, len};
  }

  /* --------------------------------------------------------------------------
   */
  T& operator[](u64 i) { assert(i < len); return arr[i]; }
  const T& operator[](u64 i) const { assert(i < len); return arr[i]; }

  T* begin() { return arr; }
  T* end() { return arr + len; }

  const T* begin() const { return arr; }
  const T* end() const { return arr + len; }
};

}

#endif // _iro_stackarray_h
