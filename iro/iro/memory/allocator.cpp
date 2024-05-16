#include "allocator.h"
#include "stdlib.h"

#include "../logger.h"

namespace iro::mem
{

// default allocator used by containers and such
STLAllocator stl_allocator;

void* STLAllocator::allocate(u64 size)
{
	return malloc(size);
}

void* STLAllocator::reallocate(void* ptr, u64 size)
{
	return realloc(ptr, size);
}

void STLAllocator::free(void* ptr)
{
	::free(ptr);
}

}
