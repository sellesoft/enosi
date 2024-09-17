#include "process.h"
#include "platform.h"

namespace iro
{

Process Process::spawn(
    str        file, 
    Slice<str> args, 
    Stream     streams[3], 
    str        cwd)
{
  Process out = {};
  if (!platform::processSpawn(&out.handle, file, args, streams, cwd))
    return nil;
  return out;
}

Process Process::spawnpty(str file, Slice<str> args, fs::File* stream)
{
  Process out = {};
  if (!platform::processSpawnPTY(&out.handle, file, args, stream))
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



}

