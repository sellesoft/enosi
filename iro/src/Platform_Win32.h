/*
 *  Shared utilities for dealing with Win32 stuff.
 *
 *  NOTE that this cannot be included after Logger.h, otherwise ERROR 
 *       will be undefined. Really need to rework logging at some point.
 */ 

#if IRO_WIN32
#ifndef __iro_PlatformWin32_h
#define __iro_PlatformWin32_h

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#undef ERROR
#undef max
#undef min
#undef interface

#include "Common.h"
#include "Unicode.h"
#include "io/Format.h"

namespace iro::platform
{

/* ============================================================================
 *  Scoped error message.
 */
struct Win32ErrMsg
{
  LPSTR msg;
  DWORD len;

  Win32ErrMsg() : Win32ErrMsg(GetLastError()) {}

  Win32ErrMsg(DWORD error)
  {
    len = 
      FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER 
          | FORMAT_MESSAGE_FROM_SYSTEM 
          | FORMAT_MESSAGE_IGNORE_INSERTS, 
          NULL, 
          error,
          MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), 
          (LPSTR)&msg, 
          0, 
          NULL);
  }

  ~Win32ErrMsg()
  {
    LocalFree(msg);
  }
};

}

#define ERROR_WIN32(...) ERROR(__VA_ARGS__, ": ", iro::platform::Win32ErrMsg())

namespace iro::io
{

static s64 format(IO* io, platform::Win32ErrMsg& err)
{
  return io::format(io, String::from((u8*)err.msg, err.len));
}

}

#endif
#endif
