/*
 *  API for creating and iteracting with OS processes.
 */

#ifndef _iro_process_h
#define _iro_process_h

#include "Common.h"
#include "Unicode.h"
#include "containers/Slice.h"
#include "io/IO.h"

#include "traits/Nil.h"

#include "fs/FileSystem.h"

namespace iro
{

/* ============================================================================
 */
struct Process
{
  typedef void* Handle;

#undef Status
  enum class Status
  {
    Running,
    ExitedNormally,
    ExitedFatally,
    TerminatedEarly,
  };

  Handle handle;
  Status status;
  s32 exit_code;

  static Process spawn(
      String        file, 
      Slice<String> args, 
      String        cwd,
      b8            non_blocking = true);

  b8 hasOutput() const;
  b8 hasErrOutput() const;

  u64 read(Bytes buffer) const;
  u64 readstderr(Bytes buffer) const;
  u64 write(Bytes buffer) const;

  // Checks the status of this Process and updates status/exit_code
  // if it has exited.
  void check();
  
  // Closes this process, terminating it early if it is still running,
  // and cleans up its internal resources.
  b8 close();
};

/* ============================================================================
 */
struct ProcessIO : io::IO
{
  Process& proc;

  ProcessIO(Process& proc) : proc(proc) {}

  s64 read(Bytes buffer) override
  {
    return proc.read(buffer);
  }

  s64 write(Bytes buffer) override
  {
    return proc.write(buffer);
  }
};

}

DefineNilValue(iro::Process, {nullptr}, { return x.handle == nullptr; });
DefineMove(iro::Process, { to.handle = from.handle; });


#endif
