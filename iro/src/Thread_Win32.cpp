#if IRO_WIN32

#include "Platform_Win32.h"

#include "Common.h"
#include "Logger.h"
#include "Platform.h"
#include "Thread.h"
#include "Unicode.h"

using namespace iro;

static iro::Logger logger =
  iro::Logger::create("thread.win32"_str, iro::Logger::Verbosity::Info);

static DWORD main_thread_id = GetCurrentThreadId();

namespace iro::thread
{

/* ============================================================================
 */
struct Win32Thread
{
  HANDLE handle;
  FuncType* func;
  void* data;
  u64 bytes;
};

/* ----------------------------------------------------------------------------
 */
DWORD WINAPI win32ThreadFunc(void* data)
{
  Win32Thread* thread = (Win32Thread*)data;
  assertpointer(thread);

  void* memory = nullptr;
  if (thread->bytes > 0)
  {
    memory = platform::reserveMemory(thread->bytes);
    if (memory == nullptr)
    {
      ERROR("failed to reserve memory for win32 thread");
      return 1;
    }

    if (!platform::commitMemory(memory, thread->bytes))
    {
      ERROR("failed to commit memory for win32 thread");
      return 1;
    }
  }

  Context context = {};
  context.allocator.memory = (u8*)memory;
  context.allocator.size = thread->bytes;
  context.allocator.used = 0;
  context.data = thread->data;

  void* result = thread->func(&context);

  if (thread->bytes > 0)
  {
    platform::decommitMemory(memory, thread->bytes);
    platform::releaseMemory(memory, thread->bytes);
  }

  return (DWORD)(u64)result;
}

/* ----------------------------------------------------------------------------
 */
void* create(FuncType* func, void* data, u64 thread_memory_size)
{
  Win32Thread* thread = mem::stl_allocator.allocateType<Win32Thread>();
  if (thread == nullptr)
  {
    ERROR("failed to allocate win32 thread");
    return nullptr;
  }

  thread->func = func;
  thread->data = data;
  thread->bytes = thread_memory_size;

  HANDLE handle = CreateThread(0, 0, win32ThreadFunc, thread, 0, 0);
  if (handle == NULL)
  {
    ERROR_WIN32("failed to create win32 thread");
    mem::stl_allocator.free(thread);
    return nullptr;
  }

  thread->handle = handle;

  return thread;
}

/* ----------------------------------------------------------------------------
 */
b8 join(void* handle, u64 timeout_ms)
{
  Win32Thread* thread = (Win32Thread*)handle;
  if (thread == nullptr)
    return false;

  DWORD result = WaitForSingleObject(thread->handle, timeout_ms);
  CloseHandle(thread->handle);
  mem::stl_allocator.free(thread);

  return result == WAIT_OBJECT_0;
}

/* ----------------------------------------------------------------------------
 */
void detach(void* handle)
{
  Win32Thread* thread = (Win32Thread*)handle;
  if (thread == nullptr)
    return;

  CloseHandle(thread->handle);
  mem::stl_allocator.free(thread);
}

/* ----------------------------------------------------------------------------
 */
b8 isMainThread()
{
  return GetCurrentThreadId() == main_thread_id;
}

} // namespace iro::thread

#endif // #if IRO_WIN32
