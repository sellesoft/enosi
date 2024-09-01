/*
 *  A representation of an OS window.
 */

#ifndef _ecs_window_h
#define _ecs_window_h

#include "iro/common.h"
#include "iro/unicode.h"

#include "../math/vec.h"

using namespace iro;

/* ============================================================================
 */
struct Window
{
  str title;

  void* handle;
  void* context;

  vec2i pos;
  vec2i size;

  b8 focused;

  enum class DisplayMode
  {
    Windowed,
    Borderless,
    BorderlessMaximized,
    FullScreen,
  };

  DisplayMode display_mode;

  enum class CursorMode
  {
    Normal,
    FirstPerson,
  };

  CursorMode cursor_mode;

  // Initializes whatever backend is being used to display windows.
  static b8 initializeBackend();

  b8 init(str title);
  void deinit();

  b8 update();
  
  void setCursorPosition(s32 x, s32 y);

  void setDisplayMode(DisplayMode mode);
  void setCursorMode(CursorMode mode);
};

#endif
