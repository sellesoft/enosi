#include "luastate.h"

#include "logger.h"
#include "unicode.h"
#include "fs/file.h"
#include "containers/slice.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#undef stdin
#undef stdout
#undef stderr

namespace iro
{

Logger logger = Logger::create("luastate"_str, Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
b8 LuaState::init()
{
  L = lua_open();
  luaL_openlibs(L);

#define addGlobalCFunc(name) \
  pushcfunction(name); \
  setglobal(STRINGIZE(name));

  addGlobalCFunc(iro__lua_inspect);

#undef addGlobalCFunc

  if (!require("cdefs"_str))
    return ERROR("failed to load cdefs module\n");
  pop(); // Pop the boolean that will be left on the stack.

  return true;
}

/* ----------------------------------------------------------------------------
 */
void LuaState::deinit()
{
  lua_close(L);
  L = nullptr;
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pop(s32 count)
{
  lua_pop(L, count);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::insert(s32 idx)
{
  lua_insert(L, idx);
}

/* ----------------------------------------------------------------------------
 */
s32 LuaState::gettop()
{
  return lua_gettop(L);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::settop(s32 idx)
{
  return lua_settop(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::newtable()
{
  lua_newtable(L);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::settable(s32 idx)
{
  lua_settable(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::gettable(s32 idx)
{
  lua_gettable(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::getfield(s32 idx, const char* k)
{
  lua_getfield(L, idx, k);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::rawgeti(s32 tblidx, s32 idx)
{
  lua_rawgeti(L, tblidx, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::setglobal(const char* name)
{
  lua_setglobal(L, name);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::getglobal(const char* name)
{
  lua_getglobal(L, name);
}

/* ----------------------------------------------------------------------------
 */
s32 LuaState::objlen(s32 idx)
{
  return lua_objlen(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::load(io::IO* in, const char* name)
{
  static const s64 reader_buffer_size = 128;
  u8 reader_buffer[reader_buffer_size];
  struct D { u8* buffer; io::IO* io; } d {reader_buffer, in};

  auto reader = [](lua_State* L, void* data, size_t* size) -> const char*
  {
    D* d = (D*)data;
    *size = d->io->read({d->buffer, reader_buffer_size});
    return (const char*)d->buffer;
  };

  if (lua_load(L, reader, &d, name))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::loadbuffer(Bytes buffer, const char* name)
{
  auto loader = [](lua_State* L, void* data, size_t* size) -> const char* 
  {
    String* buffer = (String*)data;
    if (buffer->isEmpty())
      return nullptr;
    *size = buffer->len;
    buffer->len = 0;
    return (char*)buffer->ptr;
  };

  if (lua_load(L, loader, &buffer, name))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::loadfile(const char* path)
{
  if (luaL_loadfile(L, path))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::loadstring(const char* s)
{
  if (luaL_loadstring(L, s))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::dofile(const char* s)
{
  if(luaL_dofile(L, s))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::dostring(const char* s)
{
  if (luaL_dostring(L, s))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::pcall(s32 nargs, s32 nresults, s32 errfunc)
{
  if (lua_pcall(L, nargs, nresults, errfunc))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::callmeta(const char* name, s32 idx)
{
  if (!luaL_callmeta(L, idx, name))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
void LuaState::getfenv(s32 idx)
{
  lua_getfenv(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::setfenv(s32 idx)
{
  if (!lua_setfenv(L, idx))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
String LuaState::tostring(s32 idx)
{
  size_t len;
  const char* s = lua_tolstring(L, idx, &len);
  return {(u8*)s, len};
}

/* ----------------------------------------------------------------------------
 */
f32 LuaState::tonumber(s32 idx)
{
  return lua_tonumber(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::toboolean(s32 idx)
{
  return lua_toboolean(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void* LuaState::tolightuserdata(s32 idx)
{
  return lua_touserdata(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushstring(String s)
{
  // TODO(sushi) handle non-temrinated input
  lua_pushlstring(L, (char*)s.ptr, s.len);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushlightuserdata(void* data)
{
  lua_pushlightuserdata(L, data);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushinteger(s32 i)
{
  lua_pushinteger(L, i);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushvalue(s32 idx)
{
  lua_pushvalue(L, idx);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushnil()
{
  lua_pushnil(L);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushcfunction(int (*cfunc)(lua_State*))
{
  lua_pushcfunction(L, cfunc);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::pushboolean(b8 v)
{
  lua_pushboolean(L, v);
}

/* ----------------------------------------------------------------------------
 */
void LuaState::concat(s32 n)
{
  lua_concat(L, n);
}

/* ----------------------------------------------------------------------------
 */
int LuaState::type(s32 idx)
{
  return lua_type(L, idx);
}

/* ----------------------------------------------------------------------------
 */
const char* LuaState::typeName(s32 idx)
{
  return lua_typename(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::next(s32 idx)
{
  return lua_next(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::isnil(s32 idx)
{
  return lua_isnil(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::isstring(s32 idx)
{
  return lua_isstring(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::isboolean(s32 idx)
{
  return lua_isboolean(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::istable(s32 idx)
{
  return lua_istable(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::isnumber(s32 idx)
{
  return lua_isnumber(L, idx);
}

/* ----------------------------------------------------------------------------
 */
b8 LuaState::dump(io::IO* dest)
{
  auto writer = [](lua_State* L, const void* p, size_t sz, void* ud) ->int
  {
    io::IO* dest = (io::IO*)ud;

    if (!dest->write({(u8*)p, sz}))
      return 1;
    return 0;
  };

  if (lua_dump(L, writer, dest))
    return false;
  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 LuaState::require(String modname, u32* out_ret)
{
  u32 top = gettop();

  getglobal("require");
  pushstring(modname);
  if (!pcall(1, LUA_MULTRET))
  {
    ERROR("failed to require module '", modname, "':\n", tostring());
    return false;
  }

  if (out_ret)
    *out_ret = gettop() - top;

  return true;
}

/* ----------------------------------------------------------------------------
 */
static void writeLuaValue(LuaState* L, io::IO* dest, int idx)
{
  int t = L->type(idx);

  switch (t)
  {
  case LUA_TTABLE:
    ERROR("writeLuaValue encountered a table, but this should have been "
          "passed to 'writeLuaTable'!\n");
    break;
  case LUA_TNIL:
    io::format(dest, "nil");
    break;
  case LUA_TNUMBER:
    io::format(dest, L->tonumber(idx));
    break;
  case LUA_TSTRING:
    io::formatv(dest, 
        '"', io::SanitizeControlCharacters(L->tostring(idx)), '"');
    break;
  case LUA_TBOOLEAN:
    io::format(dest, (L->toboolean(idx)? "true" : "false"));
    break;
  case LUA_TTHREAD:
    io::format(dest, "<lua thread>");
    break;
  case LUA_TFUNCTION:
    io::format(dest, "<lua function>");
    break;
  case LUA_TUSERDATA:
    io::format(dest, "<lua userdata>");
    break;
  case LUA_TLIGHTUSERDATA:
    io::format(dest, "<lua lightuserdata>");
    break;
  default:
    io::formatv(dest, 
        "<unhandled type in writeLuaValue: ", L->typeName(t), ">");
    break;
  }
}

/* ----------------------------------------------------------------------------
 */
static void writeLuaTable(
    LuaState* L, 
    io::IO*   dest, 
    int       idx, 
    u32       layers, 
    u32       max_depth)
{
  auto indent = [layers, dest](s32 offset)
  {
    for (s32 i = 0; i < layers+offset; i++) 
      io::format(dest, "   ");
  };

  io::format(dest, '\n');
  indent(-1);
  io::format(dest, "{\n");
  defer { indent(-1); io::format(dest, "}\n"); };


  L->pushvalue(idx);
  L->pushnil();

  while (L->next(-2))
  {
    int vt = L->type(-1);

    indent(0);

    writeLuaValue(L, dest, -2);

    io::format(dest, " = ");

    if (vt == LUA_TTABLE)
    {
      if (layers >= max_depth)
        io::format(dest, "...\n");
      else
      {
        writeLuaTable(L, dest, -1, layers + 1, max_depth);
        io::format(dest, '\n');
      }
    }
    else
    {
      writeLuaValue(L, dest, -1);
      io::format(dest, '\n');
    }
    L->pop();
  }
  L->pop();
}

/* ------------------------------------------------------------------------------------------------
 */
void LuaState::stackDump(io::IO* dest, u32 max_depth)
{
  s32 top = gettop();

  for (s32 i = 1; i <= top; i++)
  {
    int t = type(i);
    io::formatv(dest, "i ", i, ": ");
    if (t == LUA_TTABLE)
      writeLuaTable(this, dest, i, 1, max_depth);
    else
      writeLuaValue(this, dest, i);
    io::format(dest, '\n');
  }
}

void LuaState::stackDump(u32 max_depth)
{
  stackDump(&fs::stdout, max_depth);
}

/* ------------------------------------------------------------------------------------------------
 */
void LuaState::installDebugHook()
{
  auto hook = [](lua_State* L, lua_Debug* ar) -> void
  {
    lua_getinfo(L, "nSlu", ar);

    if (ar->event == LUA_HOOKCALL)
      INFO("calling ");
    else
      INFO("leaving ");

    switch (cStrHash(ar->what))
    {
    case "main"_hashed:
      INFO("main chunk\n");
      break;

    case "Lua"_hashed:
      INFO("lua function\n");
      break;

    case "C"_hashed:
      INFO("C function\n");
      break;

    case "tail"_hashed:
      INFO("tail call\n");
      break;
    }

    if (ar->name)
    {
      INFO("function name: ", ar->name, "\n");
    }
  };

  if (lua_sethook(L, hook, LUA_MASKCALL | LUA_MASKRET, 0))
  {
    ERROR("failed to install debug hook!\n");
  }
}

/* ------------------------------------------------------------------------------------------------
 */
void LuaState::tableInsert(s32 table_idx, s32 value_idx)
{
  getglobal("table");
  const s32 I_table = gettop();
  pushstring("insert"_str);
  gettable(I_table);
  const s32 I_table_insert = gettop();
  pushvalue(table_idx);
  pushvalue(value_idx);
  if (!pcall(2, 0))
    ERROR("failed to insert value into table:\n", tostring(), "\n");
  pop();
}

/* ============================================================================
 */
extern "C"
{

/* ----------------------------------------------------------------------------
 *  Used by utils.lua to dump lua values.
 *
 *  params:
 *    1: the value to print
 *    2: the max depth to print tables (optional, default 1)
 */
EXPORT_DYNAMIC
int iro__lua_inspect(lua_State* L)
{
  LuaState lua = LuaState::fromExistingState(L);

  if (lua.isnil(1))
  {
    ERROR("iro_lua_inspect(): expected a value as first argument \n");
    return 0;
  }

  s32 max_depth = 1;
  if (lua.isnumber(2))
    max_depth = lua.tonumber(2);

  if (lua.istable(1))
  {
    writeLuaTable(&lua, &fs::stdout, 1, 1, max_depth);
  }
  else
  {
    writeLuaValue(&lua, &fs::stdout, 1);
  }

  return 0;
}

}

}
