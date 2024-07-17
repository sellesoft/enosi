/*
 *  Bump allocator, which is pretty much a chunked arena.
 *
 *  Allocates memory that we dont intend to cleanup until we're finished with 
 *  all of it. So this does not implement free. In order to support 
 *  reallocating, though, we do still store the size of an allocation before 
 *  the actual data.
 */

#ifndef _iro_bump_h
#define _iro_bump_h

#include "allocator.h"
#include "assert.h"

namespace iro::mem
{

/* ============================================================================
 */
struct Bump : public Allocator
{
  const static u32 slab_size = 4096;

  struct Slab
  {
    Slab* next = nullptr;
    u8 content[slab_size] = {};
  };

  u8* start;
  u8* cursor;

  Slab* slabs;

  b8   init();
  void deinit();

  void* allocate(u64 size) override;

  void* reallocate(void* ptr, u64 size) override;
  
  [[deprecated("Bump does not implement free! It is OK to call this function "
               "directly, but it will do nothing.")]]
  void free(void* ptr) override {}
};

/* ============================================================================
 *  A variant of bump that allocates slabs of size 4096 but will also allocate
 *  slabs of arbitrary size if necessary. Useful for collecting strings that
 *  may be really big and what not.
 */
struct LenientBump : public Allocator
{
  const static u32 slab_size = 4096;

  struct Slab
  {
    Slab* next = nullptr;
    u32 size = 0;
  };

  u8* start = nullptr;
  u8* cursor = nullptr;

  Slab* slabs;

  b8   init();
  void deinit();

  void* allocate(u64 size) override;
  void* reallocate(void* ptr, u64 size) override;

  [[deprecated("LenientBump does not implement free! It is OK to call this "
               "function directly, but it will do nothing.")]]
  void free(void* ptr) override {}
};

}

#endif // _iro_bump_h
