/*
 *  Stuff related to dealing with unicode.
 */

#ifndef _iro_unicode_h
#define _iro_unicode_h

#include "common.h"
#include "containers/slice.h"
#include "traits/nil.h"
#include "traits/container.h"

#include "memory/allocator.h"
#include "memory/memory.h"

#include "assert.h"

namespace iro::utf8
{

/* 
 *  Using https://github.com/JuliaStrings/utf8proc/blob/master/utf8proc.c
 *  as a reference for some of this stuff.
 */

// Returns how many bytes are needed to encode the given 
// codepoint. If 0 is returned then the given code point
// should not be encoded.
u8 bytesNeededToEncodeCharacter(u32 codepoint);

b8 isContinuationByte(u8 c);

/* ============================================================================
 *  Representation of an encoded character with enough space for a 4 byte 
 *  character and a count of how many bytes were used.
 */
struct Char
{
  u8 bytes[4];
  u8 count;

  static Char invalid() { return {{},0}; }
  b8 isValid() { return count != 0; }
};

// Encodes the given codepoint into 'ch'.
// Returns false if failure occurs for any reason.
Char encodeCharacter(u32 codepoint);

/* ============================================================================
 *  Representation of a decoded unicode codepoint along with the number of 
 *  bytes needed it to represent it in utf8.
 */
struct Codepoint
{
  u32 codepoint;
  u32 advance;

  operator u32() { return codepoint; }

  bool operator ==(u32  x) { return codepoint == x; }
  bool operator ==(Codepoint c) { return codepoint == c.codepoint; }
  bool operator !=(u32  x) { return codepoint != x; }
  bool operator ==(char x) { return codepoint == x; }
  bool operator !=(char x) { return codepoint != x; }
};

// Attempt to decode the character at 's' into 'codepoint'.
// If decoding fails for any reason, false is returned.
// TODO(sushi) it may be nice to have an 'unsafe' version w all the valid checks disabled 
//             if we're ever confident we're dealing with proper utf8 strings
Codepoint decodeCharacter(u8* s, s32 slen);

inline u64 stringHash(const struct str* x);

/* ============================================================================
 *  A string encoded in utf8.
 */
struct str
{
  u8* bytes = nullptr;
  u64 len = 0;

  operator Bytes() { return {bytes, len}; }

  // where 'end' is a pointer to the byte AFTER the last of the range desired
  static inline str from(u8* start, u8* end) 
    { assert(start <= end); return {start, u64(end - start)}; }
  static inline str from(u8* start, u64 len) { return {start, len}; }
  static inline str from(Slice<u8> slice) { return {slice.ptr, slice.len}; }
  static str fromCStr(const char* s);

  b8 isEmpty() { return len == 0; }

  u64 hash() { return stringHash(this); }

  // Advances this str by 'n' codepoints and returns the last codepoint passed.
  // This may return invalid codepoints if this str is not valid utf8!
  Codepoint advance(s32 n = 1);

  void increment(s32 n) { n = (n > len? len : n); bytes += n; len -= n; }

  // If the provided buffer is large enough, copy this 
  // str into it followed by a null terminator and return true.
  // Otherwise return false;
  b8 nullTerminate(u8* buffer, s32 buffer_len);

  str sub(s32 start) 
  { 
    assert(start >= 0 && start < len); 
    return {bytes + start, len - start}; 
  }
  
  str sub(s32 start, s32 end) 
  { 
    assert(start >= 0 && start <= end && end <= len); 
    return {bytes + start, u64(end - start)};  
  }

  b8 operator ==(str s);

  u8* begin() { return bytes; }
  u8* end()   { return bytes + len; }

  u8& last() { return *(end()-1); }

  // returns how many characters there are in this string
  u64 countCharacters();

  b8 startsWith(str s);

  struct pos
  {
    u64 x;

    pos operator + (const int& rhs) { return pos{x + rhs}; }
    static pos found(u64 x) { return {x}; }
    static pos notFound() { return {(u64)-1}; }
    b8 found() { return x != (u64)-1; }
    operator u64() { return x; }
  };

  // each find function should provide a byte and codepoint variant
  pos findFirst(u8 c);
  pos findLast(u8 c);

  void split(
      u8 c, 
      ExpandableContainer<str> auto* container, 
      b8 remove_empty = true)
  {
    str scan = *this;

    while (not scan.isEmpty())
    {
      pos p = scan.findFirst(c);
      if (!p.found())
      {
        containerAdd(container, scan);
        return;
      }

      if (p == 0 && remove_empty)
        continue;

      if (!containerAdd(container, scan.sub(0, p)))
        return;

      scan = scan.sub(p+1);
    }
  }

  struct LineAndColumn
  {
    s32 line = 1;
    s32 column = 1;
  };

  LineAndColumn findLineAndColumnAtOffset(s32 offset)
  {
    LineAndColumn result = {};
    str me = *this;
    while (not me.isEmpty() && me.bytes - bytes < offset)
    {
      Codepoint c = me.advance();
      if (c.codepoint == '\n')
      {
        result.line += 1;
        result.column = 1;
      }
      else
      {
        result.column += 1;
      }
    }
    return result;
  }

  str allocateCopy(mem::Allocator* allocator = &mem::stl_allocator)
  {
    str out;
    out.bytes = (u8*)allocator->allocate(len);
    out.len = len;
    mem::copy(out.bytes, bytes, len);
    return out;
  }
};

inline u64 stringHash(const str* x)
{
  u64 seed = 14695981039;
  for (s32 i = 0; i < x->len; i++)
  {
    seed ^= (u8)x->bytes[i];
    seed *= 1099511628211; //64bit FNV_prime
  }
  return seed;
}

}

namespace iro
{
// since we only use utf8 internally, just bring the string type into the 
// global namespace
using str  = utf8::str;

static str operator ""_str (const char* s, size_t len)
{
  return str{(u8*)s, len};
}

}

DefineNilValue(iro::utf8::str, {nullptr}, { return x.bytes == nullptr; });
DefineNilValue(iro::utf8::Codepoint, {(u32)-1}, { return x.codepoint == (u32)-1; });

#endif // _iro_unicode_h

