/*
 *  Enumeration of keys.
 */

#ifndef _ecs_input_keys_h
#define _ecs_input_keys_h

#include "iro/common.h"
#include "iro/unicode.h"

using namespace iro;

enum class Key
{
  Invalid,

  A, B, C, D, E, F, G, H, I, J, K, L, M, 
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

  N1, N2, N3, N4, N5, N6, N7, N8, N9, N0,

  F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

  Up, Down, Left, Right,

  Escape, Backquote, Tab, CapsLock, Minus, Equals, Backspace, LBracket, 
  RBracket, BackSlash, Semicolon, Apostrophe, Enter, Comma, Period, 
  ForwardSlash, Space, 

  LShift, RShift,
  LCtrl, RCtrl,
  LMeta, RMeta,
  LAlt, RAlt,
  Apps,

  Insert, Delete, Home, End, PageUp, PageDown, PrintScreen, ScrollLock,
  Pause,

  NP0, NP1, NP2, NP3, NP4, NP5, NP6, NP7, NP8, NP9,
  NPMultiply, NPDivide, NPPlus, NPMinus, NPPeriod, NPNumLock,

  COUNT,
};

str getKeyStr(Key key);

enum class Mod
{
  Any,
  None,
  LCtrl, RCtrl,
  LShift, RShift,
  LAlt, RAlt,
};

enum class MouseButton
{
  Invalid,

  Left, Right, Middle,

  COUNT,
};

str getouseButtonStr(MouseButton button);

#endif
