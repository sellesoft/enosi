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

/* ------------------------------------------------------------------------------------------------ error helper
 */
template<typename... T>
b8 reportErrno(T... args)
{
	ERROR(args..., ": ", strerrorname_np(errno), " ", strerror(errno), "\n");
	errno = 0;
	return false;
}

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
		return reportErrno("failed to open file at path '", path, "'");

	*out_handle = (void*)(s64)r;

	return true;
}

/* ------------------------------------------------------------------------------------------------ close
 */
b8 close(fs::File::Handle handle)
{
	if (::close((s64)handle) == -1)
		return reportErrno("failed to close file with handle ", handle);
	return true;
}

/* ------------------------------------------------------------------------------------------------ read
 */
s64 read(fs::File::Handle handle, Bytes buffer)
{
	int r = ::read((s64)handle, buffer.ptr, buffer.len);

	if (r == -1)
		return reportErrno("failed to read from file with handle ", handle);
	return r;
}

/* ------------------------------------------------------------------------------------------------ write
 */
s64 write(fs::File::Handle handle, Bytes buffer)
{
	int r = ::write((s64)handle, buffer.ptr, buffer.len);

	if (r == -1)
		return reportErrno("failed to write to file with handle ", handle);
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
		return reportErrno("failed to open dir at path '", path, "'");

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
		return reportErrno("failed to open dir from file handle ", file_handle, "'");

	*out_handle = dir;
	return false;
}

/* ------------------------------------------------------------------------------------------------ closedir
 */
b8 closedir(fs::Dir::Handle handle)
{
	int r = ::closedir((DIR*)handle);

	if (r == -1)
		return reportErrno("failed to close dir with handle ", handle);

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
		reportErrno("failed to read directory stream with handle ", handle);
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
			errno = 0;
		}
		else
			reportErrno("failed to stat file at path '", path, "'");
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

/* ------------------------------------------------------------------------------------------------ fileExists
 */
b8 fileExists(str path)
{
	return access((char*)path.bytes, F_OK) == 0;
}

/* ------------------------------------------------------------------------------------------------ makeDir
 */
b8 makeDir(str path, b8 make_parents)
{
	assert(notnil(path));

	if (path.len == 0)
		return true;

	// early out if path already exists
	struct stat __unused__ = {};
	if (::stat((char*)path.bytes, &__unused__) != -1)
		return true;

	if (!make_parents)
	{
		if (-1 == mkdir((char*)path.bytes, 0777))
			return reportErrno("failed to make directory at path '", path, "'");
		return true;
	}

	// TODO(sushi) if this is ever hit make it use dynamic alloc to handle it 
	if (path.len >= 4096)
	{
		ERROR("path given to makeDir is too long! the limit is 4096 characters. Path is ", path, "\n");
		return false;
	}

	u8 path_buffer[4096] = {};

	for (s32 i = 0; i < path.len; i++)
	{
		u8 c = path_buffer[i] = path.bytes[i];
		
		if (c == '/' || i == path.len - 1)
		{
			if (-1 == mkdir((char*)path_buffer, 0777))
			{
				if (errno != EEXIST)
					return reportErrno("failed to make directory at path '", path, "'");
				errno = 0;
			}
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ processSpawn
 */
b8 processSpawn(Process::Handle* out_handle, str file, Slice<str> args, Process::Stream streams[3])
{
	assert(out_handle);

	// NOTE(sushi) first arg must be the same as the file being executed
	assert(file == args[0]);

	struct Info
	{
		fs::File* f = nullptr;
		b8 non_blocking = false;
		int pipes[2] = {};
	};

	Info infos[3] = 
	{
		{streams[0].file, streams[0].non_blocking},
		{streams[1].file, streams[1].non_blocking},
		{streams[2].file, streams[2].non_blocking},
	};

	for (s32 i = 0; i < 3; i++)
	{
		if (!infos[i].f)
			continue;

		if (notnil(*infos[i].f))
		{
			ERROR("Stream ", i, " file given to processSpawn is not nil! Given files cannot already own resources.\n");
			return false;
		}

		if (-1 == pipe(infos[i].pipes))
			return reportErrno("failed to open pipes for stream ", i, " for process '", file, "'");
	}

	if (pid_t pid = fork())
	{
		// parent branch

		if (pid == -1)
			return reportErrno("failed to fork process");

		// close uneeded pipes
		for (s32 i = 0; i < 3; i++)
		{
			if (!infos[i].f)
			{
				if (i == 0)
				{
					// if no stream was given for stdin then
					// close the writing end of that pipe
					::close(infos[i].pipes[1]);
				}
				else
				{
					// if no stream was given for stdout/err
					// then close the reading ends of those pipes
					::close(infos[i].pipes[0]);
				}
			}
			else
			{
				fs::OpenFlags flags = {};

				if (infos[i].non_blocking)
					flags.set(fs::OpenFlag::NoBlock);

				if (i == 0)
				{
					// close reading end of stdin pipe and set stream file
					// to write to it 
					::close(infos[i].pipes[0]);
					int fd = dup(infos[i].pipes[1]);
					flags.set(fs::OpenFlag::Write);
					*infos[i].f = fs::File::fromFileDescriptor(fd, flags);
				}
				else
				{
					// close writing end of stdout/err pipes and set stream
					// files to read from them
					::close(infos[i].pipes[1]);
					int fd = dup(infos[i].pipes[0]);
					flags.set(fs::OpenFlag::Read);
					*infos[i].f = fs::File::fromFileDescriptor(fd, flags);
				}
			}
		}

		*out_handle = (void*)(s64)pid;
	}
	else
	{
		// child branch

		for (s32 i = 0; i < 3; i++)
		{
			if (!infos[i].f)
			{
				if (i == 0)
				{
					// if no stream was given for stdin then close 
					// the reading end of that pipe (we wont be reading from it)
					::close(infos[i].pipes[0]);
				}
				else
				{
					// if no stream was given for stdout/err
					// then close the writing ends of those pipes
					::close(infos[i].pipes[1]);
				}
			}
			else
			{
				if (i == 0)
				{
					// close writing end of stdin pipe
					::close(infos[i].pipes[1]);
					// replace our stdin fd with the one created by the parent
					dup2(infos[i].pipes[0], 0);
				}
				else
				{
					// close reading end of stdout/err pipes
					::close(infos[i].pipes[0]);
					// replace our stdout/err pipes with the ones created by the parent
					dup2(infos[i].pipes[1], i);
				}
			}
		}



		if (-1 == execvp((char*)args[0].bytes, (char**)args.ptr))
			return reportErrno("execvp failed to replace child process with file '", file, "'");
	}
}

}

