#include "bump.h"
#include "memory.h"

namespace iro::mem
{

/* ----------------------------------------------------------------------------
 */
b8 Bump::init()
{
  slabs = (Slab*)mem::stl_allocator.allocate(sizeof(Slab));
  if (!slabs)
    return false;
  slabs->next = nullptr;
  start = cursor = slabs->content;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Bump::deinit()
{
  while (slabs)
  {
    Slab* next = slabs->next;
    mem::stl_allocator.free(slabs);
    slabs = next;
  }
}

/* ----------------------------------------------------------------------------
 */
void* Bump::allocate(u64 size)
{
  assert(size < slab_size);

  // we store the size of the allocation with the data
  // to support reallocation
  // idrk if this a good idea
  u64 total_size = size + sizeof(u64);

  if (cursor - start + total_size > slab_size)
  {
    Slab* newslab = (Slab*)mem::stl_allocator.allocate(sizeof(Slab));
    newslab->next = slabs;
    slabs = newslab;
    start = cursor = newslab->content;
  }

  u64* header = (u64*)cursor;
  *header = size;
  u8* out = (u8*)(header + 1);
  cursor += total_size;
  return out;
}

/* ----------------------------------------------------------------------------
 */
void* Bump::reallocate(void* ptr, u64 new_size)
{
  u64 old_size = *((u64*)ptr - 1);
  if (old_size < new_size)
  {
    void* dst = allocate(new_size);
    mem::copy(dst, ptr, old_size);
    return dst;
  }
  return ptr;
}

/* ----------------------------------------------------------------------------
 */
b8 LenientBump::init()
{
  slabs = (Slab*)mem::stl_allocator.allocate(sizeof(Slab) + slab_size);
  if (!slabs)
    return false;
  slabs->next = nullptr;
  slabs->size = slab_size;
  start = cursor = (u8*)slabs + sizeof(Slab);
  return true;
}

/* ----------------------------------------------------------------------------
 */
void LenientBump::deinit()
{
  while (slabs)
  {
    Slab* next = slabs->next;
    mem::stl_allocator.free(slabs);
    slabs = next;
  }
}

/* ----------------------------------------------------------------------------
 */
void* LenientBump::allocate(u64 size)
{
  u64 total_size = size + sizeof(u64);

  if (cursor - start + total_size > slabs->size)
  {
    // TODO(sushi) if there is still space in prev slab, keep using it!
    u64 new_size = total_size > slab_size ? total_size : slab_size;
    Slab* newslab = 
      (Slab*)mem::stl_allocator.allocate(sizeof(Slab) + new_size);
    newslab->next = slabs;
    slabs = newslab;
    newslab->size = new_size;
    start = cursor = (u8*)newslab + sizeof(Slab);
  }

  u64* header = (u64*)cursor;
  *header = size;
  u8* out = (u8*)(header + 1);
  cursor += total_size;
  return out;
}

/* ----------------------------------------------------------------------------
 */
void* LenientBump::reallocate(void* ptr, u64 new_size)
{
  u64 old_size = *((u64*)ptr - 1);
  if (old_size < new_size)
  {
    void* dst = allocate(new_size);
    mem::copy(dst,ptr,old_size);
    return dst;
  }
  return ptr;
}

}
