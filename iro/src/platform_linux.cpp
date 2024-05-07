#include "platform.h"

#include "logger.h"
#include "unicode.h"

#include "fcntl.h"
#include "errno.h"
#include "unistd.h"
#include "string.h"
#include "sys/uio.h"

#include "ctime"

namespace iro::platform
{

static iro::Logger logger = 
	iro::Logger::create("platform.linux"_str, iro::Logger::Verbosity::Warn);

/* ------------------------------------------------------------------------------------------------ open
 */
b8 open(fs::File::Handle* out_handle, str path, fs::OpenFlags flags)
{
	assert(out_handle);

	int oflags = 0;
	
#define flagmap(x, y) if (flags.test(fs::OpenFlag::x)) oflags |= y;
	
	flagmap(Append,    O_APPEND);
	flagmap(Create,    O_CREAT);
	flagmap(Truncate,  O_TRUNC);
	flagmap(Exclusive, O_EXCL);
	flagmap(NoFollow,  O_NOFOLLOW);
	flagmap(NoBlock,   O_NONBLOCK);

#undef flagmap

	int mode = S_IRWXU;

	int r = ::open((char*)path.bytes, oflags, mode);

	// TODO(sushi) handle non-null-terminated strings
	if (r == -1)
	{
		ERROR("failed to open file at path '", path, "': ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return false;
	}

	*out_handle = (void*)(s64)r;

	return true;
}

/* ------------------------------------------------------------------------------------------------ close
 */
b8 close(fs::File::Handle handle)
{
	if (::close((s64)handle) == -1)
	{
		ERROR("failed to close file with handle ", handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ read
 */
s64 read(fs::File::Handle handle, Bytes buffer)
{
	int r = ::read((s64)handle, buffer.ptr, buffer.len);

	if (r == -1)
	{
		ERROR("failed to read from file with handle ", handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return -1;
	}

	return r;
}

/* ------------------------------------------------------------------------------------------------ write
 */
s64 write(fs::File::Handle handle, Bytes buffer)
{
	int r = ::write((s64)handle, buffer.ptr, buffer.len);

	if (r == -1)
	{
		ERROR("failed to write to file with handle ", handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return -1;
	}

	return r;
}

/* ------------------------------------------------------------------------------------------------ clock_realtime
 */
Timespec clock_realtime()
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return {(u64)ts.tv_sec, (u64)ts.tv_nsec};
}

/* ------------------------------------------------------------------------------------------------ clock_monotonic
 */
Timespec clock_monotonic()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return {(u64)ts.tv_sec, (u64)ts.tv_nsec};
}

}
