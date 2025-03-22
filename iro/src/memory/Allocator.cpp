#include "Allocator.h"
#include "stdlib.h"
#include "stdio.h"

#include "../Logger.h"

namespace iro::mem
{

// default allocator used by containers and such
STLAllocator stl_allocator = {};

void* STLAllocator::allocate(u64 size)
{
#if IRO_DEBUG
  bytes_allocated += size;
  u64* ptr = (u64*)malloc(sizeof(u64) + size);
  *ptr = size;
  // printf("%p + %lu\n", ptr, size);
  return ptr + 1;
#else
  return malloc(size);
#endif
}

void* STLAllocator::reallocate(void* ptr, u64 size)
{
#if IRO_DEBUG
  u64* real_ptr = (u64*)ptr - 1;
  bytes_allocated = bytes_allocated - *real_ptr + size;
  *real_ptr = size;
  // printf("%p %lu -> %lu\n", real_ptr, *real_ptr, size);
  return (u64*)realloc(real_ptr, sizeof(u64) + size) + 1;
#else
  return realloc(ptr, size);
#endif
}

void STLAllocator::free(void* ptr)
{
#if IRO_DEBUG
  u64* real_ptr = (u64*)ptr - 1;
  bytes_allocated -= *real_ptr;
  // printf("%p - %lu\n", real_ptr, *real_ptr);
  ::free(real_ptr);
#else
  ::free(ptr);
#endif
}

}
