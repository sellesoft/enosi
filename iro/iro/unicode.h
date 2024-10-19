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

inline u64 stringHash(const struct String* x);

/* ============================================================================
 *  A string encoded in utf8.
 *
 *  TODO(sushi) this shares a lot of functionality with Slice<u8>. Consider
 *              inheriting it here.
 */
struct String
{
  u8* ptr = nullptr;
  u64 len = 0;

  operator Bytes() const { return {ptr, len}; }

  // where 'end' is a pointer to the byte AFTER the last of the range desired
  static inline String from(u8* start, u8* end) 
    { assert(start <= end); return {start, u64(end - start)}; }
  static inline String from(u8* start, u64 len) { return {start, len}; }
  static inline String from(Slice<u8> slice) { return {slice.ptr, slice.len}; }
  static String fromCStr(const char* s);
  static String fromCStr(u8* ptr) { return fromCStr((char*)ptr); }

  b8 isEmpty() const { return len == 0; }

  u64 hash() const { return stringHash(this); }

  bool operator ==(String s) const;

  // Advances this String by 'n' codepoints and returns the last codepoint passed.
  // This may return invalid codepoints if this String is not valid utf8!
  Codepoint advance(s32 n = 1);

  // Increments this String by n bytes.
  void increment(s32 n) { n = (n > len? len : n); ptr += n; len -= n; }

  // If the provided buffer is large enough, copy this 
  // String into it followed by a null terminator and return true.
  // Otherwise return false;
  b8 nullTerminate(u8* buffer, s32 buffer_len) const;

  // Allocates a nullterminated copy of this string using the provided 
  // allocator. The length of the returned string will be the same.
  String nullTerminate(mem::Allocator* allocator) const;

  String sub(s32 start) const
  { 
    assert(start >= 0 && start < len); 
    return {ptr + start, len - start}; 
  }
  
  String sub(s32 start, s32 end) const
  { 
    assert(start >= 0 && start <= end && end <= len); 
    return {ptr + start, u64(end - start)};  
  }

  // Returns nil if no occurance of c is found.
  String subFromFirst(u8 c) const;
  String subToFirst(u8 c) const;
  String subFromLast(u8 c) const;
  String subToLast(u8 c) const;

  // Returns nil if we only find c.
  String subFromFirstNot(u8 c) const;
  String subToFirstNot(u8 c) const;
  String subFromLastNot(u8 c) const;
  String subToLastNot(u8 c) const;

  u8* begin() { return ptr; }
  u8* end()   { return ptr + len; }

  u8* begin() const { return ptr; }
  u8* end()   const { return ptr + len; }

  u8& last() const { return *(end()-1); }

  // returns how many characters there are in this string
  u64 countCharacters() const;

  b8 startsWith(String s) const;
  b8 endsWith(String s) const;

  b8 startsWith(u8 c) const { return ptr[0] == c; }

  struct pos
  {
    u64 x;

    pos operator + (const int& rhs) { return pos{x + rhs}; }
    static pos found(u64 x) { return {x}; }
    static pos notFound() { return {(u64)-1}; }
    b8 found() { return x != (u64)-1; }
    operator u64() { return x; }
  };

  pos findFirst(u8 c) const;
  pos findLast(u8 c) const;
  pos findFirstNot(u8 c) const;
  pos findLastNot(u8 c) const;

  void split(
      u8 c, 
      ExpandableContainer<String> auto* container, 
      b8 remove_empty = true) const
  {
    String scan = *this;

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

  LineAndColumn findLineAndColumnAtOffset(s32 offset) const
  {
    LineAndColumn result = {};
    String me = *this;
    while (not me.isEmpty() && me.ptr - ptr < offset)
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

  String allocateCopy(mem::Allocator* allocator = &mem::stl_allocator) const
  {
    String out;
    out.ptr = (u8*)allocator->allocate(len);
    out.len = len;
    mem::copy(out.ptr, ptr, len);
    return out;
  }
};

inline u64 stringHash(const String* x)
{
  u64 seed = 14695981039;
  for (s32 i = 0; i < x->len; i++)
  {
    seed ^= (u8)x->ptr[i];
    seed *= 1099511628211; //64bit FNV_prime
  }
  return seed;
}

}

namespace iro
{
// since we only use utf8 internally, just bring the string type into the 
// global namespace
using String  = utf8::String;

static String operator ""_str (const char* s, size_t len)
{
  return String{(u8*)s, len};
}

}

DefineNilValue(iro::utf8::String, {nullptr}, { return x.ptr == nullptr; });
DefineNilValue(iro::utf8::Codepoint, {(u32)-1}, 
    { return x.codepoint == (u32)-1; });

#endif // _iro_unicode_h

