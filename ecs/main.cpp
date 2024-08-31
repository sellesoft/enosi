#include "stdio.h"

#include "window/Window.h"

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
  if (!Window::initializeBackend())
    return 1;

  Window window;
  window.init("hello"_str);

  for (;;) 
  {
    window.update();
  }
}
