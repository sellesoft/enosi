/* 
 */

#ifndef _iro_enumarray_h
#define _iro_enumarray_h

#include "../common.h"

template<typename TEnum, typename TElem>
struct EnumArray
{
  TElem arr[u32(TEnum::COUNT)];

  TElem& operator[](TEnum i) { return arr[(u32)i]; }
};


#endif
