#if IRO_LINUX

#include "Common.h"
#include "Logger.h"
#include "Platform.h"
#include "Thread.h"
#include "Unicode.h"

#include "errno.h"
#include "pthread.h"
#include "string.h"

using namespace iro;

static iro::Logger logger =
  iro::Logger::create("thread.linux"_str, iro::Logger::Verbosity::Info);

static pthread_t main_thread_handle = pthread_self();

namespace iro::thread
{

/* ============================================================================
 */
struct LinuxThread
{
  pthread_t handle;
  FuncType* func;
  void* data;
  u64 bytes;
};

/* ----------------------------------------------------------------------------
 */
static void* linuxThreadFunc(void* data)
{
  LinuxThread* thread = (LinuxThread*)data;

  void* memory = nullptr;
  if (thread->bytes > 0)
  {
    memory = platform::reserveMemory(thread->bytes);
    if (memory == nullptr)
    {
      ERROR("failed to reserve memory for linux thread");
      return (void*)1;
    }

    if (!platform::commitMemory(memory, thread->bytes))
    {
      ERROR("failed to commit memory for linux thread");
      return (void*)1;
    }
  }

  Context context = {};
  context.allocator.memory = (u8*)memory;
  context.allocator.size = thread->bytes;
  context.allocator.used = 0;
  context.data = thread->data;

  assert(thread->func != nullptr);

  void* result = thread->func(&context);

  if (thread->bytes > 0)
  {
    platform::decommitMemory(memory, thread->bytes);
    platform::releaseMemory(memory, thread->bytes);
  }

  return result;
}

/* ----------------------------------------------------------------------------
 */
void* create(FuncType* func, void* data, u64 thread_memory_size)
{
  LinuxThread* thread = mem::stl_allocator.allocateType<LinuxThread>();
  if (thread == nullptr)
  {
    ERROR("failed to allocate linux thread");
    return nullptr;
  }

  thread->func = func;
  thread->data = data;
  thread->bytes = thread_memory_size;

  int r = pthread_create(&thread->handle, nullptr, linuxThreadFunc, thread);

  if (r != 0)
  {
    ERROR("failed to create linux thread: ", strerror(r));
    mem::stl_allocator.free(thread);
    return nullptr;
  }

  return thread;
}

/* ----------------------------------------------------------------------------
 */
b8 join(void* handle, u64 timeout_ms)
{
  LinuxThread* thread = (LinuxThread*)handle;
  if (thread == nullptr)
    return false;

  int pthread_result = pthread_join(thread->handle, nullptr);
  if (pthread_result == -1)
  {
    ERROR("failed to join linux thread: ", strerror(errno));
    mem::stl_allocator.free(thread);
    return false;
  }
  else
  {
    mem::stl_allocator.free(thread);
    return true;
  }
}

/* ----------------------------------------------------------------------------
 */
void detach(void* handle)
{
  LinuxThread* thread = (LinuxThread*)handle;
  if (thread == nullptr)
    return;

  mem::stl_allocator.free(thread);
}

/* ----------------------------------------------------------------------------
 */
b8 isMainThread()
{
  return pthread_self() == main_thread_handle;
}

} // namespace iro::thread

#endif // #if IRO_LINUX
