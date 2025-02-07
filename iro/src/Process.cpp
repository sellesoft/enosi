#include "Process.h"
#include "Platform.h"

namespace iro
{

/* ----------------------------------------------------------------------------
 */
Process Process::spawn(
    String        file, 
    Slice<String> args, 
    String        cwd)
{
  Process out = {};
  if (!platform::processSpawn(&out.handle, file, args, cwd))
    return nil;
  return out;
}

/* ----------------------------------------------------------------------------
 */
b8 Process::hasOutput() const
{
  assert(handle);
  return platform::processHasOutput(handle);
}

/* ----------------------------------------------------------------------------
 */
u64 Process::read(Bytes buffer) const
{
  assert(handle);
  return platform::processRead(handle, buffer);
}

/* ----------------------------------------------------------------------------
 */
void Process::check()
{
  assert(handle);

  if (status == Status::Running)
  {
    if (platform::processHasExited(handle, &exit_code))
    {
      status = Status::ExitedNormally;
    }
  }
}

/* ----------------------------------------------------------------------------
 */
b8 Process::close()
{
  if (handle)
  {
    if (status == Status::Running)
    {
      status = Status::ExitedNormally;
      exit_code = 0;
    }
    b8 success = platform::processClose(handle);
    handle = nullptr;
    return success;
  }
  return true;
}

}

