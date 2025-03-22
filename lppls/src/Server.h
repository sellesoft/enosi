/*
 *  Type implementing a language server for lpp.
 */

#ifndef _lppls_Server_h
#define _lppls_Server_h

#include "iro/Common.h"
#include "iro/LuaState.h"
#include "iro/Process.h"
#include "iro/fs/File.h"

using namespace iro;

/* ============================================================================
 */
struct Server
{
  // Lua state that handles the bulk of work needed for dealing with lsp.
  LuaState lua;

  fs::File log_file;

  Process luals;

  // TODO(sushi) this will need to be made agnostic of the document language
  //             but for now, since all I use lpp for is C++, and no one else
  //             is using it.. I'm going to cheat a little.
  Process clangd;
  // This sucks but clangd outputs info over stderr and as far as I know 
  // doesn't have a way to redirect it to a file.
  fs::File clangd_log;

  b8 init();
  void deinit();

  b8 loop();

  b8 processFile();

  enum class ClientKind : u8
  { 
    Lua,
    Document,
  };

  b8 sendServerMessage();
  b8 readServerMessage();
  b8 sendClientMessage();

  struct
  {
    s32 server;
    s32 processMessage;
    s32 processFromQueue;
    s32 pollSubServers;
    s32 exit;
  } I;
};

#endif
