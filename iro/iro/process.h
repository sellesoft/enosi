/*
 *  API for creating and iteracting with OS processes.
 */

#ifndef _iro_process_h
#define _iro_process_h

#include "common.h"
#include "unicode.h"
#include "containers/slice.h"

#include "traits/nil.h"

#include "fs/fs.h"

namespace iro
{

/* ============================================================================
 */
struct Process
{
  typedef void* Handle;

  enum class Status
  {
    Running,
    ExitedNormally,
    ExitedFatally,
  };

  Handle handle;
  Status status;
  b8 ispty;
  s32 exit_code;

  /* ==========================================================================
   *  A file stream that may replace one of the process' stdio streams.
   */
  struct Stream
  {
    // prevent blocking on reads/writes 
    b8 non_blocking;
    fs::File* file;
  };
  
  // Spawns a new process from 'file', passing 'args' and optionally replacing
  // its stdio streams with the Streams given. If 'cwd' is not nil the child
  // process will be spawned with it as its working directory.
  //
  // A File given in a Stream must be nil, and will be initialized according
  // to its position in the 'streams' array. In position 0, the file will
  // act as a pipe to the process's stdin stream allowing the parent 
  // process to write to it. In position 1 or 2, the file will act as a pipe 
  // to the process's stdout and stderr streams, respectively, allowing the 
  // parent process to read from it. If the given stream is marked as 
  // non_blocking, reading or writing to the child process will be done 
  // in an async manner (eg. if the child process does not have any buffered
  // data on stdout, the parent process wont wait for data like it normally 
  // does)
  static Process spawn(
      String        file, 
      Slice<String> args, 
      Stream        streams[3], 
      String        cwd);

  static Process spawnpty(String file, Slice<String> args, fs::File* stream);

  // Checks the status of this process and sets the status of the process.
  // If the process exits, 'exit_code' is set.
  void checkStatus();
  
  // Stops a running process with the 'exit_code'.
  b8 stop(s32 exit_code);
};

}

DefineNilValue(iro::Process, {nullptr}, { return x.handle == nullptr; });
DefineMove(iro::Process, { to.handle = from.handle; });

#endif
