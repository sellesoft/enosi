#include "Process.h"
#include "Platform.h"

namespace iro
{

/* ----------------------------------------------------------------------------
 */
Process Process::spawn(
    String        file, 
    Slice<String> args, 
    String        cwd,
    b8            non_blocking,
    b8            redirect_err_to_out)
{
  Process out = {};
  if (!platform::processSpawn(
        &out.handle, 
        file, 
        args, 
        cwd, 
        non_blocking,
        redirect_err_to_out))
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
b8 Process::hasErrOutput() const
{
  assert(handle);
  return platform::processHasErrOutput(handle);
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
u64 Process::readstderr(Bytes buffer) const
{
  assert(handle);
  return platform::processReadStdErr(handle, buffer);
}

/* ----------------------------------------------------------------------------
 */
u64 Process::write(Bytes buffer) const
{
  assert(handle);
  return platform::processWrite(handle, buffer);
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

