#include "platform.h"
#include "lake.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"

#include "dirent.h"
#include "sys/stat.h"
#include "errno.h"
#include "fnmatch.h"
#include "glob.h"

extern "C"
{

/* ------------------------------------------------------------------------------------------------
 */
u8* read_file(str path) 
{
	int fd = open((char*)path.s, O_RDONLY);

	if (fd == -1)
	{
		error(path, 0, 0, "failed to open file");
		perror("open");
		exit(1);
	}

	struct stat st;
	fstat(fd, &st);

	u8* buffer = (u8*)mem.allocate(st.st_size);

	ssize_t bytes_read = read(fd, buffer, st.st_size);
	
	if (bytes_read != st.st_size)
	{
		error(path, 0, 0, "failed to read entire file");
		perror("read");
		exit(1);
	}

	return buffer;
}

/* ------------------------------------------------------------------------------------------------
 */
DirIter dir_iter(const char* path)
{
	return opendir(path);
}

/* ------------------------------------------------------------------------------------------------
 */
s32 dir_next(char* c, u32 maxlen, DirIter iter)
{
	errno = 0;
	struct dirent* d = readdir((DIR*)iter);

	if (!d)
	{
		if (errno)
		{
			error_nopath("dir_next(): readdir failed!");
			perror("readdir");
			exit(1);
		}
		return -1;
	}

	s32 namelen = strlen(d->d_name);

	if (!namelen)
		return 0;

	if (maxlen < namelen)
	{
		error_nopath("dir_next(): unable to fit entire directory path into given buffer of size ", maxlen, ". Size needed is ", namelen);
		exit(1);
	}

	memcpy(c, d->d_name, namelen);
	
	return namelen;
}

/* ------------------------------------------------------------------------------------------------
 *  TODO(sushi) this is pretty messy clean up later
 */
Glob glob_create(const char* pattern)
{
	glob_t* glob_sys = (glob_t*)mem.allocate(sizeof(glob_t));

	switch (glob(pattern, 0, nullptr, glob_sys))
	{
		case GLOB_NOSPACE: {
			error_nopath("glob_create(): glob() ran out of memory");
			exit(1);
		} break;

		case GLOB_ABORTED: {
			error_nopath("glob_create(): glob() aborted");
			exit(1);
		} break;

		case GLOB_NOMATCH: {
			mem.free(glob_sys);
			return {};
		} break;
	}

	Glob out = {};
	out.n_paths = glob_sys->gl_pathc;
	out.paths = (u8**)glob_sys->gl_pathv;
	out.handle = glob_sys;
	return out;
}

/* ------------------------------------------------------------------------------------------------
 */
void glob_destroy(Glob glob)
{
	globfree((glob_t*)glob.handle);
	mem.free(glob.handle);
}

/* ------------------------------------------------------------------------------------------------
 */
b8 is_file(const char* path)
{
	struct statx s;

	if (statx(AT_FDCWD, path, 0, STATX_MODE, &s))
	{
		perror("statx");
		error_nopath("is_file(): unable to stat file at path ", path);
		exit(1);
	}

	return S_ISREG(s.stx_mode);
}

/* ------------------------------------------------------------------------------------------------
 */
b8 is_dir(const char* path)
{
	struct statx s;

	if (statx(AT_FDCWD, path, 0, STATX_MODE, &s))
	{
		perror("statx");
		error_nopath("is_file(): unable to stat file at path ", path);
		exit(1);
	}

	return S_ISDIR(s.stx_mode);
}

/* ------------------------------------------------------------------------------------------------
 */
s64 modtime(const char* path)
{
	struct statx s;

	if (statx(AT_FDCWD, path, 0, STATX_MTIME, &s))
	{
		perror("statx");
		error_nopath("is_file(): unable to stat file at path ", path);
		exit(1);
	}

	return s.stx_mtime.tv_sec;
}

}
