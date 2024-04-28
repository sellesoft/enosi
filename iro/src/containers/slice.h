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
	T*  ptr = nullptr;
	u64 len = 0;

	static Slice<T> invalid() { return {nullptr, 0}; }
	b8 isvalid() { return ptr != nullptr; }

	b8 isempty() { return len == 0; }

	T* begin() { return ptr; }
	T* end()   { return ptr + len; }
};

typedef Slice<u8> Bytes;

}

#endif // _iro_slice_h
