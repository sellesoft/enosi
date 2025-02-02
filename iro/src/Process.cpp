#include "Process.h"
#include "Platform.h"

namespace iro
{

Process Process::spawn(
    String        file, 
    Slice<String> args, 
    Stream        streams[3], 
    String        cwd)
{
  Process out = {};
  if (!platform::processSpawn(&out.handle, file, args, streams, cwd))
    return nil;
  return out;
}

void Process::checkStatus()
{
  assert(handle);

  if (status == Status::Running)
  {
    switch (platform::processCheck(handle, &exit_code))
    {
    case platform::ProcessCheckResult::Exited:
      status = Status::ExitedNormally;
      break;
    case platform::ProcessCheckResult::Error:
      status = Status::ExitedFatally;
      break;
    }
  }
}

b8 Process::stop(s32 exit_code)
{
  assert(handle);
  if (status == Status::Running)
    return platform::stopProcess(handle, exit_code);
  return false;
}

}

