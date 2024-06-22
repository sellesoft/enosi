#include "process.h"
#include "platform.h"

namespace iro
{

Process Process::spawn(str file, Slice<str> args, Stream streams[3], str cwd)
{
  Process out = {};
  if (!platform::processSpawn(&out.handle, file, args, streams, cwd))
    return nil;
  return out;
}

void Process::checkStatus()
{
  assert(handle);

  if (!terminated)
  {
    switch (platform::processCheck(handle, &exit_code))
    {
    case platform::ProcessCheckResult::Exited:
    case platform::ProcessCheckResult::Error:
      terminated = true;
    }
  }
}

}

