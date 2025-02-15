#if IRO_WIN32

#define WIN32_LEAN_AND_MEAN
#include "winsock2.h"
#include "windows.h"
#undef ERROR
#undef max
#undef min

#include "Platform.h"
#include "Platform_Win32.h"

#include "Logger.h"
#include "Unicode.h"
#include "memory/Allocator.h"
#include "io/IO.h"
#include "containers/Pool.h"

#include "climits"
#include "cwchar"

namespace iro::platform
{

static iro::Logger logger =
  iro::Logger::create("platform.win32"_str, iro::Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
static thread_local LPSTR win32_error_msg = nullptr;
static String makeWin32ErrorMsg(DWORD error)
{
	DWORD win32_error_msg_len =
    FormatMessageA(
          FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
        (LPSTR)&win32_error_msg,
        0,
        NULL);

  return String::from((u8*)win32_error_msg, (u64)win32_error_msg_len);
}

static void cleanupWin32ErrorMsg()
{
  LocalFree(win32_error_msg);
}

/* ----------------------------------------------------------------------------
 */
template<typename... Args>
static b8 win32Err(Args... args)
{
  return ERROR(args..., ": ", Win32ErrMsg());
}

/* ----------------------------------------------------------------------------
 *  Normalizes a path given by Win32 in place and returns a new String
 *  representing it. This replaces \\ with / and removes the UNC prefix if
 *  it exists. This does not canonicalize the path, eg. it does not try to
 *  remove . and .. directories from it.
 */
static String normalizePath(String string)
{
  if (string.len > 4 && matchSeq(string.ptr, '\\', '\\', '?', '\\'))
  {
    mem::move(string.ptr, string.ptr + 4, string.len - 4);
    string.len -= 4;
  }

  for (s32 i = 0; i < string.len; ++i)
  {
    if (string.ptr[i] == '\\')
      string.ptr[i] = '/';
  }

  return string;
}

/* ----------------------------------------------------------------------------
 */
static s32 stringToWideChar(
    const char* ptr,
    int len,
    wchar_t* outptr,
    int outlen,
    b8 null_terminate = true)
{
  int bytes_written =
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        (LPCCH)ptr,
        len,
        outptr,
        (null_terminate? outlen-1 : outlen));

  if (bytes_written == 0)
    return (win32Err("failed to convert utf8 to wide chars"), 0);

  if (null_terminate)
    outptr[bytes_written] = L'\0';

  return bytes_written;
}

/* ----------------------------------------------------------------------------
 */
static s32 stringToWideChar(
    String str,
    wchar_t* outptr,
    int outlen,
    b8 null_terminate = true)
{
  return stringToWideChar(
      (char*)str.ptr,
      str.len,
      outptr,
      outlen,
      null_terminate);
}

/* ----------------------------------------------------------------------------
 */
static b8 isPathDeliminator(wchar_t c)
{
  return c == L'/' || c == L'\\';
}

// NOTE: Ideally we would use less than 32771, but GetFinalPathNameByHandleW
// can not be called to determine the output string length before filling
// the string buffer. So, we provide it a buffer that can hold the max
// WideChar path length 32767, plus 4 for the "\\?\" prefix.
//
// TODO(sushi) this NOTE is incorrect as GetFinalPathNameByHandleW returns
//             the required space when the provided buffer is too small (at
//             least the docs say so, who knows). Ideally this global be
//             removed from the internals and we instead move towards mostly
//             requiring the user to provide a buffer of sufficient size or
//             just using a function local buffer). The Linux platform layer
//             also needs to move more towards this style as well iirc.
#define win32_str_capacity 32771
thread_local static wchar_t win32_str[win32_str_capacity + 1] = {0};
thread_local const static char* win32_make_path_error = nullptr;

/* ----------------------------------------------------------------------------
 */
static wchar_t* makeWin32Path(
    String input,
    u64 extra_bytes = 0,
    u64* output_len = nullptr,
    b8 force_allocate = false)
{
  wchar_t* output = win32_str;

  char* null_terminated_input = (char*)_alloca((size_t)input.len + 1);
  input.nullTerminate((u8*)null_terminated_input, input.len+1);

  DWORD full_path_length =
    GetFullPathNameA((LPCSTR)null_terminated_input, 0, NULL, NULL);
  char* full_path = (char*)_alloca((size_t)full_path_length + 1);
  full_path_length =
    GetFullPathNameA((LPCSTR)null_terminated_input, full_path_length + 1,
    full_path, NULL);

  if (full_path_length == 0)
  {
    win32_make_path_error = "insufficient memory when converting from UTF-8"
        " to WideChar\n";
    return nullptr;
  }

  u64 bytes = full_path_length + extra_bytes;
  if (bytes == 0)
  {
    output[0] = L'\0';
    return output;
  }

  // TODO(sushi) this is necessary because GetFullPathNameA might give us a
  //             path that already has a \\?\ prefix. This should be done more
  //             cleanly later but I'm just trying to get this stuff to work
  //             rn.
  b8 has_prefix =
    full_path_length > 4 &&
    (full_path[0] == '\\' && full_path[1] == '\\' &&
     full_path[2] == '?'  && full_path[3] == '\\');

  bytes += (has_prefix? 1 : 5); // "\\?\" + null-terminator
  if (bytes > win32_str_capacity || force_allocate)
  {
    output = (wchar_t*)mem::stl_allocator.allocate(bytes);
    if (output == nullptr)
    {
      win32_make_path_error = "insufficient memory when converting from UTF-8"
        " to WideChar\n";
      return nullptr;
    }
  }

  if (!has_prefix)
  {
    output[0] = L'\\';
    output[1] = L'\\';
    output[2] = L'?';
    output[3] = L'\\';
  }

  assert(full_path_length <= INT_MAX);
  assert(bytes <= INT_MAX);
  int converted_bytes =
    stringToWideChar(
      full_path,
      full_path_length,
      output + (has_prefix? 0 : 4),
      bytes - (has_prefix? 1 : 5),
      false);

  if (converted_bytes == 0)
  {
    ERROR("failed to make win32 path: ", makeWin32ErrorMsg(GetLastError()));
    return nullptr;
  }

  if (output_len != nullptr)
    *output_len = converted_bytes + (has_prefix? 4 : 0);

  output[converted_bytes+(has_prefix? 0 : 4)] = L'\0';
  //NOTICE((wchar_t*)output, "\n");
  return output;
}

/* ----------------------------------------------------------------------------
 */
static TimePoint filetimeToTimePoint(LARGE_INTEGER win32_time)
{
  u64 time = (u64)win32_time.QuadPart;
  time -= 116444736000000000ULL; // Jan 1, 1601 to Jan 1, 1970
  return TimePoint{time / 10000000ULL, time % 10000000ULL * 100};
}

/* ----------------------------------------------------------------------------
 */
void sleep(TimeSpan time)
{
  Sleep((DWORD)time.toMilliseconds());
}

/* ----------------------------------------------------------------------------
 */
b8 open(fs::File::Handle* out_handle, String path, fs::OpenFlags flags)
{
  using enum fs::OpenFlag;
  assert(out_handle != nullptr);

  const u64 buflen = 4096;
  wchar_t wpath[buflen];

  if (0 == stringToWideChar(path, wpath, buflen))
    return false;

  DWORD access;
  if (flags.testAll<Read, Write>())
    access = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
  else if (flags.test(Write))
    access = FILE_GENERIC_WRITE;
  else if (flags.test(Read))
    access = FILE_GENERIC_READ;
  else
  {
    ERROR("failed to open file at path '", path,
        "': one of OpenFlag::Read/Write was not given\n");
    return false;
  }

  if (flags.test(Append))
  {
    access &= ~FILE_WRITE_DATA;
    access |= FILE_APPEND_DATA;
  }

  int const filtered_flags = (int)flags.flags
    & (int)(Exclusive | Create | Truncate).flags;
  DWORD disposition;
  switch (filtered_flags)
  {
    case 0:
    case (int)(Exclusive):
      disposition = OPEN_EXISTING;
      break;
    case (int)(Create):
      disposition = OPEN_ALWAYS;
      break;
    case (int)(Create | Exclusive).flags:
    case (int)(Create | Exclusive | Truncate).flags:
      disposition = CREATE_NEW;
      break;
    case (int)(Truncate):
    case (int)(Truncate | Exclusive).flags:
      disposition = TRUNCATE_EXISTING;
      break;
    case (int)(Create | Truncate).flags:
      disposition = CREATE_ALWAYS;
      break;
    default:
      assert(!"should not be possible");
      return false;
  }

  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  DWORD attributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS |
    FILE_FLAG_POSIX_SEMANTICS;

  if (flags.test(NoBlock))
  {
    attributes |= FILE_FLAG_OVERLAPPED;
  }

  HANDLE handle = CreateFileW(wpath, access, share, NULL, disposition,
    attributes, NULL);
  if (handle == INVALID_HANDLE_VALUE)
  {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_EXISTS && flags.test(Create) &&
      !flags.test(Exclusive))
    {
      ERROR("failed to open file at path '", path,
        "': OpenFlag::Create was specified on an existing directory\n");
    }
    else
    {
      ERROR("failed to open file at path '", path, "': ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
    }
    return false;
  }

  *out_handle = (fs::File::Handle)handle;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 close(fs::File::Handle handle)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);

  if (CloseHandle((HANDLE)handle))
    return true;

  ERROR("failed to close file with handle ", handle, ": ",
    makeWin32ErrorMsg(GetLastError()), "\n");
  cleanupWin32ErrorMsg();
  return false;
}

/* ----------------------------------------------------------------------------
 */
s64 read(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 /*is_pty*/)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  assert(buffer.ptr != nullptr || buffer.len == 0);

  DWORD bytes_read = 0;
  defer { TRACE("read ", (u32)bytes_read, " bytes\n"); };

  if (non_blocking)
  {
    TRACE("non-blocking read from ", (u64)handle, "\n");
    defer { TRACE("read ", (u64)bytes_read, " bytes\n"); };

    // Perform an 'overlapped' read, which requires an Event.
    // TODO(sushi) creating and closing an Event everytime we do this might not
    //             be very good, but the docs mention that it is safer to use
    //             separate Event objects for each overlapped operation:
    // https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-and-overlapped-input-and-output
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(0,1,0,0);
    if (overlapped.hEvent == NULL)
      return win32Err("failed to create Event for non blocking read");
    defer { CloseHandle(overlapped.hEvent); };

    if (!ReadFile(
          (HANDLE)handle,
          buffer.ptr,
          buffer.len,
          &bytes_read,
          &overlapped))
    {
      if (GetLastError() == ERROR_BROKEN_PIPE)
        // The process has probably closed and the pipe has no more information
        // to read.
        return 0;

      if (GetLastError() != ERROR_IO_PENDING)
        return win32Err("non blocking read from ", handle, " failed");

      // Cancel the pending I/O operation to avoid blocking on
      // GetOverlappedResult
      if (!CancelIoEx((HANDLE)handle, &overlapped)
          && GetLastError() != ERROR_NOT_FOUND)
        return win32Err("failed to cancel I/O operations for ", handle);

      // Get any data that we might have recieved before cancelling.
      if (!GetOverlappedResult((HANDLE)handle, &overlapped, &bytes_read, true)
          && GetLastError() != ERROR_OPERATION_ABORTED)
        return win32Err("failed to get overlapped result from ", handle);
    }
  }
  else
  {
    if (!ReadFile((HANDLE)handle, buffer.ptr, buffer.len, &bytes_read, NULL))
      return win32Err("failed to read from ", handle);
  }

  return (s64)bytes_read;
}

/* ----------------------------------------------------------------------------
 *   This function uses a sentinel for detecting if a write error occurs on a
 *   file handle used by the logger, since if that happens we will hit a stack
 *   overflow with the ERROR log macro used within. This is probably not a
 *   safe way to handle this, so should be made more robust later.
 */
thread_local static b8 iro_platform_writing_logging_error = false;
s64 write(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 /*is_pty*/)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  assert(buffer.ptr != nullptr || buffer.len == 0);

  OVERLAPPED overlapped, *overlapped_ptr;
  if (non_blocking)
  {
    memset(&overlapped, 0, sizeof(overlapped));
    overlapped_ptr = &overlapped;
  }
  else
  {
    overlapped_ptr = NULL;
  }

  DWORD bytes_written;
  BOOL success = WriteFile((HANDLE)handle, (LPCVOID)buffer.ptr,
    (DWORD)buffer.len, &bytes_written, overlapped_ptr);

  if (success == FALSE)
  {
    DWORD error = GetLastError();
    if (!(non_blocking && error == ERROR_IO_PENDING))
    {
      if (iro_platform_writing_logging_error)
      {
        printf("FATAL INTERNAL ERROR: recursion detected while writing an "
               "error out of platform::write, this is likely because "
               "we cannot write to a file handle stored in the logger!\n");
        printf("Last win32 error was: \n%s\n", makeWin32ErrorMsg(error).ptr);
        return 0;
      }
      iro_platform_writing_logging_error = true;
      ERROR("failed to write to file with handle ", handle, ": ",
        makeWin32ErrorMsg(error), "\n");
      iro_platform_writing_logging_error = false;
      cleanupWin32ErrorMsg();
      return -1;
    }
  }

  assert((DWORD)((s64)bytes_written) == bytes_written);
  return (s64)bytes_written;
}

/* ----------------------------------------------------------------------------
 *  On Windows this function is somewhat of a special case. Since we don't
 *  really have an equivalent to linux's poll, and this is currently only
 *  used for checking if a non-blocking file has data to read, we just use
 *  PeekNamedPipe and check if there are any bytes available to read. Once
 *  I'm back on Linux, I need to figure out why I added this to begin with
 *  and ideally find a way to remove it. It was apparently added because
 *  it was necessary for pseudo terminals, which don't work with async I/O on
 *  Windows anyways.
 */
b8 poll(fs::File::Handle handle, fs::PollEventFlags* flags)
{
  flags->clear();
  flags->set(fs::PollEvent::In);
  return true;
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  assert(flags != nullptr);

  DWORD bytes_available;
  DWORD bytes_available_message;
  if (!PeekNamedPipe(
        (HANDLE)handle,
        NULL,
        0,
        NULL,
        &bytes_available,
        &bytes_available_message))
  {
    // Its possible the process closed and the pipe has no more information.
    if (GetLastError() != ERROR_BROKEN_PIPE)
      return win32Err("failed to peek named pipe");
  }

  flags->clear();
  if (bytes_available > 0 || bytes_available_message > 0)
    flags->set(fs::PollEvent::In);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 isatty(fs::File::Handle handle)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  DWORD console_mode;
  return GetFileType((HANDLE)handle) == FILE_TYPE_CHAR &&
    GetConsoleMode((HANDLE)handle, &console_mode);
}

/* ----------------------------------------------------------------------------
 */
b8 setNonBlocking(fs::File::Handle handle)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);

  DWORD access =
      FILE_GENERIC_READ
    | FILE_GENERIC_WRITE;

  DWORD share =
      FILE_SHARE_READ
    | FILE_SHARE_WRITE
    | FILE_SHARE_DELETE;

  DWORD attributes =
      FILE_FLAG_BACKUP_SEMANTICS
    | FILE_FLAG_POSIX_SEMANTICS
    | FILE_FLAG_OVERLAPPED;

  HANDLE new_handle = ReOpenFile((HANDLE)handle, access, share, attributes);
  if (new_handle == INVALID_HANDLE_VALUE)
    return win32Err("failed to set file handle ", handle, " as non blocking");

  return true;
}

/* ----------------------------------------------------------------------------
 */
Timespec clock_realtime()
{
  u64 win32_time;
  GetSystemTimeAsFileTime((FILETIME*)&win32_time);
  win32_time -= 116444736000000000ULL; // Jan 1, 1601 to Jan 1, 1970
  return {win32_time / 10000000ULL, win32_time % 10000000ULL * 100};
}

/* ----------------------------------------------------------------------------
 */
Timespec clock_monotonic()
{
  static LARGE_INTEGER ticks_per_sec;
  if (ticks_per_sec.QuadPart == 0)
    QueryPerformanceFrequency(&ticks_per_sec);

  LARGE_INTEGER ticks;
  QueryPerformanceCounter(&ticks);

  u64 seconds = ticks.QuadPart / ticks_per_sec.QuadPart;
  u64 nanoseconds = (ticks.QuadPart % ticks_per_sec.QuadPart)
    * (1000000000ULL / ticks_per_sec.QuadPart);
  return {seconds, nanoseconds};
}

/* ----------------------------------------------------------------------------
 */
b8 opendir(fs::Dir::Handle* out_handle, String path)
{
  assert(out_handle != nullptr);
  assert(!path.isEmpty());

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  s32 wpath_len = stringToWideChar(path, wpath, buflen-3);
  assert(wpath_len != buflen-3 && "temp buffer too small in opendir");

  if (!(GetFileAttributesW(wpath) & FILE_ATTRIBUTE_DIRECTORY))
    return ERROR("failed to open dir at path '", path,
                 "': not a directory\n");

  if (isPathDeliminator(wpath[wpath_len-1]))
  {
    wpath[wpath_len] = L'*';
    wpath[wpath_len+1] = L'\0';
  }
  else
  {
    wpath[wpath_len] = L'\\';
    wpath[wpath_len+1] = L'*';
    wpath[wpath_len+2] = L'\0';
  }

  WIN32_FIND_DATAW data;
  HANDLE search_handle = FindFirstFileW(wpath, &data);

  if (search_handle == INVALID_HANDLE_VALUE)
    return win32Err("failed to open path '", path, "'");

  *out_handle = (fs::Dir::Handle)search_handle;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 opendir(fs::Dir::Handle* out_handle, fs::File::Handle file_handle)
{
  assert(out_handle != nullptr);
  assert((HANDLE)file_handle != INVALID_HANDLE_VALUE);

  BY_HANDLE_FILE_INFORMATION file_info;
  if (!GetFileInformationByHandle((HANDLE)file_handle, &file_info))
    return win32Err("failed to open dir from handle ", file_handle);

  if (!(file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
  {
    ERROR("failed to open dir from file handle ", file_handle, "': "
      "not a directory\n");
    return false;
  }

  GetFinalPathNameByHandleW((HANDLE)file_handle, win32_str,
    win32_str_capacity + 1, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

  WIN32_FIND_DATAW data;
  HANDLE search_handle = FindFirstFileW(win32_str, &data);
  if (search_handle == INVALID_HANDLE_VALUE)
  {
    ERROR("failed to open dir from file handle ", file_handle, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  *out_handle = (fs::Dir::Handle)search_handle;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 closedir(fs::Dir::Handle handle)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);

  if (FindClose((HANDLE)handle))
    return true;

  ERROR("failed to close dir with handle ", handle, ": ",
    makeWin32ErrorMsg(GetLastError()), "\n");
  cleanupWin32ErrorMsg();
  return false;
}

/* ----------------------------------------------------------------------------
 */
s64 readdir(fs::Dir::Handle handle, Bytes buffer)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  assert(buffer.isValid());

  WIN32_FIND_DATAW find_data;
  while (true)
  {
    if (FindNextFileW((HANDLE)handle, &find_data) == 0)
    {
      DWORD error = GetLastError();
      if (error == ERROR_NO_MORE_FILES)
      {
        return 0;
      }
      else
      {
        ERROR("failed to close dir with handle ", handle, ": ",
          makeWin32ErrorMsg(error), "\n");
        cleanupWin32ErrorMsg();
        return -1;
      }
    }

    // skip "." and ".."
    if (find_data.cFileName[0] == L'.' && (find_data.cFileName[1] == L'\0' ||
       (find_data.cFileName[1] == L'.' && find_data.cFileName[2] == L'\0')))
    {
      continue;
    }

    break;
  }

  int utf8_len =
    WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
      find_data.cFileName,
      -1,
      (char*)buffer.ptr,
      (int)buffer.len,
      nullptr,
      nullptr);

  if (utf8_len == 0)
  {
    ERROR("failed to convert dir entry to utf8: ",
          makeWin32ErrorMsg(GetLastError()));
    cleanupWin32ErrorMsg();
    return -1;
  }

  return utf8_len-1; // maybe safe idk
}

/* ----------------------------------------------------------------------------
 */
b8 stat(fs::FileInfo* out_info, String path)
{
  assert(out_info != nullptr);
  assert(notnil(path));
  assert(!path.isEmpty());

  mem::zero(out_info, sizeof(*out_info));

  if (path.len == 2 && path.ptr[1] == ':')
  {
    out_info->kind = fs::FileKind::NotFound;
    return false;
  }

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  if (0 == stringToWideChar(path, wpath, buflen))
    return false;

  HANDLE handle =
    CreateFileW(
        wpath,
        FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ
        | FILE_SHARE_WRITE
        | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS
        | FILE_FLAG_POSIX_SEMANTICS,
        NULL);

  if (handle == INVALID_HANDLE_VALUE)
  {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND)
    {
      out_info->kind = fs::FileKind::NotFound;
    }
    else
    {
      ERROR("failed to stat file at path '", path, "': ",
        makeWin32ErrorMsg(error), "\n");
      cleanupWin32ErrorMsg();
    }
    return false;
  }
  defer{ CloseHandle(handle); };

  switch (GetFileType(handle))
  {
    case FILE_TYPE_CHAR:
      DWORD console_mode;
      if (GetConsoleMode(handle, &console_mode))
      {
        out_info->kind = fs::FileKind::Character;
        return true;
      }
      break;
    case FILE_TYPE_DISK:
      break;
    case FILE_TYPE_PIPE:
      if (GetNamedPipeInfo(handle, NULL, NULL, NULL, NULL) == 0)
        out_info->kind = fs::FileKind::Socket;
      else
        out_info->kind = fs::FileKind::FIFO;
      return true;
    default:
      DWORD error = GetLastError();
      if (error != NO_ERROR)
      {
        ERROR("failed to stat file at path '", path, "': ",
          makeWin32ErrorMsg(error), "\n");
        cleanupWin32ErrorMsg();
      }
      out_info->kind = fs::FileKind::Unknown;
      return false;
  }

  FILE_BASIC_INFO basic_info;
  if (GetFileInformationByHandleEx(handle, FileBasicInfo, &basic_info,
    sizeof(basic_info)) == 0)
  {
    ERROR("failed to stat file at path '", path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  FILE_STANDARD_INFO standard_info;
  if (GetFileInformationByHandleEx(handle, FileStandardInfo, &standard_info,
    sizeof(standard_info)) == 0)
  {
    ERROR("failed to stat file at path '", path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  if (basic_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
  {
    out_info->kind = fs::FileKind::SymLink;
    out_info->byte_size = (u64)standard_info.EndOfFile.QuadPart;
  }
  else if (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
  {
    out_info->kind = fs::FileKind::Directory;
  }
  else
  {
    out_info->kind = fs::FileKind::Regular;
    out_info->byte_size = (u64)standard_info.EndOfFile.QuadPart;
  }

  out_info->last_access_time = filetimeToTimePoint(basic_info.LastAccessTime);
  out_info->last_modified_time = filetimeToTimePoint(basic_info.LastWriteTime);
  out_info->last_status_change_time = filetimeToTimePoint(basic_info.ChangeTime);
  out_info->birth_time = filetimeToTimePoint(basic_info.CreationTime);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 fileExists(String path)
{
  assert(notnil(path));
  assert(!path.isEmpty());

  const u64 buflen = 4096;

  wchar_t wpath[buflen] = {};
  if (0 == stringToWideChar(path, wpath, buflen))
    return false;

  return GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES;
}

/* ----------------------------------------------------------------------------
 */
b8 copyFile(String dst, String src)
{
  assert(notnil(dst));
  assert(notnil(src));
  assert(!dst.isEmpty());
  assert(!src.isEmpty());

  const u64 buflen = 4096;
  wchar_t wpath_dst[buflen];
  wchar_t wpath_src[buflen];

  if (0 == stringToWideChar(dst, wpath_dst, buflen))
    return false;

  if (0 == stringToWideChar(src, wpath_src, buflen))
    return false;

  if (SUCCEEDED(CopyFileW(wpath_src, wpath_dst, NULL)) == TRUE)
    return true;

  return win32Err("failed to copy file from ", src, " to ", dst);
}

/* ----------------------------------------------------------------------------
 */
b8 unlinkFile(String path)
{
  assert(notnil(path));
  assert(!path.isEmpty());

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  if (0 == stringToWideChar(path, wpath, buflen))
    return false;

  if (DeleteFileW(wpath))
    return true;

  return win32Err("failed to unlink file at path '", path, "'");
}

/* ----------------------------------------------------------------------------
 */
b8 removeDir(String path)
{
  assert(notnil(path));
  assert(!path.isEmpty());

  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to remove dir at path '", path, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };

  if (!RemoveDirectoryW(wpath))
    return true;

  ERROR("failed to remove dir at path '", path, "': ",
    makeWin32ErrorMsg(GetLastError()), "\n");
  cleanupWin32ErrorMsg();
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 makeDir(String path, b8 make_parents)
{
  assert(notnil(path));

  if (path.isEmpty())
    return true;

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  s32 wpath_length = stringToWideChar(path, wpath, buflen);

  if (wpath_length == 0)
    return false;

  if (wpath[wpath_length-1] != '\\' && wpath[wpath_length-1] != '/')
  {
    assert(wpath_length < buflen);
    wpath[wpath_length] = '\\';
    wpath[wpath_length+1] = '\0';
  }

  // early out if path already exists
  if (GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES)
    return true;

  if (!make_parents)
  {
    if (CreateDirectoryW(wpath, NULL) == 0)
    {
      ERROR("failed to make directory at path '", path, "': ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
    return true;
  }

  wchar_t wpath_partial[buflen];
  for (s64 i = 0; i < path.len; i += 1)
  {
    if ((path.ptr[i] == '/' && path.ptr[i-1] != ':') || i == path.len - 1)
    {
      auto partial_path = String{path.ptr, (u64)i+1};

      if (0 == stringToWideChar(partial_path, wpath_partial, buflen))
        return false;

      if (CreateDirectoryW(wpath_partial, NULL) == 0)
      {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
          return win32Err("failed to make directory at '", partial_path, "' "
                          "when trying to make directory '", path, "'");
      }
    }
  }

  return true;
}

using PipeName = io::StaticBuffer<64>;

/* ----------------------------------------------------------------------------
 */
static b8 createServerPipe(
    HANDLE* handle,
    PipeName* pipe_name,
    DWORD access,
    u64 rand)
{

  for (;;)
  {
    io::formatv(pipe_name, R"(\\.\pipe\)", rand);

    *handle =
      CreateNamedPipeA(
        (LPCSTR)pipe_name->asStr().ptr,
          access
        | FILE_FLAG_FIRST_PIPE_INSTANCE,
          PIPE_TYPE_BYTE
        | PIPE_READMODE_BYTE
        | PIPE_WAIT,
        1, // Max number of instances of this pipe.
        65536,
        65536,
        0,
        nullptr);

    if (*handle != INVALID_HANDLE_VALUE)
      return true;

    int err = GetLastError();
    if (err != ERROR_PIPE_BUSY && err != ERROR_ACCESS_DENIED)
      return win32Err("failed to create server pipe");

    // There must have been a name collision, so increment rand until
    // we find a good name.
    rand += 1;
  }
}

/* ----------------------------------------------------------------------------
 */
static b8 createPipePair(
    HANDLE* server_pipe,
    DWORD server_access,
    HANDLE* client_pipe,
    DWORD client_access,
    b8 inherit_client,
    u64 rand)
{
  // server_access |= WRITE_DAC;
  // client_access |= WRITE_DAC;

  auto failsafe = deferWithCancel
  {
    if (*server_pipe != INVALID_HANDLE_VALUE)
      CloseHandle(*server_pipe);
    if (*client_pipe != INVALID_HANDLE_VALUE)
      CloseHandle(*client_pipe);
  };

  PipeName server_pipe_name;
  if (!createServerPipe(server_pipe, &server_pipe_name, server_access, rand))
    return false;

  SECURITY_ATTRIBUTES sec_attr;
  sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attr.lpSecurityDescriptor = nullptr;
  sec_attr.bInheritHandle = inherit_client;

  *client_pipe =
    CreateFileA(
      (LPCSTR)server_pipe_name.asStr().ptr,
      client_access,
      0,
      &sec_attr,
      OPEN_EXISTING,
      0,
      nullptr);

  if (*client_pipe == INVALID_HANDLE_VALUE)
    return win32Err("failed to create client pipe");

  if (!ConnectNamedPipe(*server_pipe, nullptr))
  {
    if (GetLastError() != ERROR_PIPE_CONNECTED)
      return win32Err("failed to connect named pipe");
  }

  failsafe.cancel();
  return true;
}

/* ----------------------------------------------------------------------------
 */
static b8 createNullHandle(HANDLE* handle, DWORD access)
{
  SECURITY_ATTRIBUTES sec_attr;
  sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attr.lpSecurityDescriptor = NULL;
  sec_attr.bInheritHandle = TRUE;

  *handle =
    CreateFileW(
      L"NUL",
      access,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      &sec_attr,
      OPEN_EXISTING,
      0,
      NULL);

  if (*handle == INVALID_HANDLE_VALUE)
    return win32Err("failed to create nul file handle");
  return true;
}

/* ============================================================================
 */
struct ProcessWin32
{
  HANDLE h_process = INVALID_HANDLE_VALUE;
  // A handle to the pipe end that we read from. Both the child's stdout and
  // stderr are set to this pipe.
  HANDLE h_stdout = INVALID_HANDLE_VALUE;
};


// TODO(sushi) make thread safe.
using ProcessPool = StaticPool<ProcessWin32>;
static ProcessPool g_process_pool;

/* ----------------------------------------------------------------------------
 */
b8 processSpawn(
    Process::Handle* out_handle,
    String file,
    Slice<String> args,
    String cwd)
{
  assert(out_handle);
  assert(notnil(file));

  ProcessWin32* p_proc = g_process_pool.add();
  if (p_proc == nullptr)
    return ERROR("failed to allocate a ProcessWin32\n");

  HANDLE h_child_stdout_write = INVALID_HANDLE_VALUE;
  HANDLE h_child_stdout_read = INVALID_HANDLE_VALUE;

  auto failsafe = deferWithCancel
  {
    ERROR("failed to spawn process ", file, "\n");
    if (h_child_stdout_write != INVALID_HANDLE_VALUE)
      CloseHandle(h_child_stdout_write);
    if (h_child_stdout_read != INVALID_HANDLE_VALUE)
      CloseHandle(h_child_stdout_read);
    g_process_pool.remove(p_proc);
  };

  u64 file_chars = 2 + file.countCharacters();
  u64 cmd_chars = file_chars;
  for (int i = 0; i < args.len; i += 1)
    cmd_chars += 3 + args[i].countCharacters(); // +3 b/c "" and space
  if (cmd_chars == 0)
    return false;
  cmd_chars += 1; // null-terminator
  assert(cmd_chars <= 32766); // CreateProcessW() max lpCommandLine length

  // allocate the wchar string
  wchar_t* wargs = win32_str;
  if (cmd_chars - 1 > win32_str_capacity)
  {
    wargs = (wchar_t*)mem::stl_allocator.allocate(cmd_chars);
    if (wargs == nullptr)
    {
      ERROR("failed to spawn process '", file, "': insufficient memory when"
        " converting from UTF-8 to WideChar\n");
      return false;
    }
  }
  defer{ if (wargs != win32_str) mem::stl_allocator.free(wargs); };

  mem::zero(wargs, cmd_chars);

  u64 cmd_offset = 0;

  auto cmdWriteChar = [&](wchar_t c)
  {
    wargs[cmd_offset++] = c;
  };

  // Lazy way to handle properly quoting arguments. Should do this better later
  // but idc for working out how to do this in utf16 rn.
  io::Memory arg_quote_buf;
  arg_quote_buf.open();
  defer { arg_quote_buf.close(); };
  auto cmdWriteString = [&](String s)
  {
    arg_quote_buf.clear();
    for (u32 i = 0; i < s.len; ++i)
    {
      if (s.ptr[i] == '"')
        io::formatv(&arg_quote_buf, '\\', '"');
      else
        io::format(&arg_quote_buf, (char)s.ptr[i]);
    }
    int bytes_written =
      stringToWideChar(
        arg_quote_buf.asStr(),
        (wargs + cmd_offset),
        cmd_chars - cmd_offset,
        false);
    if (bytes_written == 0)
      return false;
    cmd_offset += bytes_written;
    return true;
  };

  // convert the utf8 file to wchar
  cmdWriteChar(L'"');
  assert(file_chars <= INT_MAX);
  if (!cmdWriteString(file))
    return ERROR("failed to spawn pty process '", file, "'\n");
  cmdWriteChar(L'"');

  // convert the utf8 args to wchar
  for (int i = 0; i < args.len - 1; ++i)
  {
    cmdWriteChar(L' ');
    cmdWriteChar(L'"');

    if (!cmdWriteString(args[i]))
      return ERROR("failed to spawn pty process '", file, "'\n");

    cmdWriteChar(L'"');
  }
  cmdWriteChar(L'\0');

  // convert the utf8 cwd to wchar
  wchar_t* wcwd = NULL;
  if (notnil(cwd))
  {
    u64 cwd_chars = cwd.countCharacters();
    wcwd = (wchar_t*)mem::stl_allocator.allocate(cwd_chars);
    if (wcwd == nullptr)
    {
      ERROR("failed to spawn process '", file, "': insufficient memory when"
        " converting from UTF-8 to WideChar\n");
      return false;
    }

    if (!stringToWideChar(cwd, wcwd, cwd_chars))
      return ERROR("failed to convert cwd to wide chars\n");
  }
  defer{ if (wcwd != nullptr) mem::stl_allocator.free(wcwd); };

  SECURITY_ATTRIBUTES sec_attr = {};
  sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attr.bInheritHandle = TRUE;
  sec_attr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&h_child_stdout_read, &h_child_stdout_write, &sec_attr, 0))
    return win32Err("failed to create pipes for process");

  if (!SetHandleInformation(h_child_stdout_read, HANDLE_FLAG_INHERIT, 0))
    return win32Err("failed to set child stdout read as non-inherited");

  STARTUPINFOW startup_info = {};
  startup_info.cb          = sizeof(startup_info);
  startup_info.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = SW_HIDE;
  startup_info.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput  = h_child_stdout_write;
  startup_info.hStdError   = h_child_stdout_write;

  // create the process with handle duplication, no window, and parent stdio
  PROCESS_INFORMATION process_info = {};
  if (!CreateProcessW(
        NULL,
        wargs,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        wcwd,
        &startup_info,
        &process_info))
    return win32Err("failed to create process");

  p_proc->h_stdout = h_child_stdout_read;
  p_proc->h_process = process_info.hProcess;
  CloseHandle(h_child_stdout_write);
  CloseHandle(process_info.hThread); // we don't need to track the thread

  TRACE("started process ", file, ":", (u64)process_info.hProcess, " with args:\n");
  for (String s : args)
  {
    TRACE(s, "\n");
  }

  *out_handle = (Process::Handle)p_proc;

  failsafe.cancel();
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processHasOutput(Process::Handle h_process)
{
  auto* p_proc = (ProcessWin32*)h_process;
  if (p_proc == nullptr)
    return ERROR("processHasOutput passed a null process handle\n");

  DWORD bytes_available = 0;
  if (!PeekNamedPipe(p_proc->h_stdout, NULL, 0, NULL, &bytes_available, NULL))
  {
    if (GetLastError() != ERROR_BROKEN_PIPE)
      return win32Err("failed to peek process pipe");
    return false;
  }
  if (bytes_available > 0)
    TRACE((u64)p_proc->h_process, " has ", (u64)bytes_available, " bytes available\n");
  return bytes_available > 0;
}

/* ----------------------------------------------------------------------------
 */
u64 processRead(Process::Handle h_process, Bytes buffer)
{
  assert(buffer.ptr != nullptr && buffer.len != 0);

  auto* p_proc = (ProcessWin32*)h_process;
  if (p_proc == nullptr)
    return ERROR("processReadOutput passed a null process handle\n");

  DWORD bytes_to_read = buffer.len;
  if (WaitForSingleObject(p_proc->h_process, 0) != WAIT_OBJECT_0)
  {
    // The process is still running, so peek the pipe.
    if (!PeekNamedPipe(
          p_proc->h_stdout,
          NULL,
          0,
          NULL,
          &bytes_to_read,
          NULL))
    {
      if (GetLastError() != ERROR_BROKEN_PIPE)
        return win32Err("failed to peek process pipe");
    }

    TRACE("peeked ", (u64)bytes_to_read, " from ", (u64)p_proc->h_process, "\n");
    bytes_to_read = min<u64>(bytes_to_read, buffer.len);
  }

  if (0 == bytes_to_read)
    return 0;

  DWORD bytes_read;
  TRACE("read from ", (u64)p_proc->h_process, "\n");
  if (!ReadFile(
        p_proc->h_stdout,
        buffer.ptr,
        bytes_to_read,
        &bytes_read,
        NULL))
    return win32Err("failed to read process stdout pipe");
  TRACE("found ", (u64)bytes_read, " bytes\n");

  return (u64)bytes_read;
}

/* ----------------------------------------------------------------------------
 */
b8 processHasExited(Process::Handle h_process, s32* out_exit_code)
{
  auto* p_proc = (ProcessWin32*)h_process;
  if (p_proc == nullptr)
    return ERROR("processHasExited passed a null process handle\n");

  if (WaitForSingleObject(p_proc->h_process, 0) == WAIT_OBJECT_0)
  {
    if (out_exit_code)
      GetExitCodeProcess(p_proc->h_process, (LPDWORD)out_exit_code);

    TRACE((u64)p_proc->h_process, " exited\n");
    return true;
  }

  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 processClose(Process::Handle h_process)
{
  auto* p_proc = (ProcessWin32*)h_process;
  if (p_proc == nullptr)
    return ERROR("processClose passed a null process handle\n");

  if (!processHasExited(h_process, nullptr))
  {
    if (!TerminateProcess(p_proc->h_process, 0))
      return win32Err("failed to stop process with handle ", h_process);
  }

  if (p_proc->h_stdout != INVALID_HANDLE_VALUE)
    CloseHandle(p_proc->h_stdout);
  CloseHandle(p_proc->h_process);

  g_process_pool.remove(p_proc);
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 realpath(fs::Path* path)
{
  assert(path != nullptr);

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  if (0 == stringToWideChar(path->buffer.asStr(), wpath, buflen))
    return false;

  DWORD full_wpath_chars = GetFullPathNameW(wpath, 0, NULL, NULL);
  if (full_wpath_chars == 0)
    return win32Err("failed to canonicalize path ", *path);

  wchar_t* full_wpath =
	  (wchar_t*)mem::stl_allocator.allocateType<wchar_t>(full_wpath_chars);
  if (full_wpath == nullptr)
    return ERROR("failed to canonicalize path '", *path, "': insufficient "
                 "memory when converting from UTF-8 to WideChar\n");
  defer{ mem::stl_allocator.free(full_wpath); };

  full_wpath_chars = GetFullPathNameW(wpath, full_wpath_chars,
    full_wpath, NULL);
  if (full_wpath_chars == 0)
    return win32Err("failed to canonicalize path ", *path);

  assert(full_wpath_chars <= (DWORD)INT_MAX);
  int utf8_bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
    full_wpath, (int)full_wpath_chars, NULL, 0, NULL, NULL);
  if (utf8_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to canonicalize path '", *path, "': invalid unicode was"
      " found in a string when converting from WideChar to UTF-8\n");
    return false;
  }

  path->clear();
  path->reserve((s32)utf8_bytes + 1);
  path->commit((s32)utf8_bytes);

  int converted_bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
    full_wpath, (int)full_wpath_chars, (char*)path->buffer.ptr,
    utf8_bytes + 1, NULL, NULL);
  if (converted_bytes == 0)
    return win32Err("failed to canonicalize path ", *path);

  auto normalized = normalizePath(path->buffer.asStr());
  path->buffer.len = normalized.len;

  return true;
}

/* ----------------------------------------------------------------------------
 */
fs::Path cwd(mem::Allocator* allocator)
{
  assert(allocator != nullptr);

  DWORD wpath_chars = GetCurrentDirectoryW(0, NULL);
  if (wpath_chars == 0)
  {
    ERROR("failed to get the current working directory: ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return nil;
  }

  wchar_t* wpath = win32_str;
  if (wpath_chars > win32_str_capacity)
  {
    wpath = (wchar_t*)mem::stl_allocator.allocate(wpath_chars + 1);
    if (wpath == nullptr)
    {
      ERROR("failed to get the current working directory: insufficient memory"
        " when converting from UTF-8 to WideChar\n");
      return nil;
    }
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };

  wpath_chars = GetCurrentDirectoryW(wpath_chars + 1, wpath);
  if (wpath_chars == 0)
  {
    ERROR("failed to get the current working directory: ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return nil;
  }

  assert(wpath_chars <= (DWORD)INT_MAX);
  int utf8_bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
    wpath, (int)wpath_chars, NULL, 0, NULL, NULL);
  if (utf8_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to get the current working directory: invalid unicode was"
      " found in a string when converting from WideChar to UTF-8\n");
    return nil;
  }

  fs::Path path;
  if (!path.init(allocator))
  {
    ERROR("failed to get the current working directory: fs::Path failed to"
      " init\n");
    return nil;
  }
  path.reserve((s32)utf8_bytes + 1);
  path.commit((s32)utf8_bytes);

  int converted_bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
    wpath, (int)wpath_chars, (char*)path.buffer.ptr, utf8_bytes + 1,
    NULL, NULL);
  if (converted_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to get the current working directory: invalid unicode was"
      " found in a string when converting from WideChar to UTF-8\n");
    path.destroy();
    return nil;
  }

  String normalized = normalizePath(path.buffer.asStr());
  path.buffer.len = normalized.len;

  return path;
}

/* ----------------------------------------------------------------------------
 */
b8 chdir(String path)
{
  assert(notnil(path));

  const u64 buflen = 4096;
  wchar_t wpath[buflen];
  s32 wpath_len = stringToWideChar(path, wpath, buflen-1);

  if (0 == wpath_len)
    return false;

  if (SetCurrentDirectoryW(wpath) == 0){
    ERROR("failed to get the current working directory: ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 chdir(fs::Dir::Handle dir_handle)
{
  GetFinalPathNameByHandleW((HANDLE)dir_handle, win32_str,
    win32_str_capacity + 1, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

  if (SetCurrentDirectoryW(win32_str) == 0)
  {
    ERROR("failed to get the current working directory: ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
TermSettings termSetNonCanonical(mem::Allocator* allocator)
{
  DWORD mode;
  HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(console, &mode);
  SetConsoleMode(console, mode & ~ENABLE_LINE_INPUT);
  return (TermSettings)(u64)mode;
}

/* ----------------------------------------------------------------------------
 */
void termRestoreSettings(TermSettings settings, mem::Allocator* allocator)
{
  HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(console, (DWORD)(u64)settings);
}

/* ----------------------------------------------------------------------------
 */
b8 touchFile(String path)
{
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to touch file at path '", path, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };

  HANDLE handle = CreateFileW(wpath, FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS,
    NULL);

  if (handle == INVALID_HANDLE_VALUE)
  {
    ERROR("failed to touch file at path '", path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  defer{ CloseHandle(handle); };

  FILETIME file_time;
  SYSTEMTIME system_time;
  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);

  if (SetFileTime(handle, NULL, NULL, &file_time) == 0)
  {
    ERROR("failed to touch file at path '", path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
s32 getEnvVar(String name, Bytes buffer)
{
	DWORD bytes_needed = GetEnvironmentVariable((LPCSTR)name.ptr, NULL, 0);

	if (bytes_needed == 0)
	{
		if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
      return -1;
    return ERROR_WIN32("failed to get environment variable");
	}

  if (isnil(buffer))
    return bytes_needed;

	if (buffer.len < bytes_needed)
	{
		ERROR("provided buffer is not large enough to store env var value\n");
		return -1;
	}

  if (0 == GetEnvironmentVariable((LPCSTR)name.ptr, (LPSTR)buffer.ptr, buffer.len))
    return ERROR_WIN32("failed to get environment variable");

  return s32(bytes_needed)-1;
}

/* ----------------------------------------------------------------------------
 */
b8 setEnvVar(String name, String value)
{
  if (!SetEnvironmentVariable((LPCSTR)name.ptr, (LPCSTR)value.ptr))
    return ERROR_WIN32("failed to set envvar '", name, "' to '", value, "'");
  return true;
}

/* ----------------------------------------------------------------------------
 */
void exit(int status)
{
  _exit(status);
}

/* ----------------------------------------------------------------------------
 */
b8 isRunningUnderDebugger()
{
  return IsDebuggerPresent();
}

/* ----------------------------------------------------------------------------
 */
void debugBreak()
{
  DebugBreak();
}

/* ----------------------------------------------------------------------------
 */
u16 byteSwap(u16 x)
{
#if defined(_MSC_VER)
  return _byteswap_ushort(x);
#else // #if defined(_MSC_VER)
  return __builtin_bswap16(x);
#endif // #else // #if defined(_MSC_VER)
}

u32 byteSwap(u32 x)
{
#if defined(_MSC_VER)
  return _byteswap_ulong(x);
#else // #if defined(_MSC_VER)
  return __builtin_bswap32(x);
#endif // #else // #if defined(_MSC_VER)
}

u64 byteSwap(u64 x)
{
#if defined(_MSC_VER)
  return _byteswap_uint64(x);
#else // #if defined(_MSC_VER)
  return __builtin_bswap64(x);
#endif // #else // #if defined(_MSC_VER)
}

}

#endif // #if IRO_WIN32
