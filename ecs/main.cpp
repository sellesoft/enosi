#include "stdio.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"

#include "window/Window.h"
#include "graphics/Renderer.h"

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

int main()
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

  if (!Window::initializeBackend())
    return 1;

  Window window;
  if (!window.init("hello"_str))
    return 1;

  Renderer renderer;
  if (!renderer.init(&window))
    return 1;

  for (;;) 
  {
    window.update();
    renderer.update(&window);
  }
}
