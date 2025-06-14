/*
 *  A wrapper around C-arrays that just adds a count and some helper functions.
 */

#ifndef _iro_StackArray_h
#define _iro_StackArray_h

#include "../Common.h"
#include "Slice.h"
#include "ArrayFuncs.h"

#include <new>

namespace iro
{

/* ============================================================================
 */
template<typename T, s32 Capacity>
struct StackArray
{
  T arr[Capacity] = {};
  s32 len = 0;


  /* --------------------------------------------------------------------------
   */
  s32 capacity() const { return Capacity; }

  /* --------------------------------------------------------------------------
   */
  b8 isFull() const { return len == Capacity; }

  /* --------------------------------------------------------------------------
   */
  b8 isEmpty() const { return len == 0; }

  /* --------------------------------------------------------------------------
   */
  T* push()
  {
    if (isFull())
      return nullptr;
    return new (arr + len++) T;
  }

  /* --------------------------------------------------------------------------
   */
  b8 push(const T& v)
  {
    if (T* p = push())
    {
      *p = v;
      return true;
    }

    return false;
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
  void remove(T* ptr)
  {
    remove(ptr - arr);
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
  void removeUnordered(T* ptr)
  {
    removeUnordered(ptr - arr);
  }

  /* --------------------------------------------------------------------------
   */
  void removeUnordered(s32 idx)
  {
    assert(idx >= 0 && idx < len);
    array::removeUnordered(arr, &len, idx);
  }

  /* --------------------------------------------------------------------------
   */
  void clear()
  {
    len = 0;
  }

  /* --------------------------------------------------------------------------
   */
  Slice<T> asSlice()
  {
    return {arr, u64(len)};
  }

  /* --------------------------------------------------------------------------
   */
  T& operator[](s32 i) { assert(i < len); return arr[i]; }
  const T& operator[](s32 i) const { assert(i < len); return arr[i]; }

  T* begin() { return arr; }
  T* end() { return arr + len; }

  const T* begin() const { return arr; }
  const T* end() const { return arr + len; }

  T* last() { return isEmpty()? nullptr : arr + len - 1; }
  const T* last() const { return isEmpty()? nullptr : arr + len - 1; }
};

}

#endif // _iro_stackarray_h
