/* 
 *  Small helper for defining a static array that is indexed by a C++ enum 
 *  class.
 */

#ifndef _iro_EnumArray_h
#define _iro_EnumArray_h

#include "../Common.h"

template<typename TEnum, typename TElem>
struct EnumArray
{
  TElem arr[u32(TEnum::COUNT)] = {};

  TElem& operator[](TEnum i) { return arr[(u32)i]; }
  const TElem& operator[](TEnum i) const { return arr[(u32)i]; }

  TElem* begin() { return arr; }
  TElem* end() { return arr + u32(TEnum::COUNT); }

  const TElem* begin() const { return arr; }
  const TElem* end() const { return arr + u32(TEnum::COUNT); }
};


#endif
