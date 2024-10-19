/*
 *  Small helper for iterating over command line arguments.
 *
 */

#ifndef _iro_argiter_h
#define _iro_argiter_h

#include "common.h"
#include "unicode.h"

namespace iro
{

struct ArgIter
{
  const char** argv = nullptr;
  u32 argc = 0;
  u32 idx = 0;

  String current = nil;

  ArgIter(const char** argv, u32 argc) : argv(argv), argc(argc)
  {
    idx = 1;
    next();
  }

  void next()
  {
    if (idx == argc)
    {
      current = nil;
      return;
    }

    current = String::fromCStr(argv[idx++]);
  }
};

}

#endif // _iro_argiter_h
