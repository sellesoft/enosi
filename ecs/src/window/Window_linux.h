/*
 *  Shared window structures relevant to Linux.
 */

#ifndef _ecs_window_linux_h
#define _ecs_window_linux_h

#include "../input/Keys.h"

#define Window X11Window
#define Font X11Font
#define Time X11Time
#define KeyCode X11KeyCode
#include "X11/Xlib.h" // TODO(sushi) Wayland implementation, maybe
#include "X11/Xatom.h"
#include "X11/Xresource.h"
#include "X11/cursorfont.h"
#include "X11/Xcursor/Xcursor.h"
#include "X11/Xutil.h"
#include "X11/Xos.h"
#include "X11/extensions/Xrandr.h"
#undef Window
#undef Font
#undef Time
#undef KeyCode

struct X11Stuff
{
  Display* display;
  int screen;
  X11Window root;
  XContext context;

  Cursor default_cursor;
  Cursor hidden_cursor;
};

extern X11Stuff x11;

Key keysymToKey(KeySym k);

#endif
