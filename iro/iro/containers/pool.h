/*
 *
 *  Pool structure for making chunks of unmoving memory and allocating from them in an 
 *  efficient manner;
 *
 */

#ifndef _ilo_pool_h
#define _ilo_pool_h

#include "../common.h"
#include "../memory/allocator.h"
#include "new"

#include "../traits/container.h"

namespace iro
{

template
<
	typename T, // element type
	s32 N_slots_per_chunk = 16
>
struct Pool 
{
	typedef Pool<T, N_slots_per_chunk> Self;

	struct Slot
	{
		// NOTE(sushi) element/next_free_slot union MUST be first
		//             as this avoids the offsetof warning about non-standard-layout 
		//             or some garbage IDK C++ is weird
		//             MAYBE ill look into it more later
		union
		{
			T     element;
			Slot* next_free_slot;
		};
		b8 used;
	};

	struct Chunk
	{
		Chunk* next = nullptr;
		Slot   slots[N_slots_per_chunk];
	};
	

	Chunk* current_chunk = nullptr;
	Slot*  free_slot = nullptr;

	mem::Allocator* allocator = nullptr;


	/* -------------------------------------------------------------------------------------------- 
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */ 
	static Self create(mem::Allocator* allocator = &mem::stl_allocator)
	{
		Self out = {};
		
		out.allocator = allocator;
		out.newChunk();
		out.free_slot = out.current_chunk->slots;

		return out;
	}

	/* -------------------------------------------------------------------------------------------- destroy
	 */ 
	void destroy()
	{
		while (current_chunk)	
		{
			Chunk* next = current_chunk->next;
			allocator->free(current_chunk);
			current_chunk = next;
		}

		current_chunk = nullptr;
		free_slot = nullptr;
	}

	/* -------------------------------------------------------------------------------------------- add
	 */ 
	T* add()
	{
		Slot* slot = free_slot;
		if (!slot->next_free_slot)
		{
			newChunk();
			slot->next_free_slot = current_chunk->slots;
		}

		free_slot = slot->next_free_slot;

		slot->used = true;
		return new (&slot->element) T;
	}

	void add(const T& x)
	{
		T* v = add();
		*v = x;
	}

	/* -------------------------------------------------------------------------------------------- remove
	 */ 
	void remove(T* x)
	{
		Slot* slot = (Slot*)x;
		slot->element.~T();
		slot->used = false;
		slot->next_free_slot = free_slot;
		free_slot = slot;
	}

	/* --------------------------------------------------------------------------------------------
	 *  Internal helpers
	 */ 
private:

	/* -------------------------------------------------------------------------------------------- newChunk
	 */ 
	void newChunk()
	{
		auto chunk = (Chunk*)allocator->allocate(sizeof(Chunk));
		
		for (s32 i = 0; i < N_slots_per_chunk; i++)
		{
			Slot& slot = chunk->slots[i];
			slot.next_free_slot = (i == N_slots_per_chunk - 1 ? nullptr : &chunk->slots[i+1]);
			slot.used = false;
		}

		chunk->next = current_chunk;
		current_chunk = chunk;
	}

	// VERY inefficient helper for iterating over all elements 
	// of this pool. This should only be used sparingly, like 
	// for deinitializing things when you don't already have 
	// a better list of them to work with.
	//
	// TODO(sushi) this is allll fucked up please clean it up !!!!
	struct Iterator
	{
		Chunk* current;
		s32 slotidx;

		T* operator++()
		{
			for (;;)
			{
				if (slotidx == N_slots_per_chunk - 1)
				{
					current = current->next;
					slotidx = 0;
					if (!current)
						return nullptr;
				}
				if (current->slots[slotidx].used)
				{
					slotidx += 1;
					return &current->slots[slotidx - 1].element;
				}
				slotidx += 1;
			}
		}

		b8 operator != (const Iterator& rhs)
		{
			return rhs.slotidx != slotidx || current != rhs.current;
		}

		T& operator*()
		{
			return current->slots[slotidx].element;
		}
	};

public:

	Iterator begin() { return {current_chunk, 0}; }
	Iterator end()   { return {nullptr, 0}; }
};

}

// DefineExpandableContainerT(iro::Pool, { self->add(value); return true; });

#endif
