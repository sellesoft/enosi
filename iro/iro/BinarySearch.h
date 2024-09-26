/*
 *  Generic binary search function.
 */

#include "common.h"
#include "containers/slice.h"
#include "Pair.h"

namespace iro
{

template<typename A, typename B, typename Compare> 
Pair<s32, b8> binarySearch(
    Slice<A> slice, 
    const B& val, 
    Compare comp) 
{
	if (slice.len == 0) 
    return {0, false};

  s32 l = 0,
      m = 0,
      r = slice.len - 1;

  while (l <= r) 
  {
    m = l + (r - l) / 2;
    auto c = comp(slice[m], val);
    if (!c) 
      return {m, true};
    else if (c < 0) 
      l = m + 1;
    else 
      r = m - 1;
  }

  return {m, false};
}

}
