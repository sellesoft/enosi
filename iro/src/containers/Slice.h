/*
 *  Counted view over a contiguous array of some type.
 *
 *  I feel like typically this should be assumed not to own data that its pointing to.
 */

#ifndef _iro_Slice_h
#define _iro_Slice_h

#include "../Common.h"
#include "../traits/Nil.h"
#include "assert.h"

namespace iro
{

template<typename T>
struct Slice
{
  T*  ptr = nullptr;
  u64 len = 0;

  static Slice<T> from(T* ptr, u64 len) { return { ptr, len }; }

  template<u64 size>
  static Slice<T> from(T (&buffer)[size]) { return from(buffer, size); }

  static Slice<T> invalid() { return {nullptr, 0}; }
  b8 isValid() { return ptr != nullptr; }

  b8 isEmpty() { return len == 0; }

  Slice<T> sub(s32 start) const
  {
    assert(start >= 0 && start < len);
    return { ptr + start, len - start };
  }

  T* begin() { return ptr; }
  T* end()   { return ptr + len; }

  T& operator[](u64 i) const { return ptr[i]; }
};

typedef Slice<u8> Bytes;

}

template<typename T>
struct NilValue<iro::Slice<T>>
{
  constexpr static const iro::Slice<T> Value = {nullptr};
  inline static b8 isNil(const iro::Slice<T>& x) { return x.ptr == nullptr; }
};

#endif // _iro_slice_h
