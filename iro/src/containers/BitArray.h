/*
 *  A statically sized bit array.
 */

#ifndef _iro_BitArray_h
#define _iro_BitArray_h

#include "../Common.h"
#include "assert.h"

namespace iro
{

consteval u32 floorLog2(u32 x)
{
  return x == 1 ? 0 : 1 + floorLog2(x >> 1);
}

enum
{
  BitsPerWord = 8 * sizeof(u32),
  BitsPerWordLog2 = floorLog2(BitsPerWord),
};

/* ============================================================================
 */
template<u32 size>
struct BitArray
{
  using Self = BitArray<size>;

  enum
  {
    WordCount = (size + (BitsPerWord - 1)) / BitsPerWord,
  };

  u32 words[WordCount];

  void set(u32 offset)
  {
    assert(offset < size);
    u32 word = offset >> BitsPerWordLog2;
    u32 bit  = offset - word * BitsPerWord; 
    words[word] |= (1 << bit);
  }

  void unset(u32 offset)
  {
    assert(offset < size);
    u32 word = offset >> BitsPerWordLog2;
    u32 bit  = offset - word * BitsPerWord;
    words[word] &= ~(1 << bit);
  }

  b8 test(u32 offset) const
  {
    u32 word = offset >> BitsPerWordLog2;
    u32 bit  = offset - word * BitsPerWord;
    return 0 != (words[word] & (1 << bit));
  }

  void clear()
  {
    for (u32 i = 0; i < WordCount; ++i)
      words[i] = 0;
  }

  b8 testAny() const
  {
    for (u32 i = 0; i < WordCount; ++i)
    {
      if (words[i])
        return true;
    }
    return false;
  }

  Self bitOr(const Self& rhs) const
  {
    Self out = *this;
    for (u32 i = 0; i < WordCount; ++i)
      out.words[i] |= rhs.words[i];
    return out;
  }

  Self bitAnd(const Self& rhs) const
  {
    Self out = *this;
    for (u32 i = 0; i < WordCount; ++i)
      out.words[i] &= rhs.words[i];
    return out;
  }

  Self bitXor(const Self& rhs) const
  {
    Self out = *this;
    for (u32 i = 0; i < WordCount; ++i)
      out.words[i] ^= rhs.words[i];
    return out;
  }

  Self bitNot() const
  {
    Self out = {};
    for (u32 i = 0; i < WordCount; ++i)
      out.words[i] = ~words[i];
    return out;
  }
};

/* ============================================================================
 *   A wrapper around BitArray that takes an enum to use as the bits needed.
 *   The enum must define a COUNT element as its last element.
 */
template<typename TEnum>
struct EnumBitArray : public BitArray<(u32)TEnum::COUNT>
{
  using Up = BitArray<(u32)TEnum::COUNT>;
  void set(TEnum offset)        { return Up::set((u32)offset); }
  void unset(TEnum offset)      { return Up::unset((u32)offset); }
  b8   test(TEnum offset) const { return Up::test((u32)offset); }
};

}  // namespace iro

#endif
