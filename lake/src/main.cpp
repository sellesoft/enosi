#include "Lake.h"
#include "stdlib.h"

#include "iro/Logger.h"
#include "iro/Platform.h"
#include "iro/fs/Glob.h"
#include "iro/fs/Path.h"

using namespace iro;

#if IRO_LINUX
#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");
#endif // #if IRO_LINUX

#include "stdio.h"
#undef stdout

int main(const int argc, const char* argv[]) 
{
  if (!iro::log.init())
    return 1;
  defer { iro::log.deinit(); };

  iro::platform::makeDir("temp"_str, false);
  auto f = 
    fs::File::from("temp/log"_str, 
          fs::OpenFlag::Create 
        | fs::OpenFlag::Truncate 
        | fs::OpenFlag::Write);
  defer { if (notnil(f)) f.close(); };

  {
    using enum Log::Dest::Flag;
    iro::log.newDestination("stdout"_str, &fs::stdout, 
          AllowColor 
        | ShowCategoryName 
        | ShowVerbosity 
        | PadVerbosity 
        | TrackLongestName
        | PrefixNewlines);

    if (notnil(f))
      iro::log.newDestination("templog"_str, &f, {});
  }

  Lake lake;

  if (!lake.init(argv, argc))
    return 1;
  defer { lake.deinit(); };

  if (!lake.run())
    return 1;
  return 0;
}
