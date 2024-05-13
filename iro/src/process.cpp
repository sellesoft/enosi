#include "process.h"
#include "platform.h"

namespace iro
{

Process Process::open(str progname, Slice<str> args, Stream streams[3])
{
	Process out = {};
	if (!out.init(progname, args, streams))
		return nil;
	return out;
}

b8 Process::init(str progname, Slice<str> args, Stream streams[3])
{
	return platform::processSpawn(&handle, progname, args, streams);
}

}

