#if IRO_LINUX

#include "Platform.h"

#include "Logger.h"
#include "Unicode.h"
#include "containers/StackArray.h"
#include "containers/Pool.h"

#include "pty.h"
#include "utmp.h"
#include "poll.h"
#include "fcntl.h"
#include "utime.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include "dirent.h"
#include "string.h"
#include "termios.h"
#include "sys/uio.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "sys/ptrace.h"
#include "sys/sendfile.h"
#include "sys/mman.h"

#include "valgrind/callgrind.h"

#include "ctime"

namespace iro::platform
{

static iro::Logger logger =
  iro::Logger::create("platform.linux"_str, iro::Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
template<typename... T>
b8 reportErrno(T... args)
{
  ERROR(args..., "\n");
  ERROR("errno: ", strerror(errno), "\n");
  errno = 0;
  return false;
}

/* ----------------------------------------------------------------------------
 */
void sleep(TimeSpan time)
{
  usleep((int)time.toMicroseconds());
}

/* ----------------------------------------------------------------------------
 */
b8 open(fs::File::Handle* out_handle, String path, fs::OpenFlags flags)
{
  using enum fs::OpenFlag;

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

  if (flags.testAll<Read, Write>())
    oflags |= O_RDWR;
  else if (flags.test(Write))
    oflags |= O_WRONLY;
  else if (flags.test(Read))
    oflags |= O_RDONLY;
  else
  {
    ERROR("failed to open file at path '", path,
        "': one of OpenFlag::Read/Write was not given\n");
    return false;
  }

  int mode = S_IRWXU;

  int r = ::open((char*)path.ptr, oflags, mode);

  // TODO(sushi) handle non-null-terminated strings
  if (r == -1)
    return reportErrno( "failed to open file at path '", path, "'");

  *out_handle = (u64)r;

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 close(fs::File::Handle handle)
{
  if (::close((s64)handle) == -1)
    return reportErrno( "failed to close file with handle ", handle);
  return true;
}

/* ----------------------------------------------------------------------------
 */
s64 read(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 is_pty)
{
  int r = ::read((s64)handle, buffer.ptr, buffer.len);

  if (r == -1)
  {
    if ((errno != EAGAIN || !non_blocking) &&
        (errno != EIO || !is_pty))
      return reportErrno( "failed to read from file with handle ", handle);
    else
      r = errno = 0;
  }
  return r;
}

/* ----------------------------------------------------------------------------
 */
s64 write(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 is_pty)
{
  int r = ::write((s64)handle, buffer.ptr, buffer.len);

  if (r == -1)
  {
    if ((errno != EAGAIN || !non_blocking) &&
        (errno != EIO || !is_pty))
      return reportErrno( "failed to write to file with handle ", handle);
    else
      r = errno = 0;
  }
  return r;
}

/* ----------------------------------------------------------------------------
 */
b8 poll(fs::File::Handle handle, fs::PollEventFlags* flags)
{
  struct pollfd pfd = {};
  pfd.fd = (s64)handle;

#define flagmap(x, y) \
    if (flags->test(fs::PollEvent::x)) pfd.events |= y;

  flagmap(In,  POLLIN);
  flagmap(Out, POLLOUT);

#undef flagmap

  flags->clear();

  if (-1 == poll(&pfd, 1, 0))
    return reportErrno("failed to poll process with handle ", handle);

#define flagmap(x, y) \
    if (pfd.revents & x) flags->set(fs::PollEvent::y);

  flagmap(POLLIN,   In);
  flagmap(POLLOUT,  Out);
  flagmap(POLLERR,  Err);
  flagmap(POLLNVAL, Invalid);
  flagmap(POLLHUP,  HangUp);

#undef flagmap

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 isatty(fs::File::Handle handle)
{
  return ::isatty((s64)handle);
}

/* ----------------------------------------------------------------------------
 */
b8 setNonBlocking(fs::File::Handle handle)
{
  int oldflags = fcntl( (s64)handle, F_GETFL, 0);
  if (-1 == fcntl(
        (s64)handle,
        F_SETFL,
        oldflags | O_NONBLOCK))
    return reportErrno(
        "failed to set file with handle ", handle, " as non-blocking");
  return true;
}

/* ----------------------------------------------------------------------------
 */
Timespec clock_realtime()
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return {(u64)ts.tv_sec, (u64)ts.tv_nsec};
}

/* ----------------------------------------------------------------------------
 */
Timespec clock_monotonic()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return {(u64)ts.tv_sec, (u64)ts.tv_nsec};
}

/* ----------------------------------------------------------------------------
 */
b8 opendir(fs::Dir::Handle* out_handle, String path)
{
  assert(out_handle);

  DIR* dir = ::opendir((char*)path.ptr);

  if (!dir)
    return reportErrno( "failed to open dir at path '", path, "'");

  *out_handle = dir;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 opendir(fs::Dir::Handle* out_handle, fs::File::Handle file_handle)
{
  assert(out_handle && file_handle);

  DIR* dir = fdopendir((s64)file_handle);

  if (!dir)
    return reportErrno( "failed to open dir from file handle ", file_handle);

  *out_handle = dir;
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 closedir(fs::Dir::Handle handle)
{
  int r = ::closedir((DIR*)handle);

  if (r == -1)
    return reportErrno( "failed to close dir with handle ", handle);

  return true;
}

/* ----------------------------------------------------------------------------
 */
s64 readdir(fs::Dir::Handle handle, Bytes buffer)
{
  assert(handle && buffer.ptr && buffer.len);

  errno = 0;
  struct dirent* de = ::readdir((DIR*)handle);

  if (!de)
  {
    if (errno == 0)
      return 0;
    reportErrno(
      "failed to read directory stream with handle ", handle);
    return -1;
  }

  s64 len = strlen(de->d_name);
  if (len > buffer.len)
  {
    ERROR("buffer given to readdir() is too small. File name '", de->d_name,
        "' is ", len, " bytes long, but given buffer is ", buffer.len,
        " bytes long\n");
    return -1;
  }

  mem::copy(buffer.ptr, de->d_name, len);
  return len;
}

/* ----------------------------------------------------------------------------
 */
b8 stdinHasData()
{
  struct pollfd fds;
  fds.fd = 0;
  fds.events = POLLIN;

  if (-1 == poll(&fds, 1, 0))
    return reportErrno("failed to poll stdin");

  return 0 != (fds.revents & POLLIN);
}

/* ----------------------------------------------------------------------------
 */
b8 stat(fs::FileInfo* out_info, String path)
{
  assert(out_info && path.ptr && path.len);

  struct statx s;

  int mode =
    STATX_MODE |
    STATX_SIZE |
    STATX_ATIME |
    STATX_BTIME |
    STATX_CTIME |
    STATX_MTIME;

  if (::statx(AT_FDCWD, (char*)path.ptr, 0, mode, &s) == -1)
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

  auto timespecToTimePoint =
    [](statx_timestamp ts)
      { return TimePoint{u64(ts.tv_sec), u64(ts.tv_nsec)}; };

  out_info->birth_time = timespecToTimePoint(s.stx_btime);
  out_info->last_access_time = timespecToTimePoint(s.stx_atime);
  out_info->last_modified_time = timespecToTimePoint(s.stx_mtime);
  out_info->last_status_change_time = timespecToTimePoint(s.stx_ctime);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 fileExists(String path)
{
  return access((char*)path.ptr, F_OK) == 0;
}

/* ----------------------------------------------------------------------------
 *  This implementation will not work on Linux versions < 2.6.33 but I'm not
 *  sure if that'll ever be a problem. sendfile is more efficient than simply
 *  read/writing because it doesnt require moving mem around in userspace.
 */
b8 copyFile(String dst, String src)
{
  assert(notnil(dst) and notnil(src) and
         not dst.isEmpty() and not src.isEmpty());

  using namespace fs;

  auto srcf = File::from(src, OpenFlag::Read);
  if (isnil(srcf))
  {
    ERROR("copyFile(): failed to open src file '", src, "'\n");
    return false;
  }
  defer { srcf.close(); };

  auto dstf = File::from(dst,
        OpenFlag::Write
      | OpenFlag::Truncate
      | OpenFlag::Create);
  if (isnil(dstf))
  {
    ERROR("copyFile(): failed to open dst file '", dst, "'\n");
    return false;
  }
  defer { dstf.close(); };

  off_t bytes_copied = 1;
  auto srcinfo = srcf.getInfo();

  while (bytes_copied > 0)
  {
    bytes_copied =
      sendfile((s64)dstf.handle, (s64)srcf.handle, nullptr, srcinfo.byte_size);
    if (bytes_copied < 0)
      return reportErrno( "failed to copy '", src, "' to '", dst, "'");
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 unlinkFile(String path)
{
  assert(notnil(path) and not path.isEmpty());

  if (-1 == unlink((char*)path.ptr))
    return reportErrno( "failed to unlink file at path '", path, "'");
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 removeDir(String path)
{
  assert(notnil(path) and not path.isEmpty());

  if (-1 == rmdir((char*)path.ptr))
    return reportErrno( "failed to remove directory at path '", path, "'");
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 makeDir(String path, b8 make_parents)
{
  assert(notnil(path));

  if (path.len == 0)
    return true;

  // early out if path already exists
  struct stat __unused__ = {};
  if (::stat((char*)path.ptr, &__unused__) != -1)
    return true;

  if (!make_parents)
  {
    if (-1 == mkdir((char*)path.ptr, 0777))
      return reportErrno( "failed to make directory at path '", path, "'");
    return true;
  }

  // TODO(sushi) if this is ever hit make it use dynamic alloc to handle it
  if (path.len >= 4096)
  {
    ERROR("path given to makeDir is too long! "
        "The limit is 4096 characters. Path is ", path, "\n");
    return false;
  }

  u8 path_buffer[4096] = {};

  for (s32 i = 0; i < path.len; i++)
  {
    u8 c = path_buffer[i] = path.ptr[i];

    if (c == '/' || i == path.len - 1)
    {
      if (-1 == mkdir((char*)path_buffer, 0777))
      {
        if (errno != EEXIST)
          return reportErrno( "failed to make directory at path '", path, "'");
        errno = 0;
      }
    }
  }

  return true;
}

/* ============================================================================
 */
struct ProcessLinux
{
  // TODO(sushi) considering this is all we store here and all we need to store
  //             on win32 is a HANDLE we should probably just store it on
  //             the Process and avoid this static pool stuff.
  pid_t pid;
  int stdin;
  int stdout;
  int stderr;
};

using ProcessPool = StaticPool<ProcessLinux>;
static ProcessPool g_process_pool;

/* ----------------------------------------------------------------------------
 */
b8 processSpawn(
    Process::Handle* out_handle,
    String file,
    Slice<String> args,
    String cwd,
    b8 non_blocking,
    b8 redirect_err_to_out)
{
  assert(out_handle);

  ProcessLinux* p_proc = g_process_pool.add();
  if (p_proc == nullptr)
    return ERROR("failed to allocate a ProcessLinux\n");

  // TODO(sushi) replace this with some container thats a stack buffer
  //             up to a point then dynamically allocates
  Array<char*> argsc = Array<char*>::create(args.len);
  argsc.push((char*)file.ptr);
  for (s32 i = 0; i < args.len; i++)
  {
    argsc.push((char*)args[i].ptr);
  }
  argsc.push(nullptr);

  defer { argsc.destroy(); };

  int stdin_pipes[2];
  int stdout_pipes[2];
  int stderr_pipes[2];

  if (-1 == pipe(stdout_pipes))
    return reportErrno("failed to open pipes for process ", file);

  if (-1 == pipe(stdin_pipes))
    return reportErrno("failed to open pipes for process ", file);

  if (!redirect_err_to_out)
  {
    if (-1 == pipe(stderr_pipes))
      return reportErrno("failed to open pipes for process ", file);
  }

  if (pid_t pid = fork())
  {
    // parent branch

    if (pid == -1)
      return reportErrno("failed to fork process");

    ::close(stdout_pipes[1]);
    if (!redirect_err_to_out)
      ::close(stderr_pipes[1]);
    ::close(stdin_pipes[0]);

    if (non_blocking)
    {
      int oldflags = fcntl(stdout_pipes[0], F_GETFL, 0);
      if (-1 == fcntl(
            stdout_pipes[0],
            F_SETFL,
            oldflags | O_NONBLOCK))
        return reportErrno("failed to set child pipe as non-blocking");

      oldflags = fcntl(stdin_pipes[1], F_GETFL, 0);
      if (-1 == fcntl(
            stdin_pipes[1],
            F_SETFL,
            oldflags | O_NONBLOCK))
        return reportErrno("failed to set child pipe as non-blocking");
    }

    p_proc->pid = pid;
    p_proc->stderr = redirect_err_to_out? stdout_pipes[0] : stderr_pipes[0];
    p_proc->stdout = stdout_pipes[0];
    p_proc->stdin = stdin_pipes[1];
    *out_handle = p_proc;
  }
  else
  {
    // child branch

    CALLGRIND_ZERO_STATS;

    if (notnil(cwd))
    {
      // this miiight cause problems if the string were given is destroyed
      // before we reach this point somehow but hopefully that is very
      // unlikely
      chdir(cwd);
    }

    ::close(stdout_pipes[0]);
    dup2(stdout_pipes[1], 1);

    if (!redirect_err_to_out)
    {
      ::close(stderr_pipes[0]);
      dup2(stderr_pipes[1], 2);
    }
    else
      dup2(stdout_pipes[1], 2);

    ::close(stdin_pipes[1]);
    dup2(stdin_pipes[0], 0);

    if (-1 == execvp(argsc.arr[0], argsc.arr))
    {
      reportErrno(
        "execvp failed to replace child process with file '", file, "'");
      exit(1);
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processHasOutput(Process::Handle h_process)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processHasOutput passed a null process handle\n");

  struct pollfd pfd = {};
  pfd.fd = (s64)p_proc->stdout;

  pfd.events = POLLIN;

  if (-1 == poll(&pfd, 1, 0))
    return reportErrno("failed to poll process with handle ", h_process);

  if (pfd.revents & POLLIN)
    return true;

  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 processHasErrOutput(Process::Handle h_process)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processHasOutput passed a null process handle\n");

  struct pollfd pfd = {};
  pfd.fd = (s64)p_proc->stderr;

  pfd.events = POLLIN;

  if (-1 == poll(&pfd, 1, 0))
    return reportErrno("failed to poll process with handle ", h_process);

  if (pfd.revents & POLLIN)
    return true;

  return false;
}

/* ----------------------------------------------------------------------------
 */
u64 processRead(Process::Handle h_process, Bytes buffer)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processRead passed a null process handle\n");

  int r = ::read(p_proc->stdout, buffer.ptr, buffer.len);

  if (r == -1)
  {
    if (errno == EAGAIN)
      r = errno = 0;
    else
      return reportErrno("failed to read from process\n");
  }

  return r;
}

/* ----------------------------------------------------------------------------
 */
u64 processReadStdErr(Process::Handle h_process, Bytes buffer)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processReadStdErr passed a null process handle\n");

  int r = ::read(p_proc->stderr, buffer.ptr, buffer.len);

  if (r == -1)
  {
    if (errno == EAGAIN)
      r = errno = 0;
    else
      return reportErrno("failed to read stderr from process\n");
  }

  return r;
}

/* ----------------------------------------------------------------------------
 */
u64 processWrite(Process::Handle h_process, Bytes buffer)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processWrite passed a null process handle\n");

  int r = ::write(p_proc->stdin, buffer.ptr, buffer.len);

  if (r == -1)
  {
    if (errno == EAGAIN)
      r = errno = 0;
    else
      return reportErrno("failed to write to process\n");
  }

  return r;
}

/* ----------------------------------------------------------------------------
 */
b8 processHasExited(Process::Handle h_process, s32* out_exit_code)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processHasExited passed a null process handle\n");

  int status = 0;
  int r = waitpid(p_proc->pid, &status, WNOHANG);
  if (-1 == r)
  {
    if (errno == ECHILD)
    {
      // The child mist have exited.
      errno = 0;
      return true;
    }
    reportErrno(
      "waitpid failed on process with handle ", h_process);
    return true;
  }

  if (r)
  {
    if (WIFEXITED(status))
    {
      if (out_exit_code)
        *out_exit_code = WEXITSTATUS(status);
      return true;
    }
  }

  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 processClose(Process::Handle h_process)
{
  auto* p_proc = (ProcessLinux*)h_process;
  if (p_proc == nullptr)
    return ERROR("processClose passed a null process handle\n");

  if (!processHasExited(h_process, nullptr))
  {
    if (-1 == kill(p_proc->pid, SIGKILL))
      return reportErrno("failed to close process ", h_process);
  }

  g_process_pool.remove(p_proc);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 realpath(fs::Path* path)
{
  StackArray<u8, PATH_MAX> buf;

  if (::realpath((char*)path->buffer.ptr, (char*)buf.arr) == nullptr)
    return reportErrno( "failed to canonicalize path ", *path, "'");

  buf.len = strlen((char*)buf.arr);

  path->buffer.clear();
  path->buffer.reserve(buf.len);
  path->buffer.commit(buf.len);
  mem::copy(path->buffer.ptr, buf.arr, buf.len);

  return true;
}

/* ----------------------------------------------------------------------------
 */
fs::Path cwd(mem::Allocator* allocator)
{
  fs::Path path;

  if (!path.init(allocator))
    return nil;

  const u64 maxlen = PATH_MAX * 4;
  u64 bufferlen = PATH_MAX / 16;

  path.reserve(bufferlen);

  for (;;)
  {
    if (!getcwd((char*)path.buffer.ptr, bufferlen))
    {
      if (errno != ERANGE)
      {
        path.destroy();
        reportErrno("failed to get cwd");
        return nil;
      }

      errno = 0;
      bufferlen *= 2;

      if (bufferlen > maxlen)
      {
        ERROR("the length of cwd is greater than the max length cwd() is "
              "willing to allocate (", maxlen, ")!\n");
        path.destroy();
        return nil;
      }

      path.reserve(bufferlen);
      continue;
    }

    path.commit(strlen((char*)path.buffer.ptr));
    return path;
  }
}

/* ----------------------------------------------------------------------------
 */
b8 chdir(String path)
{
  if (-1 == ::chdir((char*)path.ptr))
    return reportErrno(
        "failed to chdir into path '", path, "'");
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 chdir(fs::Dir::Handle dir)
{
  if (-1 == fchdir((int)dirfd((DIR*)dir)))
    return reportErrno(
        "failed to chdir into directory with handle ",
        (void*)dir);
  return true;

}

/* ----------------------------------------------------------------------------
 */
TermSettings termSetNonCanonical(mem::Allocator* allocator)
{
  struct termios* mode =
    (struct termios*)allocator->allocate(sizeof(struct termios));

  tcgetattr(STDIN_FILENO, mode);

  struct termios newmode = *mode;
  newmode.c_lflag &= ~(ICANON|ECHO);
  newmode.c_cc[VMIN] = 1; // min characters needed for non-canonical read
  newmode.c_cc[VTIME] = 0; // timeout for read in non-canonical
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &newmode);
  return (TermSettings)mode;
}

/* ----------------------------------------------------------------------------
 */
void termRestoreSettings(TermSettings settings, mem::Allocator* allocator)
{
  tcsetattr(STDIN_FILENO, TCSANOW, (struct termios*)settings);
  allocator->free(settings);
}

/* ----------------------------------------------------------------------------
 */
b8 touchFile(String path)
{
  struct stat info;
  if (-1 == stat((char*)path.ptr, &info))
    return reportErrno("failed to touch path '", path, "'");

  struct utimbuf newtimes;
  newtimes.modtime = time(0);
  newtimes.actime = info.st_atime;

  if (-1 == utime((char*)path.ptr, &newtimes))
    return reportErrno("failed to set modtime of path '", path, "'");

  return true;
}

/* ----------------------------------------------------------------------------
 */
s32 getEnvVar(String name, Bytes buffer)
{
  char* val = getenv((char*)name.ptr);
  if (val == nullptr)
    return -1;

  s32 bytes_needed = strlen(val);
  if (isnil(buffer))
    return bytes_needed;

  if (buffer.len < bytes_needed)
  {
    ERROR("provided buffer is not large enough to store env var value\n");
    return -1;
  }

  mem::copy(buffer.ptr, val, bytes_needed+1);
  return bytes_needed;
}

/* ----------------------------------------------------------------------------
 */
b8 setEnvVar(String name, String value)
{
  if (-1 == setenv((char*)name.ptr, (char*)value.ptr, true))
    return reportErrno("failed to set envvar ", name);
  return true;
}

/* ----------------------------------------------------------------------------
 */
void exit(int status)
{
  ::exit(status);
}

/* ----------------------------------------------------------------------------
 */
b8 isRunningUnderDebugger()
{
  if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0)
    return true;
  else
    ptrace(PTRACE_DETACH, 0, 1, 0);

  return false;
}

/* ----------------------------------------------------------------------------
 */
void debugBreak()
{
  __builtin_debugtrap();
}

/* ----------------------------------------------------------------------------
 */
u64 getPid()
{
  return getpid();
}

/* ----------------------------------------------------------------------------
 */
u16 byteSwap(u16 x)
{
  return __builtin_bswap16(x);
}

u32 byteSwap(u32 x)
{
  return __builtin_bswap32(x);
}

u64 byteSwap(u64 x)
{
  return __builtin_bswap64(x);
}

/* ----------------------------------------------------------------------------
 */
void* reserveMemory(u64 size)
{
  void* result = mmap(nullptr, size, PROT_NONE,
    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED)
  {
    reportErrno("failed to reserve memory");
    result = nullptr;
  }
  return result;
}

/* ----------------------------------------------------------------------------
 */
b8 commitMemory(void* ptr, u64 size)
{
  if (-1 == mprotect(ptr, size, PROT_READ | PROT_WRITE))
    return reportErrno("failed to commit memory");
  return true;
}

/* ----------------------------------------------------------------------------
 */
void* reserveLargeMemory(u64 size)
{
  void* result = mmap(nullptr, size, PROT_NONE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (result == MAP_FAILED)
  {
    reportErrno("failed to reserve large memory");
    result = nullptr;
  }
  return result;
}

/* ----------------------------------------------------------------------------
 */
b8 commitLargeMemory(void* ptr, u64 size)
{
  if (-1 == mprotect(ptr, size, PROT_READ | PROT_WRITE))
    return reportErrno("failed to commit memory");
  return true;
}

/* ----------------------------------------------------------------------------
 */
void decommitMemory(void* ptr, u64 size)
{
  if (-1 == madvise(ptr, size, MADV_DONTNEED))
  {
    reportErrno("failed to decommit memory");
    return;
  }
  if (-1 == mprotect(ptr, size, PROT_NONE))
  {
    reportErrno("failed to decommit memory");
    return;
  }
}

/* ----------------------------------------------------------------------------
 */
void releaseMemory(void* ptr, u64 size)
{
  if (-1 == munmap(ptr, size))
    reportErrno("failed to release memory");
}

}

#endif // #if IRO_LINUX
