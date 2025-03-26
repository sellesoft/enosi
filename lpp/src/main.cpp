#include "Driver.h"

#include "iro/Common.h"
#include "iro/fs/FileSystem.h"
#include "iro/Logger.h"
#include "iro/Platform.h"
#include "iro/containers/SmallArray.h"

#if IRO_LINUX
#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");
#endif

using namespace lpp;

int main(int argc, const char** argv) 
{
  iro::log.init();
  defer { iro::log.deinit(); };

  Logger logger;
  logger.init("lpp"_str, Logger::Verbosity::Trace);

  {
    using enum Log::Dest::Flag;
    Log::Dest::Flags flags;

    if (fs::stdout.isatty())
    {
      flags = 
          AllowColor
        | ShowVerbosity
        | PrefixNewlines;
    }
    else
    {
      flags = 
          ShowVerbosity
        | PrefixNewlines;
    }

    iro::log.newDestination("stderr"_str, &fs::stderr, flags);
  }

  SmallArray<String, 8> args;
  for (s32 i = 1; i < argc; ++i)
    args.push(String::fromCStr(argv[i]));

  Driver::InitParams driver_params = {};

  Driver driver;
  if (!driver.init(driver_params))
    return 1;
  defer { driver.deinit(); };

  switch (driver.processArgs(args.asSlice()))
  {
  case Driver::ProcessArgsResult::Error:
    return 1;

  case Driver::ProcessArgsResult::EarlyOut:
    return 0;
  }

  driver.streams.out.stream = 
    driver.streams.out.stream ?: &fs::stdout;

  Lpp lpp = {}; 
  if (!driver.construct(&lpp))
    return 1;
  defer { lpp.deinit(); };

  if (!lpp.run())
    return 1;

  return 0;
}
