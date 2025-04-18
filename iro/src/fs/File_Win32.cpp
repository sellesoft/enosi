#if IRO_WIN32

/*
 *  We must define these variables here because Windows does not use 
 *  the same std I/O file handle numbers Linux 
 */

#include "File.h"

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#undef ERROR
#undef max
#undef min
#undef interface

namespace iro::fs
{

File stdout = 
  File::fromFileDescriptor(
    (fs::File::Handle)GetStdHandle(STD_OUTPUT_HANDLE), 
    "stdout"_str, 
    OpenFlag::Write);

File stderr = 
  File::fromFileDescriptor(
    (fs::File::Handle)GetStdHandle(STD_ERROR_HANDLE),
    "stderr"_str,
    OpenFlag::Write);

File stdin = 
  File::fromFileDescriptor(
    (fs::File::Handle)GetStdHandle(STD_INPUT_HANDLE),
    "stdin"_str,
    OpenFlag::Read);

}

#endif
