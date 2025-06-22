#ifndef _iro_Mutex_h
#define _iro_Mutex_h

#include "Common.h"

namespace iro
{

/* ============================================================================
 */
struct Mutex
{
  void* handle = nullptr;

  b8 init();
  static inline Mutex create() { Mutex mutex; mutex.init(); return mutex; }
  void deinit();
  void lock();
  b8 tryLock(u32 timeout_ms = 0);
  void unlock();
};

/* ============================================================================
 */
struct ScopedMutexLock
{
  Mutex& mutex;

  explicit ScopedMutexLock(Mutex& mutex) : mutex(mutex) { mutex.lock(); }
  ~ScopedMutexLock() { mutex.unlock(); }
  ScopedMutexLock(const ScopedMutexLock&) = delete;
  ScopedMutexLock& operator=(const ScopedMutexLock&) = delete;
};

} // namespace iro

#endif // _iro_Mutex_h