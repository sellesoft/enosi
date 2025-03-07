/* 
 *  Lpp state.
 */

#ifndef _lpp_Lpp_h
#define _lpp_Lpp_h

#include "Lex.h"
#include "iro/Common.h"
#include "iro/containers/LinkedPool.h"
#include "iro/LuaState.h"


#include "Source.h"
#include "Parser.h"

using namespace iro;

// Use on functions that need to be exposed to luajit's ffi interface.
#define LPP_LUAJIT_FFI_FUNC EXPORT_DYNAMIC

namespace lpp
{

struct Driver;
struct Metaprogram;

/* ============================================================================
 *  Set of consumer interfaces for getting information from lpp's systems 
 *  throughout the preprocessing process.
 */
struct LppConsumers
{
  LexerDiagnosticConsumer* lex_diag_consumer = nullptr;
};

/* ============================================================================
 */
struct Lpp
{
  LuaState lua;

  DLinkedPool<Metaprogram> metaprograms;
  DLinkedPool<Source> sources;

  // A stream which information is read from or written to. The String and
  // IO are not owned by Lpp and are expected to be kept around until it 
  // is deinitialized.
  struct Stream
  {
    String name;
    io::IO* io;
  };

  struct Streams
  {
    Stream in;
    Stream out;
    Stream dep;
    Stream meta;
  };

  Streams streams;

  LppConsumers consumers;

  struct InitParams
  {
    Streams streams;

    // Args passed through to the metaprograms.
    Slice<String> args;

    Slice<String> require_dirs;
    Slice<String> cpath_dirs;
    Slice<String> include_dirs;

    LppConsumers consumers;
  };

  b8   init(const InitParams& params);
  void deinit();

  b8 run();
  b8 processStream(String name, io::IO* instream, io::IO* outstream);
}; 

}

#endif // _lpp_lpp_h
