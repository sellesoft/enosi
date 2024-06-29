#include "term.h"

#include "logger.h"
#include "stdlib.h"
#include "fs/file.h"

#include "notcurses/notcurses.h"

namespace iro
{

static Logger logger = Logger::create("term"_str, Logger::Verbosity::Trace);

void Term::test()
{
#if IRO_NOTCURSES
  struct notcurses* nc;
  notcurses_options opts = {};
  if (!(nc = notcurses_core_init(&opts, nullptr)))
  {
    FATAL("failed to init notcurses\n");
    return;
  }

  auto stdplane = notcurses_stdplane(nc);

  ncplane_options planeopts = {};

  ncplane_set_scrolling(stdplane, true);
  ncplane_set_bg_rgb8(stdplane, 255,0,0);

  u8 col[3] = {};

  for (;;)
  {
    struct { int x,y; } pos;
    notcurses_cursor_yx(nc, &pos.y, &pos.x);

    nccell cell;
    if (rand() % (pos.x + 1) == 0)
    {
      col[0] = 255*rand()%(pos.x + 1);
      col[1] = rand()%(pos.y + 1);
      col[2] = rand()%255;
    }
 
    nccell_set_bg_rgb8(&cell, col[0], col[1], col[2]);
    char c[2] = {char(rand() % 0xfff), 0};

    nccell_load(stdplane, &cell, c);
    ncplane_putc(stdplane, &cell);

    ncinput in;
    if (!notcurses_get_nblock(nc, &in))
      continue;

    if (in.id == 'q')
      break;
  }

  notcurses_stop(nc);
#else
  FATAL("iro was not built with TERM enabled\n");
  exit(1);
#endif
}

}

