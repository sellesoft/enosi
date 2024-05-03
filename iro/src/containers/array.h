/*
 *  Growing array
 */

#ifndef _iro_array_h
#define _iro_array_h

#include "../common.h"
#include "../memory/memory.h"
#include "../memory/allocator.h"

#include "assert.h"

namespace iro
{

template<typename T>
struct Array 
{
	struct Header
	{
		s32 len;
		s32 space;
	};

	T* arr;
	mem::Allocator* allocator;

	/* -------------------------------------------------------------------------------------------- create
	 */ 
	static Array<T> create(s32 init_space = 8, mem::Allocator* allocator = &mem::stl_allocator)
	{
		init_space = (8 > init_space ? 8 : init_space);

		Array<T> out = {};
		out.allocator = allocator;

		Header* header = (Header*)allocator->allocate(sizeof(Header) + init_space * sizeof(T));

		header->space = init_space;
		header->len = 0;

		out.arr = (T*)(header + 1);

		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */ 
	void destroy()
	{
		allocator->free(getHeader());
		arr = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- len / space
	 */ 
	s32& len()   { return getHeader()->len; }
	s32& space() { return getHeader()->space; }

	/* -------------------------------------------------------------------------------------------- push
	 */ 
	T* push()
	{
		growIfNeeded(1);

		len() += 1;
		return new (arr + len() - 1) T; 
	}

	void push(const T& x)
	{
		growIfNeeded(1);

		arr[len()] = x;
		len() += 1;
	}

	/* -------------------------------------------------------------------------------------------- pop
	 */ 
	void pop()
	{
		len() -= 1;
	}

	/* -------------------------------------------------------------------------------------------- insert
	 */ 
	void insert(s32 idx, const T& x)
	{
		growIfNeeded(1);

		if (!len()) 
			push(x);
		else
		{
			mem::move(arr + idx + 1, arr + idx, sizeof(T) * (len() - idx));
			len() += 1;
			arr[idx] = x;
		}
	}

	T* insert(s32 idx)
	{
		assert(idx >= 0 && idx <= len());
		if (idx == len())
			return push();

		growIfNeeded(1);
		mem::move(arr + idx + 1, arr + idx, sizeof(T) * (len() - idx));
		len() += 1;
		return new (arr + idx) T;
	}

	/* -------------------------------------------------------------------------------------------- getHeader
	 */ 
	Header* getHeader() { return (Header*)arr - 1; }

	/* -------------------------------------------------------------------------------------------- C++ stuff
	 */ 
	T& operator [](s64 i) { return arr[i]; }

	T* begin() { return arr; }
	T* end()   { return arr + len(); }

	/* -------------------------------------------------------------------------------------------- growIfNeeded
	 */ 
	void growIfNeeded(s32 new_elements)
	{
		Header* header = getHeader();

		if (header->len + new_elements <= header->space)
			return;

		while (header->len + new_elements > header->space)
			header->space *= 2;

		header = (Header*)allocator->reallocate(header, sizeof(Header) + header->space * sizeof(T));
		arr = (T*)(header + 1);
	}
};

}

#endif
