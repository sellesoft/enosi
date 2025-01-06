/* ----------------------------------------------
 *
 *  lpp state and interface used throughout the 
 *  project.
 *
 *  TODO(sushi) add a lpp api function for quitting execution entirely via a longjmp 
 *              or something. When an error occurs in nested metaprograms 
 *              (like via an import macro or sometinhg) it will currently 
 *              report that every single file failed, whne we only want to show info
 *              about the failing file 
 */

#ifndef _lpp_Lpp_h
#define _lpp_Lpp_h

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

struct Metaprogram;

struct Lpp
{
  LuaState lua;

  DLinkedPool<Metaprogram> metaprograms;
  DLinkedPool<Source> sources;

  b8 initialized;

  String input;
  String output;

  b8  generate_depfile;
  String depfile_output;

  b8 output_metafile;
  String metafile_output;

  // True when we should run in lsp mode (--lsp).
  b8 lsp;
  // Print lua metaprograms (--print-meta)
  b8 print_meta;

  b8   init();
  void deinit();

  b8 run();

  b8 processArgv(int argc, const char** argv);
  b8 processStream(String name, io::IO* instream, io::IO* outstream);
  b8 writeAuxFile(String extension, String text);
}; 

}

#endif // _lpp_lpp_h
