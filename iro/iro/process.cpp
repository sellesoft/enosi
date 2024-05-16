#include "process.h"
#include "platform.h"

namespace iro
{

Process Process::spawn(str file, Slice<str> args, Stream streams[3])
{
	Process out = {};
	if (!platform::processSpawn(&out.handle, file, args, streams))
		return nil;
	return out;
}

void Process::checkStatus()
{
	assert(handle);

	if (!terminated)
		terminated = platform::processCheck(handle, &exit_code);
}

}

