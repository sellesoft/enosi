/*
 *  Counted view over a contiguous array of some type.
 *
 *  I feel like typically this should be assumed not to own data that its pointing to.
 */

#ifndef _iro_slice_h
#define _iro_slice_h

#include "common.h"

namespace iro
{

template<typename T>
struct Slice
{
	T*  ptr;
	u64 len;

	T* begin() { return ptr; }
	T* end()   { return ptr + len; }
};

typedef Slice<u8> Bytes;

}

#endif // _iro_slice_h
