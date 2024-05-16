#include "bump.h"
#include "memory.h"

namespace iro::mem
{

b8 Bump::init()
{
	allocated_slabs_list = (u8**)mem::stl_allocator.allocate(sizeof(u8**) + slab_size);
	if (!allocated_slabs_list)
		return false;
	*allocated_slabs_list = 0;
	start = cursor = (u8*)(allocated_slabs_list + 1);
	return true;
}

void Bump::deinit()
{
	for (;;)
	{
		u8** next = (u8**)*allocated_slabs_list;
		mem::stl_allocator.free(allocated_slabs_list);
		if (!next)
			break;
		allocated_slabs_list = next;
	}
}

void* Bump::allocate(u64 size)
{
	assert(size < slab_size);

	// we store the size of the allocation with the data
	// to support reallocation
	// idrk if this a good idea
	u64 total_size = size + sizeof(u64);

	if (cursor - start + total_size > slab_size)
	{
		u8* newslab = (u8*)mem::stl_allocator.allocate(sizeof(u8**) + slab_size);
		*((u8**)start - 1) = newslab;
		start = cursor = newslab + 1;
	}

	u64* header = (u64*)cursor;
	*header = size;
	u8* out = (u8*)(header + 1);
	cursor += total_size;
	return out;
}

void* Bump::reallocate(void* ptr, u64 new_size)
{
	u64 old_size = *((u64*)ptr - 1);
	if (old_size < new_size)
	{
		void* dst = allocate(new_size);
		mem::copy(dst, ptr, old_size);
		return dst;
	}
	return ptr;
}

}
