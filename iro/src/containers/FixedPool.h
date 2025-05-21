/*
 *  Fixed pool structure for making chunks of unmoving memory and allocating
 *  from them in an efficient manner. The pool size is fixed at compile time
 *  and there is no memory allocated for the pool.
 */
#ifndef _iro_FixedPool_h
#define _iro_FixedPool_h

#include "../Common.h"
#include "../traits/Nil.h"

#include <assert.h>
#include <new>

namespace iro
{

template<typename T, s32 N_slots>
struct FixedPool
{
  static constexpr s32 MAX_COUNT = N_slots;

  union Slot
  {
    T     element;
    Slot* next_free_slot = nullptr;
  };

  Slot slots[N_slots];
  Slot* free_slot = nullptr;

  /* --------------------------------------------------------------------------
   */
  static FixedPool create()
  {
    FixedPool out = {};
    out.init();
    return out;
  }

  /* --------------------------------------------------------------------------
   */
  void init()
  {
    for (s32 i = 0; i < N_slots; i++)
    {
      slots[i].next_free_slot = &slots[i + 1];
    }
    slots[N_slots - 1].next_free_slot = nullptr;
    free_slot = &slots[0];
  }

  /* --------------------------------------------------------------------------
   */
  void deinit()
  {
    for (s32 i = 0; i < N_slots; i++)
    {
      slots[i].element.~T();
    }
    free_slot = nullptr;
  }

  /* --------------------------------------------------------------------------
   */
  T* add()
  {
    assert(free_slot != nullptr
      && "FixedPool is full - cannot add more elements");

    if (free_slot == nullptr)
      return nullptr;

    Slot* slot = free_slot;
    free_slot = slot->next_free_slot;

    return new (&slot->element) T;
  }

  void add(const T& x)
  {
    T* v = add();
    if (v != nullptr)
      *v = x;
  }

  /* --------------------------------------------------------------------------
   */
  void remove(T* x)
  {
    Slot* slot = (Slot*)x;
    assert(slot >= slots && slot < slots + N_slots
      && "invalid pointer passed to remove");

    if (slot < slots || slot >= slots + N_slots)
      return;

    slot->element.~T();
    slot->next_free_slot = free_slot;
    free_slot = slot;
  }
  
  /* --------------------------------------------------------------------------
   */
  s32 indexOf(T* x)
  {
    return (s32)(x - (T*)slots);
  }
  
  /* --------------------------------------------------------------------------
   */
  T* atIndex(s32 index)
  {
    assert(index >= 0 && index < N_slots
      && "invalid index passed to atIndex");

    if (index < 0 || index >= N_slots)
      return nullptr;

    return (T*)(slots + index);
  }

  // Manually written because C++ is very cool.
  struct NilTrait
  {
    static constexpr FixedPool<T, N_slots> getValue()
    {
      return {};
    }

    static inline bool isNil(const FixedPool<T, N_slots>& x)
    {
      return x.free_slot == nullptr;
    }
  };
};

}

#endif // #ifndef _iro_FixedPool_h 
