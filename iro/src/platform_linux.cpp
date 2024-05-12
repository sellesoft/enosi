#include "platform.h"

#include "logger.h"
#include "unicode.h"

#include "fcntl.h"
#include "errno.h"
#include "unistd.h"
#include "dirent.h"
#include "string.h"
#include "sys/uio.h"
#include "sys/stat.h"

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

	if (flags.testAll<fs::OpenFlag::Read, fs::OpenFlag::Write>())
		oflags |= O_RDWR;
	else if (flags.test(fs::OpenFlag::Write))
		oflags |= O_WRONLY;
	else if (flags.test(fs::OpenFlag::Read))
		oflags |= O_RDONLY;
	else
	{
		ERROR("failed to open file at path '", path, "': one of OpenFlag::Read/Write was not given\n");
		return false;
	}

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

/* ------------------------------------------------------------------------------------------------ opendir
 */
b8 opendir(fs::Dir::Handle* out_handle, str path)
{
	assert(out_handle);

	DIR* dir = ::opendir((char*)path.bytes);

	if (!dir)
	{
		ERROR("failed to open dir at path '", path, "': ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return false;
	}

	*out_handle = dir;
	return true;
}

/* ------------------------------------------------------------------------------------------------ opendir
 */
b8 opendir(fs::Dir::Handle* out_handle, fs::File::Handle file_handle)
{
	assert(out_handle && file_handle);

	DIR* dir = fdopendir((s64)file_handle);

	if (!dir)
	{
		ERROR("failed to open dir from file handle ", file_handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return false;
	}

	*out_handle = dir;
	return false;
}

/* ------------------------------------------------------------------------------------------------ closedir
 */
b8 closedir(fs::Dir::Handle handle)
{
	int r = ::closedir((DIR*)handle);

	if (r == -1)
	{
		ERROR("failed to close dir with handle ", handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ readdir
 */
s64 readdir(fs::Dir::Handle handle, Bytes buffer)
{
	assert(handle && buffer.ptr && buffer.len);

	errno = 0;
	struct dirent* de = ::readdir((DIR*)handle);

	if (!de)
	{
		if (errno == 0)
			return false;
		ERROR("failed to read directory stream with handle ", handle, ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
		errno = 0;
		return -1;
	}
	
	s64 len = strlen(de->d_name);
	if (len > buffer.len)
	{
		ERROR("buffer given to readdir() is too small. File name '", de->d_name, "' is ", len, " bytes long, but given buffer is ", buffer.len, " bytes long\n");
		return -1;
	}

	mem::copy(buffer.ptr, de->d_name, len); 
	return len;
}

/* ------------------------------------------------------------------------------------------------ stat
 */
b8 stat(fs::FileInfo* out_info, str path)
{
	assert(out_info && path.bytes && path.len);

	struct statx s;

	int mode = 
		STATX_MODE |
		STATX_SIZE |
		STATX_ATIME |
		STATX_BTIME |
		STATX_CTIME |
		STATX_MTIME;

	if (::statx(AT_FDCWD, (char*)path.bytes, 0, mode, &s) == -1)
	{
		if (errno == ENOENT)
		{
			out_info->kind = fs::FileKind::NotFound;
		}
		else
		{
			ERROR("failed to stat file at path '", path, "': ", strerrorname_np(errno), " ", strerror(errno), "\n");
		}
		errno = 0;
		return false;
	}

	switch (s.stx_mode & S_IFMT)
	{
#define map(x, y) case x: out_info->kind = fs::FileKind::y; break;

		map(S_IFBLK,  Block);
		map(S_IFCHR,  Character);
		map(S_IFDIR,  Directory);
		map(S_IFIFO,  FIFO);
		map(S_IFLNK,  SymLink);
		map(S_IFREG,  Regular);
		map(S_IFSOCK, Socket);

		default: out_info->kind = fs::FileKind::Unknown;
#undef map
	}
	
	out_info->byte_size = s.stx_size;
	
	auto timespecToTimePoint = [](statx_timestamp ts) { return TimePoint{u64(ts.tv_sec), u64(ts.tv_nsec)}; };

	out_info->birth_time = timespecToTimePoint(s.stx_btime);
	out_info->last_access_time = timespecToTimePoint(s.stx_atime);
	out_info->last_modified_time = timespecToTimePoint(s.stx_mtime);
	out_info->last_status_change_time = timespecToTimePoint(s.stx_ctime);

	return true;
}

/* ------------------------------------------------------------------------------------------------ file_exists
 */
b8 file_exists(str path)
{
	return access((char*)path.bytes, F_OK) == 0;
}

}

