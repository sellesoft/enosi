/*
 *  Allocator which uses stack before resorting to the heap.
 *
 *  No memory returned may be realloc'd or free'd. This is primarily useful 
 *  for allocating a series of temporary things so they can be easily 
 *  deallocated when no longer needed, like Bump and LenientBump, but 
 *  utilizes stack before moving to heap.
 */

#ifndef _iro_smallallocator_h
#define _iro_smallallocator_h

#include "../common.h"
#include "bump.h"

namespace iro::mem
{

/* ============================================================================
 */
template<u64 N>
struct SmallAllocator : public LenientBump
{
  u64 stack_len = 0;
  b8 is_small = true;
  u8 stack[N];

  /* --------------------------------------------------------------------------
   *  Even though there is no 'init', this MUST be called incase we needed to 
   *  move to heap.
   */
  void deinit()
  {
    if (!is_small)
      LenientBump::deinit();
  }

  /* --------------------------------------------------------------------------
   *  TODO(sushi) try to use up all stack space even after we've moved to heap.
   */
  void* allocate(u64 size) override
  {
    if (!is_small)
      return LenientBump::allocate(size);

    u64 stack_remaining = N - stack_len;
    if (stack_remaining < size)
    {
      if (!LenientBump::init())
        return nullptr; // uh

      return LenientBump::allocate(size);
    }
    else
    {
      void* out = stack + stack_len;
      stack_len += size;
      return out;
    }
  }

  [[deprecated("SmallAllocator does not implement reallocate!")]]
  void* reallocate(void* ptr, u64 size) override
  {
    assert(!"reallocate cannot be used with a small allocator");
    return nullptr;
  }

  [[deprecated("SmallAllocator does not implement reallocate!")]]
  void free(void* ptr) override
  {
    assert(!"free cannot be used with a small allocator");
  }
};

/* ============================================================================
 *  Variant for scoping a small allocator.
 */
template<u64 N>
struct ScopedSmallAllocator : public SmallAllocator<N>
{
  ~ScopedSmallAllocator<N>()
  {
    SmallAllocator<N>::deinit();
  }
};

}

#endif
