/*
 *  Wrapper around Lua.
 *
 */

#ifndef _iro_LuaState_h
#define _iro_LuaState_h

#include "Common.h"
#include "Unicode.h"

struct lua_State;

namespace iro
{

namespace io { struct IO; }

/* ============================================================================
 */
struct LuaState
{
  
  lua_State* L;

  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
   =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

  static LuaState fromExistingState(lua_State* L) 
    { LuaState out; out.L = L; return out; }
  
  b8 init();
  void deinit();

  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
   |  Lua API.
   |  All of these functions are just wrappers around the equivalent lua 
   |  functions and mostly behave the same. However, some functions, notably
   |  'pcall' returns a boolean on success or failure that is the opposite of
   |  what lua normally returns, eg. lua_pcall returns 0 on success, but 
   |  the pcall here returns 'true' on success.
   |  TODO(sushi) better document api differences.
   =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

  void pop(s32 count = 1);
  void insert(s32 idx);
  void remove(s32 idx);

  s32  gettop();
  void settop(s32 idx);

  void newtable();
  void gettable(s32 idx);
  void settable(s32 idx);
  void getfield(s32 idx, const char* k);
  void setfield(s32 idx, const char* k);

  void rawgeti(s32 tblidx, s32 idx);

  void setglobal(const char* name);
  void getglobal(const char* name);

  s32 objlen(s32 idx = -1);

  b8 load(io::IO* in, const char* name);
  b8 loadbuffer(Bytes buffer, const char* name);
  b8 loadfile(const char* path);
  b8 loadstring(const char* s);
  b8 dofile(const char* s);
  b8 dostring(const char* s);

  b8 pcall(s32 nargs = 0, s32 nresults = 0, s32 errfunc = 0);

  b8 callmeta(const char* name, s32 idx = -1);
  b8 getmetafield(const char* name, s32 idx = -1);
  b8 getmetatable(s32 idx = -1);

  void getfenv(s32 idx);
  b8   setfenv(s32 idx);

  String tostring(s32 idx = -1);
  f32    tonumber(s32 idx = -1);
  b8     toboolean(s32 idx = -1);
  void*  tolightuserdata(s32 idx = -1);

  template<typename T>
  T* tolightuserdata(s32 idx = -1)
  {
    return (T*)tolightuserdata(idx);
  }

  void pushstring(String s);
  void pushstring(const char* s);
  void pushlightuserdata(void* data);
  void pushinteger(s32 i);
  void pushvalue(s32 idx);
  void pushnil();
  void pushcfunction(int (*cfunc)(lua_State*));
  void pushboolean(b8 v);

  // Concatenates the n values on the top of the stack.
  void concat(s32 n);

  int type(s32 idx);
  const char* typeName(s32 idx);

  b8 next(s32 idx);

  b8 isnil(s32 idx = -1);
  b8 isstring(s32 idx = -1);
  b8 isboolean(s32 idx = -1);
  b8 istable(s32 idx = -1);
  b8 isnumber(s32 idx = -1);

  b8 equal(s32 lhs, s32 rhs);

  b8 dump(io::IO* dest);

  
  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
   |  Helper API
   |  Following this are functions that are not a part of the lua api.
   =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */


  // Call Lua's (or whatever require is available..) require to import 
  // 'modname'.Leaves the return values of the module at the top of the stack.
  // If 'nret' is given, the number of values returned will be written to it.
  b8 require(String modname, u32* out_nret = nullptr);

  void stackDump(u32 max_depth = 1);
  void stackDump(io::IO* out, u32 max_depth = 1);

  // currently-experimental debug hook installer 
  // that im gonna play with for a bit before deciding
  // what i wanna do with it, if anything
  void installDebugHook();

  // iterate over the key-value pairs using 'callback'
  template<typename F>
  void pairs(s32 idx, F callback)
  {
    s32 bottom = gettop();
    defer { settop(bottom); };

    pushvalue(idx);
    pushnil();

    while (next(-2))
    {
      s32 bottom_pre = gettop();
      if (!callback())
        return;
      assert(bottom_pre == gettop() && 
        "the callback must return the stack to the state it was in on entry!");
      pop();
    }
  }

  void tableInsert(s32 table_idx, s32 value_idx);
};

}

/* ============================================================================
 */
extern "C"
{

int iro__lua_inspect(lua_State* L);

}

#endif // _iro_luastate_h
