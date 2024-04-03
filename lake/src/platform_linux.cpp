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
#include "glob.h"

#include "sys/types.h"
#include "sys/wait.h"

#include "list.h"

struct LinuxProcess
{
	pid_t pid;
	int fd_out;
	int fd_err;
};

typedef Pool<LinuxProcess> ProcessPool;

// I don't care for this being global, but whatever for now.
ProcessPool proc_pool = ProcessPool::create();

extern "C"
{

/* ------------------------------------------------------------------------------------------------ read_file
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

/* ------------------------------------------------------------------------------------------------ dir_iter
 */
DirIter dir_iter(const char* path)
{
	return opendir(path);
}

/* ------------------------------------------------------------------------------------------------ dir_next
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

/* ------------------------------------------------------------------------------------------------ glob_create
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

/* ------------------------------------------------------------------------------------------------ glob_destroy
 */
void glob_destroy(Glob glob)
{
	globfree((glob_t*)glob.handle);
	mem.free(glob.handle);
}

/* ------------------------------------------------------------------------------------------------ path_exists
 */
b8 path_exists(str path)
{
	return access((char*)path.s, F_OK) == 0;
}

/* ------------------------------------------------------------------------------------------------ is_file
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

/* ------------------------------------------------------------------------------------------------ is_dir
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

/* ------------------------------------------------------------------------------------------------ modtime
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

/* ------------------------------------------------------------------------------------------------ checkerr
 */
void checkerr(int r, const char* name)
{
	if (r == -1)
	{
		perror(name);
		exit(1);
	}
}

/* ------------------------------------------------------------------------------------------------ process_spawn
 */
Process process_spawn(char** args)
{
	int stdout_pipes[2];
	int stderr_pipes[2];

	if (pipe(stdout_pipes) == -1)
	{
		perror("pipe");
		exit(1);
	}

	if (pipe(stderr_pipes) == -1)
	{
		perror("pipe");
		exit(1);
	}

	if (pid_t pid = fork())
	{
		if (pid == -1)
		{
			perror("fork");
			exit(1);
		}
		// parent branch
		
		// close the write end of each pipe since the parent will
		// not be writing to the child
		close(stdout_pipes[1]);
		close(stderr_pipes[1]);

		// duplicate the read pipes 
		int stdout_fd = dup(stdout_pipes[0]);
		int stderr_fd = dup(stderr_pipes[0]);

		// set the pipe's flags to be non-blocking on reads
		// so we can perform async process checking
		fcntl(stdout_fd, F_SETFL, fcntl(stdout_fd, F_GETFL, 0) | O_NONBLOCK);
		fcntl(stderr_fd, F_SETFL, fcntl(stderr_fd, F_GETFL, 0) | O_NONBLOCK);

		LinuxProcess* proc = proc_pool.add();
		proc->pid = pid;
		proc->fd_err = stdout_fd;
		proc->fd_out = stderr_fd;

		return proc;
	}
	else
	{ 
		// close reading pipes because we will not be reading from the parent
		close(stdout_pipes[0]);
		close(stderr_pipes[0]);

		// replace our stdout and stderr file descriptors with the 
		// pipes created by the parent
		dup2(stdout_pipes[1], 1);
		dup2(stderr_pipes[1], 2);

		if (execvp(args[0], args) == -1)
		{
			perror("execvp");
			exit(1);
		}
	}

	return nullptr;
}

/* ------------------------------------------------------------------------------------------------ process_poll
 */
PollReturn process_poll(
		Process proc_handle,
		void* out_dest, int out_suggested_bytes_read,
		void* err_dest, int err_suggested_bytes_read,
		void (*on_close)(int))
{
	LinuxProcess* proc = (LinuxProcess*)proc_handle;

	PollReturn out = {};

	int status;
	int r = waitpid(proc->pid, &status, WNOHANG);
	checkerr(r, "waitpid");

	if (r)
	{
		if (WIFEXITED(status))
		{
			if (on_close)
				on_close(WEXITSTATUS(status));

			proc_pool.remove(proc);
			return out;
		}
	}

	if (out_dest && out_suggested_bytes_read)
	{
		r = read(proc->fd_out, out_dest, out_suggested_bytes_read);
		if (errno == EWOULDBLOCK)
		{
			errno = 0;
			out.stdout_bytes_written = 0;
		}
		else
		{
			if (r)  
				printv("out read ", r, " bytes\n");
			checkerr(r, "read");
			out.stdout_bytes_written = r;
		}
	}

	if (err_dest && err_suggested_bytes_read)
	{
		r = read(proc->fd_err, err_dest, err_suggested_bytes_read);
		if (errno == EWOULDBLOCK)
		{
			errno = 0;
			out.stderr_bytes_written = 0;
		}
		else
		{
			if (r)
				printv("err read ", r, " bytes\n");
			checkerr(r, "read");
			out.stderr_bytes_written = r;
		}
	}

	return out;
}

}
