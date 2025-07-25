/*
 *  Growing array
 */

#ifndef _iro_Array_h
#define _iro_Array_h

#include "../Common.h"
#include "../memory/Memory.h"
#include "../memory/Allocator.h"

#include "ArrayFuncs.h"

#include "../traits/Nil.h"
#include "../traits/Move.h"
#include "../traits/Container.h"
#include "../traits/Iterable.h"

#include "Slice.h"

#include "new"
#include "assert.h"

namespace iro
{

template<typename T>
struct Array
{
  struct Header
  {
    s32 len = 0;
    s32 space = 0;
    mem::Allocator* allocator = nullptr;
  };

  T* arr = nullptr;

  static Header* getHeader(T* ptr) { return (Header*)ptr - 1; }
  static s32& len(T* ptr) { return getHeader(ptr)->len; }
  static s32& space(T* ptr) { return getHeader(ptr)->space; }
  static mem::Allocator* allocator(T* ptr)
    { return getHeader(ptr)->allocator; }

  static Array<T> fromOpaquePointer(T* ptr) { return Array<T>{ptr}; }

  static Array<T> create(mem::Allocator* allocator)
  {
    return create(8, allocator);
  }

  /* --------------------------------------------------------------------------
   */
  b8 init(s32 init_space = 8, mem::Allocator* allocator = &mem::stl_allocator)
  {
    init_space = (8 > init_space ? 8 : init_space);

    Header* header =
      (Header*)allocator->allocate(sizeof(Header) + init_space * sizeof(T));
    if (header == nullptr)
      return false;

    header->space = init_space;
    header->len = 0;
    header->allocator = allocator;

    arr = (T*)(header + 1);

    return true;
  }

  /* --------------------------------------------------------------------------
   */
  static Array<T> create(
      s32 init_space = 8,
      mem::Allocator* allocator = &mem::stl_allocator)
  {

    Array<T> out = {};

    if (!out.init(init_space, allocator))
      return nil;

    return out;
  }

  /* --------------------------------------------------------------------------
   */
  void destroy()
  {
    if (isnil(*this))
      return;
    clear();
    mem::Allocator* a = allocator();
    a->free(getHeader());
    *this = nil;
  }

  /* --------------------------------------------------------------------------
   */
  Slice<T> asSlice() const
  {
    return Slice<T>::from(arr, len());
  }

  /* --------------------------------------------------------------------------
   */
  Header* getHeader();
  s32& len();
  s32& space();
  mem::Allocator* allocator();

  const Header* getHeader() const { return (Header*)arr - 1; }
  s32 len() const { return arr? getHeader()->len : 0; }
  s32 space() const { return arr? getHeader()->space : 0; }

  b8 isEmpty() const { return len() == 0; }

  /* --------------------------------------------------------------------------
   */
  T* push()
  {
    growIfNeeded(1);

    return array::push(arr, &len());
  }

  void push(const T& x)
  {
    growIfNeeded(1);

    array::push(arr, &len(), x);
  }

  /* --------------------------------------------------------------------------
   */
  void pop()
  {
    assert(len() != 0);
    array::pop(arr, &len());
  }

  /* --------------------------------------------------------------------------
   */
  void insert(s32 idx, const T& x)
  {
    assert(idx >= 0 && idx <= len());
    growIfNeeded(1);

    array::insert(arr, &len(), idx, x);
  }

  T* insert(s32 idx)
  {
    assert(idx >= 0 && idx <= len());
    growIfNeeded(1);

    return array::insert(arr, &len(), idx);
  }

  /* --------------------------------------------------------------------------
   */
  void remove(T* x)
  {
    return remove(x - arr);
  }

  void remove(s32 idx)
  {
    assert(idx >= 0 && idx < len());
    array::remove(arr, &len(), idx);
  }

  /* --------------------------------------------------------------------------
   */
  void clear()
  {
    array::clear(arr, &len());
  }

  /* --------------------------------------------------------------------------
   */
  void resize(u64 new_len)
  {
    if (new_len == 0)
    {
      clear();
      return;
    }

    if (new_len < len())
    {
      for (u64 i = new_len + 1; i < len(); ++i)
        arr[i].~T();

      len() = new_len;
    }
    else if (new_len > len())
    {
      growIfNeeded(new_len - len());

      for (u64 i = len(); i < new_len; ++i)
        new (arr + i) T;

      len() = new_len;
    }
  }

  /* --------------------------------------------------------------------------
   */
  T& operator [](s64 i) { assert(i >=0 && i < len()); return arr[i]; }
  const T& operator[](s64 i) const
    { assert(i >= 0 && i < len()); return arr[i]; }

  T* begin() { return arr; }
  T* end()   { return arr + len(); }

  const T* begin() const { return arr; }
  const T* end()   const { return arr + len(); }

  T* first() { return begin(); }
  T* last() { return end() - 1; }

  /* --------------------------------------------------------------------------
   */
  void growIfNeeded(s32 new_elements)
  {
    Header* header = getHeader();

    if (header->len + new_elements <= header->space)
      return;

    while (header->len + new_elements > header->space)
      header->space *= 2;

    header =
      (Header*)header->allocator->reallocate(
          header, sizeof(Header) + header->space * sizeof(T));
    arr = (T*)(header + 1);
  }

  DefineNilTrait(Array<T>, {nullptr}, x.arr == nullptr);
  DefineMoveTrait(Array<T>, { to.arr = from.arr; });
};

// NOTE(sushi) these are defined out-of-line to prevent the compiler from
//             for inlining them to help debugging.

/* ----------------------------------------------------------------------------
 */
template<typename T>
Array<T>::Header* Array<T>::getHeader()
{
  return (Header*)arr - 1;
}

/* ----------------------------------------------------------------------------
 */
template<typename T>
s32& Array<T>::len()
{
  return getHeader()->len;
}

/* ----------------------------------------------------------------------------
 */
template<typename T>
s32& Array<T>::space()
{
  return getHeader()->space;
}

/* ----------------------------------------------------------------------------
 */
template<typename T>
mem::Allocator* Array<T>::allocator()
{
  return getHeader()->allocator;
}

}

#endif

