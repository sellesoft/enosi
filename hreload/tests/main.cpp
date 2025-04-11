#include "stdio.h"
#include "dlfcn.h"
#include "sys/mman.h"
#include "string.h"
#include "unistd.h"
#include "errno.h"
#include "link.h"
#include "stdlib.h"

#include "iro/Common.h"
#include "iro/fs/File.h"
#include "iro/Logger.h"
#include "iro/Process.h"
#include "iro/Platform.h"
#include "iro/LuaState.h"

#include "Reloader.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

using namespace iro;

inline Logger logger = Logger::create("hreload"_str, Logger::Verbosity::Info);

int g_x = 100;

void orange(int x)
{
  INFO(g_x += 10, "\n");
}

void banana(int x)
{
  orange(x);
}

void apple(int a, int b, int c)
{
  INFO("a+b", a + b, "\n");
  banana(a+b);
}

struct Thing
{
  int x = 0;

  void update()
  {
    INFO(x, "\n");

    x += 100;
  }
};

b8 doReload(hr::Reloader* r)
{
  fs::stdout.write("press enter to reload...\n"_str);

  u8 dummy[255];
  fs::stdin.read(Bytes::from(dummy, 255));

  io::StaticBuffer<32> patchnumbuf;
  patchnumbuf.open(); 
  io::format(&patchnumbuf, hr::getPatchNumber(r)); 

  String args[2] =
  {
    "patch"_str,
    patchnumbuf.asStr(),
  };

  auto lake = 
    Process::spawn(
      "/home/sushi/src/enosi/bin/lake"_str,
      Slice<String>::from(args, 2),
      "/home/sushi/src/enosi"_str);

  while (lake.status == Process::Status::Running || lake.hasOutput())
  {
    if (lake.hasOutput())
    {
      io::StaticBuffer<255> outbuf;
      outbuf.len = lake.read(Bytes::from(outbuf.buffer, outbuf.capacity()));
      INFO(outbuf);
    }
    lake.check();
  }

  void* dlhandle = dlopen(nullptr, RTLD_LAZY);

  hr::ReloadContext context;
  context.hrfpath = "build/debug/hreload-test.hrf"_str;
  context.exepath = "build/debug/hreload-test"_str;
  context.reloadee_handle = dlhandle;

  hr::ReloadResult result;

  if (!hr::doReload(r, context, &result))
    return ERROR("failed to reload symbols\n");

  return true;
}

void Thing_update(Thing* thing)
{
  thing->update();
}

int main()
{
  iro::log.init();
  defer { iro::log.deinit(); };

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
          ShowCategoryName
        | ShowVerbosity
        | PrefixNewlines;
    }

    iro::log.newDestination("stdout"_str, &fs::stdout, flags);
  }

  void* funcs[] = 
  {
    (void*)&Thing_update,
  };

  auto* reloader = hr::createReloader(Slice<void*>::from(funcs, 1));

  Thing thing;

  for (;;)
  {
    doReload(reloader);
    apple(1,2,300);
    Thing_update(&thing);
  }
}
