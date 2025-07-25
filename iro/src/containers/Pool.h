/*
 *
 *  Pool structure for making chunks of unmoving memory and allocating from them in an 
 *  efficient manner;
 *
 */

#ifndef _iro_Pool_h
#define _iro_Pool_h

#include "../Common.h"
#include "../memory/Allocator.h"
#include "new"

#include "../traits/Container.h"

namespace iro
{

template
<
  typename T, // element type
  s32 N_slots_per_chunk = 16,
  bool can_grow = true
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
      Slot* next_free_slot = nullptr;
    };
  };

  struct Chunk
  {
    Chunk* next = nullptr;
    Slot   slots[N_slots_per_chunk];
  };

  Chunk* current_chunk = nullptr;
  Slot*  free_slot = nullptr;

  mem::Allocator* allocator = nullptr;

  /* -------------------------------------------------------------------------------------------- create
   */ 
  static Self create(mem::Allocator* allocator = &mem::stl_allocator)
  {
    Self out = {};
    out.init(allocator);
    return out;
  }

  /* -------------------------------------------------------------------------------------------- create
   */ 
  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    this->allocator = allocator;
    newChunk();
    free_slot = current_chunk->slots;

    return true;
  }

  /* -------------------------------------------------------------------------------------------- destroy
   */ 
  void deinit()
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

  /* -------------------------------------------------------------------------------------------- destroy
   */ 
  void move(Pool<T>& dest)
  {
    assert(isnil(dest));

    dest.current_chunk = current_chunk;
    dest.free_slot = free_slot;
    dest.allocator = allocator;

    current_chunk = nullptr;
    free_slot = nullptr;
    allocator = nullptr;
  }

  /* -------------------------------------------------------------------------------------------- add
   */ 
  T* add()
  {
    Slot* slot = free_slot;
    if (!slot->next_free_slot)
    {
      if (can_grow)
      {
        newChunk();
        slot->next_free_slot = current_chunk->slots;
      }
      else
        return nullptr;
    }

    free_slot = slot->next_free_slot;

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
    slot->next_free_slot = free_slot;
    free_slot = slot;
  }

  DefineNilTrait(Pool<T>, {}, x.free_slot == nullptr);
    
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
    }

    chunk->next = current_chunk;
    current_chunk = chunk;
  }
};

/* ============================================================================
 *  Pool that initializes statically. This should be moved elsewhere to be 
 *  reusable.
 */
template<typename T>
struct StaticPool : public Pool<T>
{
  StaticPool() { Pool<T>::init(); }
};

}

#endif
