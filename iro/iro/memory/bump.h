/*
 *  Bump allocator, which is pretty much a chunked arena.
 *
 *  Allocates memory that we dont intend to cleanup until we're finished with all of it.
 *  So this does not implement free. In order to support reallocating, though, we do
 *  still store the size of an allocation before the actual data.
 */

#ifndef _iro_bump_h
#define _iro_bump_h

#include "allocator.h"
#include "assert.h"

namespace iro::mem
{

struct Bump : public Allocator
{
	const static u32 slab_size = 4096;

	struct Slab
	{
		Slab* next;
		u8 content[slab_size];
	};

	u8* start;
	u8* cursor;

	Slab* slabs;

	b8   init();
	void deinit();

	void* allocate(u64 size) override;

	void* reallocate(void* ptr, u64 size) override;
	
	[[deprecated("Bump does not implement free! It is OK to call this function directly, but it will do nothing.")]]
	void  free(void* ptr) override {}
};

}

#endif // _iro_bump_h
