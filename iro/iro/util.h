/*
 *  Miscellaneous utilities that dont have anywhere else to go.
 *
 *  Ideally this stuff is spread around into more appropriate locations
 *  as time goes on but who knows.
 */

#ifndef _iro_util_h
#define _iro_util_h

#include "common.h"

namespace iro
{

template<typename V, typename... T>
b8 matchAny(V v, T... args) { return ((v == args) || ...); }

}

#endif // _iro_util_h
