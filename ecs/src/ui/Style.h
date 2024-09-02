/*
 *  Definition of parameters for styling ui Items.
 *
 *  This can be thought of as the CSS of the ui system.
 */

#ifndef _ecs_ui_style_h
#define _ecs_ui_style_h

#include "../math/vec.h"

namespace ui
{

enum class Positioning
{
  Static,
  Relative,
  Absolute,
  Fixed,
};

enum class Anchor
{
  TopLeft,
  TopRight,
  BottomRight,
  BottomLeft,
};

enum class Sizing
{
  Normal,
};

struct Style
{
  Positioning positioning;
  Anchor anchor;
  Sizing sizing;

  vec2f pos;
  vec2f size;
  
  vec4f margin;
  vec4f paddng;

  vec2f scale;
  vec2f scroll;
};

}

#endif
