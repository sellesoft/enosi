#include "iro/Common.h"
#include "iro/Unicode.h"
#include "iro/fs/File.h"
#include "iro/LuaState.h"
#include "iro/Logger.h"
#include "iro/GDBScriptDef.h"

using namespace iro;

int main(int argc, const char** argv)
{
  iro::log.init();
  defer { iro::log.deinit(); };

  Logger logger;
  logger.init("lamu"_str, Logger::Verbosity::Trace);

  {
    using enum Log::Dest::Flag;
    Log::Dest::Flags flags;

    if (fs::stderr.isatty())
    {
      flags = 
          AllowColor
        | ShowCategoryName
        | ShowVerbosity
        | TrackLongestName
        | PadVerbosity
        | PrefixNewlines;
    }
    else
    {
      flags = 
          ShowCategoryName
        | ShowVerbosity
        | PrefixNewlines;
    }

    iro::log.newDestination("stderr"_str, &fs::stderr, flags);
  }

  LuaState lua = {};
  if (!lua.init())
    return 1;
  defer { lua.deinit(); };

  if (!lua.require("errhandler"_str))
    return !ERROR("failed to require errhandler\n");
  const s32 I_errhandler = lua.gettop();

  if (!lua.require("lamu"_str))
    return !ERROR("failed to load lamu\n", lua.tostring(), "\n");

//  if (!lua.loadfile("lamu.lua"))
//    return !ERROR("failed to load lamu.lua:\n", lua.tostring(), "\n");

  // if (!lua.pcall(0,0,I_errhandler))
  // {
  //   if (lua.isstring())
  //     return !ERROR("script failed: ", lua.tostring(), "\n");
  // }

  return 0;
}
