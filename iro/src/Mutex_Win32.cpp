#if IRO_WIN32

#include "Platform_Win32.h"

#include "Common.h"
#include "Logger.h"
#include "Mutex.h"

namespace iro
{

static iro::Logger logger =
  iro::Logger::create("mutex.win32"_str, iro::Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
b8 Mutex::init()
{
  HANDLE handle = CreateMutex(nullptr, FALSE, nullptr);
  if (NULL == handle)
    return ERROR_WIN32("Failed to create mutex");

  this->handle = (void*)handle;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Mutex::deinit()
{
  if (nullptr != this->handle)
  {
    CloseHandle((HANDLE)this->handle);
    this->handle = nullptr;
  }
}

/* ----------------------------------------------------------------------------
 */
void Mutex::lock()
{
  if (WAIT_FAILED == WaitForSingleObject((HANDLE)this->handle, INFINITE))
    ERROR_WIN32("Failed to lock mutex");
}

/* ----------------------------------------------------------------------------
 */
b8 Mutex::tryLock(u32 timeout_ms)
{
  return WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)this->handle, timeout_ms);
}

/* ----------------------------------------------------------------------------
 */
void Mutex::unlock()
{
  if (!ReleaseMutex((HANDLE)this->handle))
    ERROR_WIN32("Failed to unlock mutex");
}

} // namespace iro

#endif // #if IRO_WIN32
