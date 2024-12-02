#if IRO_WIN32

#define WIN32_LEAN_AND_MEAN
#include "winsock2.h"
#include "windows.h"
#undef ERROR
#undef max
#undef min

#include "platform.h"

#include "logger.h"
#include "unicode.h"
#include "memory/allocator.h"

#include "climits"
#include "cwchar"

namespace iro::platform
{

static iro::Logger logger = 
  iro::Logger::create("platform.win32"_str, iro::Logger::Verbosity::Notice);

// NOTE: Ideally we would use less than 32771, but GetFinalPathNameByHandleW
// can not be called to determine the output string length before filling
// the string buffer. So, we provide it a buffer that can hold the max
// WideChar path length 32767, plus 4 for the "\\?\" prefix.
#define win32_str_capacity 32771
thread_local wchar_t win32_str[win32_str_capacity + 1] = {0};
thread_local const char* win32_make_path_error = nullptr;

thread_local LPSTR win32_error_msg = nullptr;

/* ----------------------------------------------------------------------------
 */
static wchar_t* makeWin32Path(String input, u64 extra_bytes = 0,
  u64* output_len = 0, b8 force_allocate = false)
{
  wchar_t* output = win32_str;
  
  DWORD full_path_length = GetFullPathNameA((LPCSTR)input.ptr, 0, NULL, NULL);
  char* full_path = (char*)_alloca((size_t)full_path_length + 1);
  full_path_length = GetFullPathNameA((LPCSTR)input.ptr, full_path_length + 1,
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
  
  bytes += 5; // "\\?\" + null-terminator
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
  
  output[0] = L'\\';
  output[1] = L'\\';
  output[2] = L'?';
  output[3] = L'\\';
  
  assert(full_path_length <= INT_MAX);
  assert(bytes <= INT_MAX);
  int converted_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    (LPCCH)full_path, (int)full_path_length, output + 4, (int)bytes - 5);
  if (converted_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    win32_make_path_error = "invalid unicode was found in a string when"
      " converting from UTF-8 to WideChar\n";
    return nullptr;
  }
  
  if (output_len != nullptr)
    *output_len = converted_bytes + 4;
  
  output[converted_bytes + 4] = L'\0';
  return output;
}

static String makeWin32ErrorMsg(DWORD error)
{
	DWORD win32_error_msg_len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
    MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPSTR)&win32_error_msg, 0, NULL);
  return String::from((u8*)win32_error_msg, (u64)win32_error_msg_len);
}

static void cleanupWin32ErrorMsg()
{
  LocalFree(win32_error_msg);
}

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
  
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to open file at path '", path, "': ", win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
  DWORD access;
  if (!flags.testAll<Read, Write>())
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
  
  int filtered_flags = (int)flags.flags
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
s64 read(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 is_pty)
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
  
  DWORD bytes_read;
  BOOL success = ReadFile((HANDLE)handle, (LPVOID)buffer.ptr,
    (DWORD)buffer.len, &bytes_read, overlapped_ptr);
  
  if (success == FALSE)
  {
    DWORD error = GetLastError();
    if (!(non_blocking && error == ERROR_IO_PENDING) ||
      error != ERROR_HANDLE_EOF)
    {
      ERROR("failed to read from file with handle ", handle, ": ",
        makeWin32ErrorMsg(error), "\n");
      cleanupWin32ErrorMsg();
      return -1;
    }
  }
  
  return (s64)bytes_read;
}

/* ----------------------------------------------------------------------------
 */
s64 write(fs::File::Handle handle, Bytes buffer, b8 non_blocking, b8 is_pty)
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
      ERROR("failed to write to file with handle ", handle, ": ",
        makeWin32ErrorMsg(error), "\n");
      cleanupWin32ErrorMsg();
      return -1;
    }
  }
  
  assert((DWORD)((s64)bytes_written) == bytes_written);
  return (s64)bytes_written;
}

/* ----------------------------------------------------------------------------
 */
b8 poll(fs::File::Handle handle, fs::PollEventFlags* flags)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  assert(flags != nullptr);
  
  struct pollfd pfd = {};
  pfd.fd = (SOCKET)handle;
  
  if (flags->test(fs::PollEvent::In))
    pfd.events |= POLLIN;
  if (flags->test(fs::PollEvent::Out))
    pfd.events |= POLLOUT;
  
  flags->clear();
  
  if (WSAPoll(&pfd, 1, 0) == SOCKET_ERROR)
  {
    ERROR("failed to poll process with handle ", handle, ": ",
      makeWin32ErrorMsg(WSAGetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  if (pfd.revents & POLLIN)
    flags->set(fs::PollEvent::In);
  if (pfd.revents & POLLOUT)
    flags->set(fs::PollEvent::Out);
  if (pfd.revents & POLLERR)
    flags->set(fs::PollEvent::Err);
  if (pfd.revents & POLLNVAL)
    flags->set(fs::PollEvent::Invalid);
  if (pfd.revents & POLLHUP)
    flags->set(fs::PollEvent::HangUp);
  
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
  
  DWORD access = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  DWORD attributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS |
    FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_OVERLAPPED;
  
  HANDLE new_handle = ReOpenFile((HANDLE)handle, access, share, attributes);
  if (new_handle == INVALID_HANDLE_VALUE)
  {
    ERROR("failed to set the file handle ", handle, " to non blocking: ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
  }
  
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
  u64 nanoseconds = ((ticks.QuadPart % ticks_per_sec.QuadPart) * 1000ULL) /
    ticks_per_sec.QuadPart;
  return {seconds, nanoseconds};
}

/* ----------------------------------------------------------------------------
 */
b8 opendir(fs::Dir::Handle* out_handle, String path)
{
  assert(out_handle != nullptr);
  
  b8 free_wpath = false;
  wchar_t* wpath;
  if (path.len == 0)
  {
    wpath = (wchar_t*)L"./*";
  }
  else
  {
    u64 wpath_len;
    wpath = makeWin32Path(path, 2, &wpath_len);
    if (wpath == nullptr)
    {
      ERROR("failed to open dir at path '", path, "': ",
        win32_make_path_error);
      return false;
    }
    if (wpath != win32_str)
      free_wpath = true;
    
    if (!(GetFileAttributesW(wpath) & FILE_ATTRIBUTE_DIRECTORY))
    {
      ERROR("failed to open dir at path '", path, "': not a directory\n");
      return false;
    }
    
    wchar_t last_char = wpath[wpath_len - 1];
    if (last_char == L'/' || last_char == L'\\')
    {
      wpath[wpath_len + 0] = L'*';
      wpath[wpath_len + 1] = L'\0';
    }
    else
    {
      wpath[wpath_len + 0] = L'\\';
      wpath[wpath_len + 1] = L'*';
      wpath[wpath_len + 2] = L'\0';
    }
  }
  
  WIN32_FIND_DATAW data;
  HANDLE search_handle = FindFirstFileW(wpath, &data);
  
  if (free_wpath)
    mem::stl_allocator.free(wpath);
  
  if (search_handle == INVALID_HANDLE_VALUE)
  {
    ERROR("failed to open dir at path '", path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
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
  {
    ERROR("failed to open dir from file handle ", file_handle, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
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
  
  // NOTE: returned path starts with "\\?\"
  DWORD wpath_len = GetFullPathNameW(find_data.cFileName,
    win32_str_capacity + 1, win32_str, NULL);
  
  assert(wpath_len - 4 <= (DWORD)INT_MAX);
  int utf8_len = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
    win32_str + 4, (int)wpath_len, NULL, 0, NULL, NULL);
  if (utf8_len == 0)
    {
      assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
      ERROR("failed to close dir with handle ", handle, ": invalid unicode was"
        " found in a string when converting from WideChar to UTF-8\n");
      return -1;
    }
  
  if (utf8_len > buffer.len)
  {
    char* utf8_str = (char*)mem::stl_allocator.allocate(utf8_len);
    if (utf8_str == nullptr)
    {
      ERROR("failed to close dir with handle ", handle, ": insufficient memory"
        " when converting from WideChar to UTF-8\n");
      return -1;
    }
    
    int converted_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
      win32_str + 4, (int)(wpath_len - 4), utf8_str, utf8_len, NULL, NULL);
    if (converted_bytes == 0)
    {
      assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
      ERROR("failed to close dir with handle ", handle, ": invalid unicode was"
        " found in a string when converting from WideChar to UTF-8\n");
      return -1;
    }
    
    ERROR("failed to close dir with handle ", handle, ": buffer given to"
      " readdir() is too small. File name '", utf8_str, "' is ", utf8_len,
      " bytes long, but given buffer is ", buffer.len, " bytes long\n");
    return -1;
  }
  
  int converted_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
    win32_str + 4, (int)(wpath_len - 4), (char*)buffer.ptr, (int)buffer.len,
    NULL, NULL);
  if (converted_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to close dir with handle ", handle, ": invalid unicode was"
      " found in a string when converting from WideChar to UTF-8\n");
    return -1;
  }
  
  return (s64)converted_bytes;
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
  
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to stat file at path '", path, "': ", win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
  HANDLE handle = CreateFileW(wpath, FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS,
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
  
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to check if file at path '", path, "' exists: ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
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
  
  wchar_t* wpath_dst = makeWin32Path(dst);
  if (wpath_dst == nullptr)
  {
    ERROR("failed to copy file from '", src, "' to '", dst, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath_dst != win32_str) mem::stl_allocator.free(wpath_dst); };
  
  wchar_t* wpath_src = makeWin32Path(src, 0, 0, true);
  if (wpath_src == nullptr)
  {
    ERROR("failed to copy file from '", src, "' to '", dst, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ mem::stl_allocator.free(wpath_src); };
  
  if (SUCCEEDED(CopyFileW(wpath_src, wpath_dst, NULL)) == TRUE)
    return true;
  
  ERROR("failed to copy file from '", src, "' to '", dst, "': ",
    makeWin32ErrorMsg(GetLastError()), "\n");
  cleanupWin32ErrorMsg();
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 unlinkFile(String path)
{
  assert(notnil(path));
  assert(!path.isEmpty());
  
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to unlink file at path '", path, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
  if (!DeleteFileW(wpath))
    return true;
  
  ERROR("failed to unlink file at path '", path, "': ",
    makeWin32ErrorMsg(GetLastError()), "\n");
  cleanupWin32ErrorMsg();
  return false;
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
  
  u64 wpath_length;
  wchar_t* wpath = makeWin32Path(path, 1, &wpath_length);
  if (wpath == nullptr){
    ERROR("failed to make directory at path '", path, "': ",
      win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
  if (wpath[wpath_length-1] != '\\' && wpath[wpath_length-1] != '/')
  {
    assert(wpath_length < win32_str_capacity);
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
  
  for (s64 i = 0; i < path.len; i += 1)
  {
    if (path.ptr[i] == '/' || i == path.len - 1)
    {
      String partial_path = String{path.ptr, (u64)i+1};
      
      if (wpath != win32_str) mem::stl_allocator.free(wpath);
      wchar_t* wpath = makeWin32Path(partial_path);
      if (wpath == nullptr)
      {
        ERROR("failed to make directory at path '", path, "': ",
          win32_make_path_error);
        return false;
      }
      
      
      if (CreateDirectoryW(wpath, NULL) == 0)
      {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
        {
          ERROR("failed to make directory at path '", partial_path,
            "' when trying to make directory at path '", path, "': ",
            makeWin32ErrorMsg(error), "\n");
          cleanupWin32ErrorMsg();
          return false;
        }
      }
    }
  }
  
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processSpawn(
  Process::Handle* out_handle,
  String file,
  Slice<String> args,
  Process::Stream streams[3],
  String cwd)
{
  assert(out_handle);
  assert(notnil(file));
  
  struct Info
  {
    fs::File* f = nullptr;
    b8 non_blocking = false;
    HANDLE pipes[2] = {};
  };
  Info infos[3] =
  {
    {streams[0].file, streams[0].non_blocking},
    {streams[1].file, streams[1].non_blocking},
    {streams[2].file, streams[2].non_blocking},
  };
  
  u64 file_chars = file.countCharacters();
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
  
  // convert the utf8 file to wchar
  wargs[0] = L'"';
  assert(file_chars <= INT_MAX);
  int file_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    (LPCCH)file.ptr, (int)file.len, (wargs + 1), (int)cmd_chars);
  if (file_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to spawn process '", file, "': invalid unicode was found in"
      " a string when converting from UTF-8 to WideChar\n");
    return false;
  }
  wargs[file_chars] = L'"';
  
  // convert the utf8 args to wchar
  u64 args_offset = file_chars;
  for (int i = 0; i < args.len + 1; i += 1)
  {
    assert(args_offset < cmd_chars);
    wargs[args_offset++] = L' ';
    wargs[args_offset++] = L'"';
    
    int arg_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      (LPCCH)args[i].ptr, (int)args[i].len, (wargs + args_offset),
      (int)(cmd_chars - args_offset));
    if (arg_bytes == 0)
    {
      assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
      ERROR("failed to spawn process '", file, "': invalid unicode was found"
        " in a string when converting from UTF-8 to WideChar\n");
      return false;
    }
    
    args_offset += arg_bytes;
    wargs[args_offset++] = L'"';
  }
  assert(args_offset == cmd_chars);
  wargs[cmd_chars] = L'\0';
  
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
    
    int cwd_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      (LPCCH)cwd.ptr, (int)cwd.len, wcwd, cwd_chars);
    if (cwd_bytes == 0)
    {
      assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
      ERROR("failed to spawn process '", file, "': invalid unicode was found"
        " in a string when converting from UTF-8 to WideChar\n");
      return false;
    }
  }
  defer{ if (wcwd != nullptr) mem::stl_allocator.free(wcwd); };
  
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = NULL;
  security_attributes.bInheritHandle = TRUE;
  
  // create a pipe for stdin; have the child process inherit the reading end
  if (infos[0].f != nullptr)
  {
    if (notnil(*infos[0].f))
    {
      ERROR("The file of the stdin stream was not nil when trying to spawn"
        " process '", file, "': Given files cannot already own resources.\n");
      return false;
    }
    
    if (CreatePipe(&infos[0].pipes[0], &infos[0].pipes[1],
      &security_attributes, 0) == 0)
    {
      ERROR("failed to open pipes for the stdin stream when trying to spawn"
        " process '", file, "': ", makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
    
    if (SetHandleInformation(infos[0].pipes[0], HANDLE_FLAG_INHERIT, 0) == 0)
    {
      ERROR("failed to set handle information for the stdin stream when"
        " trying to spawn process '", file, "': ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
  }
  
  // create a pipe for stdout; have the child process inherit the writing end
  if (infos[1].f != nullptr)
  {
    if (notnil(*infos[1].f))
    {
      ERROR("The file of the stdin stream was not nil when trying to spawn"
        " process '", file, "': Given files cannot already own resources.\n");
      return false;
    }
    
    if (CreatePipe(&infos[1].pipes[0], &infos[1].pipes[1],
      &security_attributes, 0) == 0)
    {
      ERROR("failed to open pipes for the stdin stream when trying to spawn"
        " process '", file, "': ", makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
    
    if (SetHandleInformation(infos[1].pipes[1], HANDLE_FLAG_INHERIT, 0) == 0)
    {
      ERROR("failed to set handle information for the stdin stream when"
        " trying to spawn process '", file, "': ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
  }
  
  // create a pipe for stderr; have the child process inherit the writing end
  if (infos[2].f != nullptr)
  {
    if (notnil(*infos[2].f))
    {
      ERROR("The file of the stdin stream was not nil when trying to spawn"
        " process '", file, "': Given files cannot already own resources.\n");
      return false;
    }
    
    if (CreatePipe(&infos[2].pipes[0], &infos[2].pipes[1],
      &security_attributes, 0) == 0)
    {
      ERROR("failed to open pipes for the stdin stream when trying to spawn"
        " process '", file, "': ", makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
    
    if (SetHandleInformation(infos[2].pipes[1], HANDLE_FLAG_INHERIT, 0) == 0)
    {
      ERROR("failed to set handle information for the stdin stream when"
        " trying to spawn process '", file, "': ",
        makeWin32ErrorMsg(GetLastError()), "\n");
      cleanupWin32ErrorMsg();
      return false;
    }
  }
  
  STARTUPINFOW startup_info;
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = SW_HIDE;
  startup_info.hStdInput = infos[0].pipes[0];
  startup_info.hStdOutput = infos[1].pipes[1];
  startup_info.hStdError = infos[2].pipes[1];
  
  // create the process with handle duplication, no window, and parent stdio
  PROCESS_INFORMATION process_info;
  if (CreateProcessW(NULL, wargs, NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT,
    NULL, wcwd, &startup_info, &process_info) == 0)
  {
    ERROR("failed to spawn process '", file, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  // close reading end of the stdin pipe and set the stream to writing end
  if (infos[0].f != nullptr)
  {
    fs::OpenFlags flags = fs::OpenFlag::Write;
    if (infos[0].non_blocking)
      flags.set(fs::OpenFlag::NoBlock);
    *infos[0].f = fs::File::fromFileDescriptor((u64)infos[0].pipes[1], flags);
    CloseHandle(infos[0].pipes[0]);
  }
  
  // close writing end of the stdout pipe and set the stream to reading end
  if (infos[1].f != nullptr)
  {
    fs::OpenFlags flags = fs::OpenFlag::Read;
    if (infos[1].non_blocking)
      flags.set(fs::OpenFlag::NoBlock);
    *infos[1].f = fs::File::fromFileDescriptor((u64)infos[1].pipes[0], flags);
    CloseHandle(infos[1].pipes[1]);
  }
  
  // close writing end of the stderr pipe and set the stream to reading end
  if (infos[2].f != nullptr)
  {
    fs::OpenFlags flags = fs::OpenFlag::Read;
    if (infos[2].non_blocking)
      flags.set(fs::OpenFlag::NoBlock);
    *infos[2].f = fs::File::fromFileDescriptor((u64)infos[2].pipes[0], flags);
    CloseHandle(infos[2].pipes[1]);
  }
  
  CloseHandle(process_info.hThread); // we don't need to track the thread
  
  ERROR("started process ", file, ":", process_info.hProcess, " with args:\n");
  for (String s : args)
  {
    ERROR(s, "\n");
  }
  
  *out_handle = (Process::Handle)process_info.hProcess;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 stopProcess(Process::Handle handle, s32 exit_code)
{
  assert((HANDLE)handle != INVALID_HANDLE_VALUE);
  
  if (processCheck(handle, nullptr) != ProcessCheckResult::StillRunning)
    return false;
  
  if (TerminateProcess((HANDLE)handle, (UINT)exit_code) == 0)
  {
    ERROR("failed to stop process with handle '", handle, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  CloseHandle((HANDLE)handle);
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processSpawnPTY(
  Process::Handle* out_handle,
  String file,
  Slice<String> args,
  fs::File* stream)
{
  assert(out_handle != nullptr);
  assert(notnil(file));
  assert(stream != nullptr);
  
  u64 file_chars = file.countCharacters();
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
      ERROR("failed to spawn pty process '", file, "': insufficient memory"
        " when converting from UTF-8 to WideChar\n");
      return false;
    }
  }
  defer{ if (wargs != win32_str) mem::stl_allocator.free(wargs); };
  
  // convert the utf8 file to wchar
  wargs[0] = L'"';
  assert(file_chars <= INT_MAX);
  int file_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    (LPCCH)file.ptr, (int)file.len, (wargs + 1), (int)cmd_chars);
  if (file_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to spawn pty process '", file, "': invalid unicode was found"
      " in a string when converting from UTF-8 to WideChar\n");
    return false;
  }
  wargs[file_chars] = L'"';
  
  // convert the utf8 args to wchar
  u64 args_offset = file_chars;
  for (int i = 0; i < args.len + 1; i += 1)
  {
    assert(args_offset < cmd_chars);
    wargs[args_offset++] = L' ';
    wargs[args_offset++] = L'"';
    
    int arg_bytes = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      (LPCCH)args[i].ptr, (int)args[i].len, (wargs + args_offset),
      (int)(cmd_chars - args_offset));
    if (arg_bytes == 0)
    {
      assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
      ERROR("failed to spawn pty process '", file, "': invalid unicode was"
        " found in a string when converting from UTF-8 to WideChar\n");
      return false;
    }
    
    args_offset += arg_bytes;
    wargs[args_offset++] = L'"';
  }
  assert(args_offset == cmd_chars);
  wargs[cmd_chars] = L'\0';
  
  // create pipes to which the pty will connect
  HANDLE pipe_in;
  HANDLE pipe_out;
  HANDLE pipe_in_pty;
  HANDLE pipe_out_pty;
  if (CreatePipe(&pipe_in_pty, &pipe_out, NULL, 0) == 0)
  {
    ERROR("failed to open pipes for the stdin stream when trying to spawn"
      " pty process '", file, "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  if (CreatePipe(&pipe_in, &pipe_out_pty, NULL, 0) == 0)
  {
    ERROR("failed to open pipes for the stdin stream when trying to spawn"
      " pty process '", file, "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  HPCON console_handle;
  CONSOLE_SCREEN_BUFFER_INFO console_buffer_info = {};
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  GetConsoleScreenBufferInfo(stdout_handle, &console_buffer_info);
  
  // create the pty
  if (CreatePseudoConsole(console_buffer_info.dwSize, pipe_in_pty, pipe_out_pty,
    0, &console_handle) != S_OK)
  {
    ERROR("failed to create the pty when trying to spawn pty process '", file,
      "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  // no longer need the pty-end of the pipes in the parent process
  CloseHandle(pipe_out_pty);
  CloseHandle(pipe_in_pty);
  
  size_t attribute_list_size = 0;
  if (InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) == 0)
  {
    ERROR("failed to spawn pty process '", file,
      "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  STARTUPINFOEXW startup_info = {};
  startup_info.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  startup_info.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)
    mem::stl_allocator.allocate(attribute_list_size);
  if (startup_info.lpAttributeList == nullptr)
  {
    ERROR("failed to spawn pty process '", file, "': insufficient memory\n");
    return false;
  }
  defer{ mem::stl_allocator.free(startup_info.lpAttributeList); };
  
  if (InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0,
    &attribute_list_size) == 0)
  {
    ERROR("failed to spawn pty process '", file,
      "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  defer{ DeleteProcThreadAttributeList(startup_info.lpAttributeList); };
  
  if (UpdateProcThreadAttribute(startup_info.lpAttributeList, 0,
    PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console_handle,
    sizeof(console_handle), NULL, NULL) == 0)
  {
    ERROR("failed to spawn pty process '", file,
      "': ", makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  // create the process for the pty
  PROCESS_INFORMATION process_info;
  if (CreateProcessW(NULL, wargs, NULL, NULL, FALSE,
    EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &startup_info.StartupInfo,
    &process_info) == 0)
  {
    ERROR("failed to spawn process '", file, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  *out_handle = (Process::Handle)console_handle;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 stopProcessPTY(Process::Handle handle, s32 exit_code)
{
  assert((HPCON)handle != INVALID_HANDLE_VALUE);
  
  ClosePseudoConsole((HPCON)handle);
  return true;
}

/* ----------------------------------------------------------------------------
 */
ProcessCheckResult processCheck(Process::Handle handle, s32* out_exit_code)
{
  assert(handle != INVALID_HANDLE_VALUE);
  
  DWORD exit_code;
  if (GetExitCodeProcess((HANDLE)handle, &exit_code) == 0)
  {
    ERROR("failed to check process with handle '", handle, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return ProcessCheckResult::Error;
  }
  
  if (exit_code == STILL_ACTIVE)
    return ProcessCheckResult::StillRunning;
  
  if (out_exit_code)
    *out_exit_code = exit_code;
  
  return ProcessCheckResult::Exited;
}

/* ----------------------------------------------------------------------------
 */
b8 realpath(fs::Path* path)
{
  assert(path != nullptr);
  
  wchar_t* wpath = makeWin32Path(path->buffer.asStr());
  if (wpath == nullptr)
  {
    ERROR("failed to canonicalize path '", path, "': ", win32_make_path_error);
    return false;
  }
  defer{ if (wpath != win32_str) mem::stl_allocator.free(wpath); };
  
  DWORD full_wpath_chars = GetFullPathNameW(wpath, 0, NULL, NULL);
  if (full_wpath_chars == 0)
  {
    ERROR("failed to canonicalize path '", *path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  wchar_t* full_wpath = (wchar_t*)mem::stl_allocator.allocate(full_wpath_chars);
  if (full_wpath == nullptr)
  {
    ERROR("failed to canonicalize path '", *path, "': insufficient memory when"
      " converting from UTF-8 to WideChar\n");
    return false;
  }
  defer{ mem::stl_allocator.free(full_wpath); };
  
  full_wpath_chars = GetFullPathNameW(wpath, (DWORD)full_wpath_chars,
    full_wpath, NULL);
  if (full_wpath_chars == 0)
  {
    ERROR("failed to canonicalize path '", *path, "': ",
      makeWin32ErrorMsg(GetLastError()), "\n");
    cleanupWin32ErrorMsg();
    return false;
  }
  
  assert(full_wpath_chars <= (DWORD)INT_MAX);
  int utf8_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
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
  
  int converted_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
    full_wpath, (int)full_wpath_chars, (char*)path->buffer.ptr,
    utf8_bytes + 1, NULL, NULL);
  if (converted_bytes == 0)
  {
    assert(GetLastError() == ERROR_NO_UNICODE_TRANSLATION);
    ERROR("failed to canonicalize path '", *path, "': invalid unicode was"
      " found in a string when converting from WideChar to UTF-8\n");
    return false;
  }
  
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
  int utf8_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
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
  
  int converted_bytes = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS,
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
  
  return path;
}

/* ----------------------------------------------------------------------------
 */
b8 chdir(String path)
{
  assert(notnil(path));
  
  wchar_t* wpath = makeWin32Path(path);
  if (wpath == nullptr)
  {
    ERROR("failed to change the working directory to '", path, "': ",
      win32_make_path_error);
    return false;
  }
  
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
