/*
 *  A wrapper around C-arrays that just adds a count and some helper functions.
 */

#ifndef _iro_stackarray_h
#define _iro_stackarray_h

#include "../common.h"
#include "slice.h"

namespace iro
{

/* ================================================================================================ StackArray
 */
template<typename T, s64 Capacity>
struct StackArray
{
	T arr[Capacity] = {};
	u64 len = 0;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- capacity
	 */
	u64 capacity() { return Capacity; }

	/* -------------------------------------------------------------------------------------------- push
	 */
	T* push()
	{
		if (len == Capacity)
			return nullptr;
		return new (arr + len++) T;
	}
	
	/* -------------------------------------------------------------------------------------------- pop
	 */
	void pop()
	{
		if (!len)
			return;
		len -= 1;
	}

	/* -------------------------------------------------------------------------------------------- asSlice
	 */
	Slice<T> asSlice()
	{
		return {arr, len};
	}

};

}

#endif // _iro_stackarray_h
