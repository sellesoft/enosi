/*
 *  API for creating and iteracting with OS processes.
 */

#ifndef _iro_process_h
#define _iro_process_h

#include "common.h"
#include "unicode.h"
#include "containers/slice.h"
#include "containers/stackarray.h"

#include "nil.h"

#include "fs/fs.h"

namespace iro
{

/* ================================================================================================ Process
 */
struct Process
{
	typedef void* Handle;

	Handle handle;

    /* ============================================================================================ Process:Stream
	 *  A file stream that may replace one of the process' stdio streams.
     */
	struct Stream
	{
		b8 non_blocking;
		fs::File* file;
	};
	
	static Process open(str progname, Slice<str> args, Stream streams[3]);

	b8 init(str progname, Slice<str> args, Stream streams[3]);

};

}

DefineNilValue(iro::Process, {nullptr}, { return x.handle == nullptr; });

#endif
