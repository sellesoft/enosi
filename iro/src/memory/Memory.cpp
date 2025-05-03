#include "Memory.h"
#include "string.h"

namespace iro::mem
{

void copy(void* dst, void* src, u64 bytes)
{
  memcpy(dst, src, bytes);
}

void move(void* dst, void* src, u64 bytes)
{
  memmove(dst, src, bytes);
}

void zero(void* ptr, u64 bytes)
{
  memset(ptr, 0, bytes);
}

b8 equal(void* lhs, void* rhs, u64 bytes)
{
  return 0 == memcmp(lhs, rhs, bytes);
}

}
