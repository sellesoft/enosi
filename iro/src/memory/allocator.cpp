#include "allocator.h"
#include "stdlib.h"

namespace iro::mem
{

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
