/*
 *  Caches information from command line arguments and initializes lpp
 *  instances using that information. 
 */

#ifndef _lpp_Driver_h
#define _lpp_Driver_h

#include "Lpp.h"
#include "Metaprogram.h"

#include "iro/Common.h"
#include "iro/Unicode.h"
#include "iro/fs/File.h"
#include "iro/containers/SmallArray.h"

#include "iro/io/IO.h"

using namespace iro;

namespace lpp
{

struct Lpp;
struct Driver;

/* ============================================================================
 */
struct NamedStream
{
  String  name = nil;
  io::IO* stream = nullptr;

  b8 init(String name, io::IO* stream)
  {
    if (notnil(name))
      this->name = name.allocateCopy();
    this->stream = stream;
    return true;
  }

  void deinit()
  {
    if (notnil(name))
    {
      mem::stl_allocator.free(name.ptr);
      name = nil;
    }

    stream = nullptr;

    if (notnil(file))
      file.close();
  }

private:
  friend Driver;

  // When the stream is a file created by the Driver, it is stored here.
  fs::File file;
};

/* ============================================================================
 */
struct Driver
{
  typedef SmallArray<String, 4> StringArray;

  StringArray require_dirs;
  StringArray passthrough_args;
  StringArray cpath_dirs;
  StringArray include_dirs;

  struct
  {
    NamedStream in;
    NamedStream out;
    NamedStream dep;
    NamedStream meta;
  } streams;

  LppConsumers consumers;
  LppVFS* vfs;

  b8 use_full_filepaths = false;

  struct InitParams
  {
    // The streams and their names used to construct lpp. These are optional,
    // and if specified will override any of their respective flags that may
    // appear in the provided args. However, specifying a stream does not 
    // imply that its name must also be specified, if the argument for a stream
    // is used then its name will be set to whatever the arg specifies instead.
    //
    // Note that this is designed this way so that things using lpp as a 
    // library can still use the same compile commands that whatever project
    // its running under is using but still intercept the output streams.
    String  in_stream_name = nil;
    io::IO* in_stream = nullptr;

    String  out_stream_name = nil;
    io::IO* out_stream = nullptr;

    String  dep_stream_name = nil;
    io::IO* dep_stream = nullptr;

    String  meta_stream_name = nil;
    io::IO* meta_stream = nullptr;

    // A set of consumers used to extract information from lpp during 
    // preprocessing.
    LppConsumers consumers;

    LppVFS* vfs;
  };

  b8   init(const InitParams& params);
  void deinit();

  enum class ProcessArgsResult
  {
    // An error occurred during arg processing.
    Error,

    // An argument was used that is valid, but doesn't result in an
    // execution of lpp.
    EarlyOut,

    // We can construct an lpp instance and run it.
    Success,
  };

  // Processes the given arguments and cache information from them for 
  // constructing lpp instances.
  ProcessArgsResult processArgs(Slice<String> args);

  // Constructs an lpp instance from this Driver.
  b8 construct(Lpp* lpp);

  void cleanupAfterFailure();
};

}

#endif
