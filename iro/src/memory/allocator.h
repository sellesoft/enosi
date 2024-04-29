/*
 *  Allocator interface.
 */ 

#ifndef _iro_allocator_h
#define _iro_allocator_h

#include "../common.h"

namespace iro::mem
{

struct Allocator
{
	virtual void* allocate(u64 size) = 0;
	virtual void* reallocate(void* ptr, u64 size) = 0;
	virtual void  free(void* ptr) = 0;
};

struct STLAllocator : Allocator
{
	void* allocate(u64 size) override;
	void* reallocate(void* ptr, u64 size) override;
	void  free(void* ptr) override;
};

extern STLAllocator stl_allocator;

}

#endif // _iro_allocator_h
