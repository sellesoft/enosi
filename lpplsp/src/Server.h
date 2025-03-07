/*
 *  Type implementing a language server for lpp.
 */

#ifndef _lpp_Server_h
#define _lpp_Server_h

#include "iro/Common.h"
#include "iro/LuaState.h"

using namespace iro;

/* ============================================================================
 */
struct Server
{
  // Lua state that handles the bulk of work needed for dealing with lsp.
  LuaState lua;

  b8 init();
  void deinit();

  b8 loop();

  b8 processFile(String name, String text);

  struct
  {
    s32 server;
    s32 processMessage;
  } I;
};

#endif
