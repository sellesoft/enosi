#include "Lpp.h"

#include "Lex.h"
#include "stdio.h"

#include "iro/Logger.h"
#include "iro/fs/FileSystem.h"
#include "iro/ArgIter.h"
#include "iro/Platform.h"

#include "assert.h"

#include "Metaprogram.h"

extern "C"
{
#include "luajit/lua.h"

LPP_LUAJIT_FFI_FUNC
int lua__processFile(lua_State* L);
int lua__getFileFullPathIfExists(lua_State* L);
int lua__debugBreak(lua_State* L);
int lua__getCurrentInputSourceName(lua_State* L);
int lua__getInputName(lua_State* L);
}

namespace lpp
{

static Logger logger = 
  Logger::create("Lpp"_str, 
#if IRO_DEBUG
    Logger::Verbosity::Debug);
#else
    Logger::Verbosity::Notice);
#endif

const char* lpp_metaenv_stack = "__lpp_metaenv_stack";

/* ----------------------------------------------------------------------------
 */
b8 Lpp::init(const InitParams& params)
{
  INFO("init\n");

  DEBUG("creating lua state\n");
  lua.init();

  DEBUG("creating metaenv stack\n");
  lua.newtable();
  lua.setglobal(lpp_metaenv_stack);

  DEBUG("creating pools\n");
  sources.init();
  metaprograms.init();

  DEBUG("loading luajit ffi\n");
  if (!lua.require("CDefs"_str))
  {
    ERROR("failed to load luajit ffi\n");
    return false;
  }

#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lua__processFile);
  addGlobalCFunc(lua__getFileFullPathIfExists);
  addGlobalCFunc(lua__debugBreak);
  addGlobalCFunc(lua__getCurrentInputSourceName);
  addGlobalCFunc(lua__getInputName);

#undef addGlobalCFunc

  DEBUG("loading lpp lua module\n");
  if (!lua.require("Lpp"_str))
    return false;
  const s32 I_lpp = lua.gettop();


  // give lpp module a handle to us
  lua.pushstring("handle"_str);
  lua.pushlightuserdata(this);
  lua.settable(lua.gettop()-2);

  streams = params.streams;
  consumers = params.consumers;

  if (!params.args.isEmpty())
  {
    lua.pushstring("addArgv"_str);
    lua.gettable(I_lpp);
    const s32 I_addArgv = lua.gettop();

    for (String arg : params.args)
    {
      lua.pushvalue(I_addArgv);
      lua.pushstring(arg);
      if (!lua.pcall(1))
        return ERROR(lua.tostring(), "\n");
    }

    lua.pop();
  }

  if (!params.include_dirs.isEmpty())
  {
    lua.pushstring("addIncludeDir"_str);
    lua.gettable(I_lpp);
    const s32 I_addIncludeDir = lua.gettop();

    for (String dir : params.include_dirs)
    {
      lua.pushvalue(I_addIncludeDir);
      lua.pushstring(dir);
      lua.pcall(1);
    }

    lua.pop();
  }

  for (String dir : params.require_dirs)
  {
    lua.getglobal("package");
    lua.pushstring("path"_str);
    lua.gettable(-2);
    lua.pushstring(";"_str);
    lua.pushstring(dir);
    lua.pushstring("/?.lua"_str);
    lua.concat(4);
    lua.pushstring("path"_str);
    lua.pushvalue(-2);
    lua.settable(-4);
    lua.pop(2);
  }

  for (String dir : params.cpath_dirs)
  {
    lua.getglobal("package");
    lua.pushstring("cpath"_str);
    lua.gettable(-2);
    lua.pushstring(";"_str);
    lua.pushstring(dir);
#if IRO_LINUX
    lua.pushstring("/?.so"_str);
#elif IRO_WIN32
    lua.pushstring("/?.dll"_str);
#endif
    lua.concat(4);
    lua.pushstring("cpath"_str);
    lua.pushvalue(-2);
    lua.settable(-4);
    lua.pop(2);
  }

  if (!params.include_dirs.isEmpty())
  {
    lua.pushstring("addIncludeDir"_str);
    lua.gettable(I_lpp);
    const s32 I_addIncludeDir = lua.gettop();

    for (String dir : params.include_dirs)
    {
      lua.pushvalue(I_addIncludeDir);
      lua.pushstring(dir);
      lua.pcall(1);
    }

    lua.pop();
  }

  lua.pop();

  DEBUG("done initializing\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lpp::deinit()
{
  lua.deinit();

  for (auto& source : sources)
    source.deinit();
  sources.deinit();

  for (auto& metaprogram : metaprograms)
    metaprogram.deinit();
  metaprograms.deinit();

  *this = {};
}

/* ----------------------------------------------------------------------------
 */
b8 Lpp::run()
{
  DEBUG("run\n");

  using namespace fs;

  if (!processStream(
        streams.in.name, 
        streams.in.io, 
        streams.out.io))
    return false;

  if (streams.dep.io)
  {
    if (!lua.require("Lpp"_str))
      return false;
    const s32 I_lpp = lua.gettop();

    lua.pushstring("generateDepFile"_str);
    lua.gettable(I_lpp);

    if (!lua.pcall(0, 1))
      return false;

    String result = lua.tostring();

    io::format(streams.dep.io, result);
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lpp::processStream(String name, io::IO* instream, io::IO* outstream)
{
  DEBUG("creating metaprogram from input stream '", name, "'\n");

  Source* source = sources.pushTail()->data;
  if (!source->init(name))
    return false;
  defer { source->deinit(); };

  Source* dest = sources.pushTail()->data;
  if (!dest->init("dest"_str)) // TODO(sushi) dont use 'dest' here.
    return false;
  defer { dest->deinit(); };

  Metaprogram* prev = (metaprograms.isEmpty()? nullptr : &metaprograms.tail());
  Metaprogram* metaprog = metaprograms.pushTail()->data;
  if (!metaprog->init(this, instream, source, dest, prev))
    return false;
  defer { metaprog->deinit(); metaprograms.popTail(); };

  if (!metaprog->run())
    return false;

  if (outstream)
    outstream->write(metaprog->output->cache.asStr());
  
  return true;
}

/* ============================================================================
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

/* ----------------------------------------------------------------------------
 */
int lua__processFile(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  Lpp* lpp = lua.tolightuserdata<Lpp>(1);
  auto reqpath = lua.tostring(2);
  auto path = fs::Path::from(reqpath);
  defer { path.destroy(); };

  if (!path.makeAbsolute())
  {
    FATAL("failed to make path ", path, " absolute\n");
    platform::exit(1);
  }

  if (!lua.require("Lpp"_str))
    return 0;
  const s32 I_lpp = lua.gettop();

  lua.pushstring("addDependency"_str);
  lua.gettable(I_lpp);

  lua.pushstring(path.buffer.asStr());
  if (!lua.pcall(1))
  {
    ERROR("failed to add dependency on ", path, "\n");
    ERROR(lua.tostring(), "\n");
    return 0;
  }

  auto f = fs::File::from(path, fs::OpenFlag::Read);
  if (isnil(f))
    return {};
  defer { f.close(); };

  io::Memory mem;
  mem.open();
  defer { mem.close(); };

  if (!lpp->processStream(reqpath, &f, &mem))
    return 0;

  lua.pushstring(mem.asStr());
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__fileExists(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  String path = lua.tostring(1);
  lua.pushboolean(
      fs::Path::exists(path) &&
      fs::Path::isRegularFile(path));
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__getFileFullPathIfExists(lua_State* L)
{
  using namespace fs;
  auto lua = LuaState::fromExistingState(L);
  auto pathstr = lua.tostring(1);
  if (Path::exists(pathstr) &&
      Path::isRegularFile(pathstr))
  {
    auto path = fs::Path::from(lua.tostring(1));
    defer { path.destroy(); };
    if (!path.makeAbsolute())
    {
      FATAL("failed to make path ", path, "absolute!\n");
      platform::exit(1);
    }
    lua.pushstring(path.buffer.asStr());
    return 1;
  }
  return 0;
}

/* ----------------------------------------------------------------------------
 */
int lua__getCurrentInputSourceName(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto lpp = (Lpp*)lua.tolightuserdata(1);

  lua.pushstring(lpp->metaprograms.tail().input->name);
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__getInputName(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto lpp = (Lpp*)lua.tolightuserdata(1);

  lua.pushstring(lpp->streams.in.name);
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__debugBreak(lua_State* L)
{
  platform::debugBreak();
  return 0;
}

}

}
