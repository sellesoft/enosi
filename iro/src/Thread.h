#ifndef _iro_Thread_h
#define _iro_Thread_h

#include "Common.h"
#include "memory/Memory.h"

namespace iro::thread
{

/* ============================================================================
 */
struct Allocator : public iro::mem::Allocator
{
  u8* memory;
  u64 size;
  u64 used;

  void* allocate(u64 bytes) override
  {
    assertpointer(memory);
    if (used + bytes > size)
      return nullptr;

    void* out = memory + used;
    used += bytes;
    return out;
  }

  [[deprecated("iro::thread::Allocator does not implement reallocate!")]]
  void* reallocate(void* ptr, u64 bytes) override
  {
    assert(!"reallocate cannot be used with a small allocator");
    return nullptr;
  }

  [[deprecated("iro::thread::Allocator does not implement free!")]]
  void free(void* ptr) override
  {
    assert(!"free cannot be used with a small allocator");
  }
};

/* ============================================================================
 */
struct Context
{
  Allocator allocator;
  void* data;
};

/* ============================================================================
 */
typedef void* FuncType(Context* context);
static constexpr void* INVALID_HANDLE = nullptr;

/* ----------------------------------------------------------------------------
 */
void* create(FuncType* func, void* data, u64 thread_memory_size);

/* ----------------------------------------------------------------------------
 */
b8 join(void* handle, u64 timeout_ms);

/* ----------------------------------------------------------------------------
 */
void detach(void* handle);

/* ----------------------------------------------------------------------------
 */
b8 isMainThread();

} // namespace iro::thread

#endif // _iro_Thread_h