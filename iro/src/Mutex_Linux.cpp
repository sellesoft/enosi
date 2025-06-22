#include <cstring>
#include <pthread.h>
#if IRO_LINUX

#include "Common.h"
#include "Logger.h"
#include "Mutex.h"

#include "errno.h"
#include "pthread.h"
#include "time.h"

namespace iro
{

static iro::Logger logger =
  iro::Logger::create("mutex.linux"_str, iro::Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
b8 Mutex::init()
{
  int err = pthread_mutex_init((pthread_mutex_t*)&this->handle, nullptr);
  if (err != 0)
    return ERROR("Failed to initialize mutex: ", strerror(err), "\n");
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Mutex::deinit()
{
  if (nullptr != this->handle)
  {
    pthread_mutex_destroy((pthread_mutex_t*)&this->handle);
    this->handle = nullptr;
  }
}

/* ----------------------------------------------------------------------------
 */
void Mutex::lock()
{
  if (0 != pthread_mutex_lock((pthread_mutex_t*)&this->handle))
    ERROR("Failed to lock mutex: ", strerror(errno), "\n");
}

/* ----------------------------------------------------------------------------
 */
b8 Mutex::tryLock(u32 timeout_ms)
{
  if (timeout_ms > 0)
  {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += timeout_ms * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;
    return 0 == pthread_mutex_timedlock((pthread_mutex_t*)&this->handle, &ts);
  }
  return 0 == pthread_mutex_trylock((pthread_mutex_t*)&this->handle);
}

/* ----------------------------------------------------------------------------
 */
void Mutex::unlock()
{
  if (0 != pthread_mutex_unlock((pthread_mutex_t*)&this->handle))
    ERROR("Failed to unlock mutex: ", strerror(errno), "\n");
}

} // namespace iro

#endif // #if IRO_LINUX
