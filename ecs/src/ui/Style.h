/*
 *  Definition of parameters for styling ui Items.
 *
 *  This can be thought of as the CSS of the ui system.
 */

#ifndef _ecs_ui_style_h
#define _ecs_ui_style_h

#include "math/vec.h"
#include "Color.h"

#include "iro/flags.h"
#include "iro/memory/memory.h"

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
  AutoX,
  AutoY,
  PercentX,
  PercentY,
  Square,
  Flex,
};
typedef iro::Flags<Sizing> SizingFlags;

enum class Border
{
  None,
  Solid,
};

enum class Display
{
  Horizontal,
  Flex,
  Reverse,
  Hidden,
};
typedef iro::Flags<Display> DisplayFlags;

struct Style
{
  Positioning positioning;
  Anchor anchor;
  SizingFlags sizing;
  DisplayFlags display;

  vec2f pos;
  vec2f size;
  
  union
  {
    vec4f margin;
    struct
    {
      union
      {
        struct { f32 margin_left, margin_top; };
        vec2f margintl;
      };
    };
    struct
    {
      union
      {
        struct { f32 margin_right, margin_bottom; };
        vec2f marginbr;
      };
    };
  };

  union
  {
    vec4f padding;
    struct
    {
      union
      {
        struct { f32 padding_left, padding_top; };
        vec2f paddingtl;
      };
    };
    struct
    {
      union
      {
        struct { f32 padding_right, padding_bottom; };
        vec2f paddingbr;
      };
    };
  };

  vec2f scale;
  vec2f scroll;

  Border border_style;
  union
  {
    vec4f border;
    struct
    {
      union 
      {  
        struct { f32 border_left, border_top; };
        vec2f bordertl;
      };
    };
    struct
    {
      union
      {
        struct { f32 border_right, border_bottom; };
        vec2f borderbr;
      };
    };
  };

  Color background_color;

  Style() { iro::mem::zeroStruct(this); }

  b8 hasBorder() 
  { 
    return 
      border_style != Border::None &&
      (border.x != 0.f ||
       border.y != 0.f ||
       border.z != 0.f ||
       border.w != 0.f);
  }
};

}

#endif
