#include "Unicode.h"
#include "Logger.h"

#include "assert.h"
#include "stdio.h"
#undef stdin
#undef stdout
#undef stderr
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

namespace iro::utf8
{

Logger logger = Logger::create("utf8"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
u8 bytesNeededToEncodeCharacter(u32 c)
{
  if (c < 0x80)
    return 1;
  if (c < 0x800)
    return 2;
  if (c < 0x10000)
    return 3;
  if (c < 0x110000)
    return 4;
  return 0; // do not encode this character !!
}

/* ----------------------------------------------------------------------------
 */
b8 isContinuationByte(u8 c)
{
  return ((c) & 0xc0) == 0x80;
}

/* ----------------------------------------------------------------------------
 */
Char encodeCharacter(u32 codepoint)
{
  Char c = {};

  c.count = bytesNeededToEncodeCharacter(codepoint);

  switch (c.count)
  {
    default:
      ERROR("utf8::encodeCharacter(): could not resolve a proper number of"
            " bytes needed to encode codepoint: ", codepoint, "\n");
      return nil;

    case 1:
      c.bytes[0] = (u8)codepoint;
      return c;

    case 2:
      c.bytes[0] = (u8)(0b11000000 + (codepoint >> 6));
      c.bytes[1] = (u8)(0b10000000 + (codepoint & 0b00111111));
      return c;

    // NOTE(sushi) just like the lib I'm referencing, utf8proc, the range
    //             0xd800 - 0xdfff is encoded here, but this is not valid utf8
    //             as it is reserved for utf16.
    //             MAYBE ill fix this later if its a problem
    case 3:
      c.bytes[0] = (u8)(0b11100000 + (codepoint >> 12));
      c.bytes[1] = (u8)(0b11000000 + ((codepoint >> 6) & 0b00111111));
      c.bytes[2] = (u8)(0b11000000 + (codepoint & 0b00111111));
      return c;

    case 4:
      c.bytes[0] = (u8)(0b11110000 + (codepoint >> 18));
      c.bytes[1] = (u8)(0b11000000 + ((codepoint >> 12) & 0b00111111));
      c.bytes[2] = (u8)(0b11000000 + ((codepoint >>  6) & 0b00111111));
      c.bytes[3] = (u8)(0b11000000 + (codepoint & 0b00111111));
      return c;
  }
}

/* ----------------------------------------------------------------------------
 */
Codepoint decodeCharacter(u8* s, s32 slen)
{
  assert(s);

#define FERROR(...) ERROR("decodeCharacter(): "_str, __VA_ARGS__)
#define FWARN(...)  WARN("decodeCharacter(): "_str, __VA_ARGS__)

  if (slen == 0)
  {
    FWARN("passed an empty string\n");
    return nil;
  }


  if (s[0] < 0x80)
  {
    return {s[0], 1};
  }

  if ((u32)(s[0] - 0xc2) > (0xf4 - 0xc2))
  {
    ERROR("encountered invalid utf8 character\n");
    return nil;
  }

  if (s[0] < 0xe0)
  {
    if (slen < 2)
    {
      FERROR("encountered 2 byte utf8 character but given slen < 2\n");
      return nil;
    }

    if (!isContinuationByte(s[1]))
    {
      FERROR("encountered 2 byte character but byte 2 is not a"
             " continuation byte\n");
      return nil;
    }

    u32 c = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
    return {c, 2};
  }

  if (s[1] < 0xf0)
  {
    if (slen < 3)
    {
      FERROR("encountered 3 byte character but was given slen < 3\n");
      return nil;
    }

    if (!isContinuationByte(s[1]) || !isContinuationByte(s[2]))
    {
      FERROR("encountered 3 byte character but one of the trailing bytes"
             " is not a continuation character\n");
      return nil;
    }

    if (s[0] == 0xed && s[1] == 0x9f)
    {
      FERROR("encounted 3 byte character with surrogate pairs\n");
      return nil;
    }

    u32 c = ((s[0] & 0x0f) << 18) |
          ((s[1] & 0x3f) << 12) |
        ((s[2] & 0x3f));

    if (c< 0x800)
    {
      // TODO(sushi) look into why this is wrong
      FERROR("c->codepoint wound up being < 0x800 which is wrong for some"
             " reason idk yet look into it maybe???\n");
      return nil;
    }

    return {c, 3};
  }

  if (slen < 4)
  {
    FERROR("encountered 4 byte character but was given slen < 4\n");
    return nil;
  }

  if (   !isContinuationByte(s[1])
      || !isContinuationByte(s[2])
      || !isContinuationByte(s[3]))
  {
    FERROR("encountered 4 byte character but one of the trailing bytes is"
           " not a continuation character\n");
    return nil;
  }

  if (s[0] == 0xf0)
  {
    if (s[1] < 0x90)
    {
      FERROR("encountered a 4 byte character but the codepoint is less"
             " than the valid range (0x10000 - 0x10ffff)");
      return nil;
    }
  }
  else if (s[0] == 0xf4)
  {
    if (s[1] > 0x8f)
    {
      FERROR("encountered a 4 byte character but the codepoint is greater"
             " than the valid range (0x10000 - 0x10ffff)");
      return nil;
    }
  }

  u32 c = ((s[0] & 0x07) << 18) |
      ((s[1] & 0x3f) << 12) |
      ((s[2] & 0x3f) <<  6) |
      ((s[3] & 0x3f));
  return {c, 4};

#undef FERROR
#undef FWARN
}


/* ----------------------------------------------------------------------------
 */
String String::fromCStr(const char* s)
{
  String out;
  out.ptr = (u8*)s;
  out.len = 0;
  while (s[0])
  {
    out.len += 1;
    s += 1;
  }
  return out;
}

/* ----------------------------------------------------------------------------
 */
Codepoint String::advance(s32 n)
{
  Codepoint c;

  for (s32 i = 0; i < n; i++)
  {
    c = decodeCharacter(ptr, len);
    if (isnil(c))
      return nil;
    ptr += c.advance;
    len -= c.advance;
  }

  return c;
}

/* ----------------------------------------------------------------------------
 */
bool String::operator==(String s) const
{
  if (len != s.len)
    return false;

  for (s32 i = 0; i < len; i++)
  {
    if (ptr[i] != s.ptr[i])
      return false;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
String String::subFromFirst(u8 c) const
{
  auto pos = findFirst(c);
  if (pos.found())
    return sub(pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::subToFirst(u8 c) const
{
  auto pos = findFirst(c);
  if (pos.found())
    return sub(0, pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::subFromLast(u8 c) const
{
  auto pos = findLast(c);
  if (pos.found())
    return sub(pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::subToLast(u8 c) const
{
  auto pos = findLast(c);
  if (pos.found())
    return sub(0, pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::subFromLastNot(u8 c) const
{
  auto pos = findLastNot(c);
  if (pos.found())
    return sub(pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::subToLastNot(u8 c) const
{
  auto pos = findLastNot(c);
  if (pos.found())
    return sub(0, pos.x);
  return nil;
}

/* ----------------------------------------------------------------------------
 */
String String::trimLeft()
{
  auto pos = findFirstNot(' ');
  if (pos.found())
    return sub(pos.x);
  return {};
}

/* ----------------------------------------------------------------------------
 */
String String::trimRight()
{
  auto pos = findLastNot(' ');
  if (pos.found())
    return sub(0, pos.x+1);
  return *this;
}

/* ----------------------------------------------------------------------------
 */
String String::trim()
{
  return trimLeft().trimRight();
}

/* ----------------------------------------------------------------------------
 */
b8 String::nullTerminate(u8* buffer, s32 buffer_len) const
{
  if (buffer_len <= len)
    return false;

  mem::copy(buffer, ptr, len);
  buffer[len] = 0;
  return true;
}

/* ----------------------------------------------------------------------------
 */
String String::nullTerminate(mem::Allocator* allocator) const
{
  String out = {};
  out.ptr = (u8*)allocator->allocate(len+1);
  out.len = len;
  mem::copy(out.ptr, ptr, len);
  out.ptr[len] = '\0';
  return out;
}

/* ----------------------------------------------------------------------------
 */
b8 String::nullTerminate(io::IO* io) const
{
  return io::formatv(io, *this, '\0') == len;
}

/* ----------------------------------------------------------------------------
 */
u64 String::countCharacters() const
{
  String scan = *this;
  u64 accum = 0;
  for (;;)
  {
    if (scan.len == 0)
      break;
    scan.advance();
    accum += 1;
  }
  return accum;
}

/* ----------------------------------------------------------------------------
 */
String::pos String::findFirst(u8 c) const
{
  for (s32 i = 0; i < len; i++)
  {
    if (ptr[i] == c)
      return pos::found(i);
  }
  return pos::notFound();
}

/* ----------------------------------------------------------------------------
 */
String::pos String::findLast(u8 c) const
{
  for (s32 i = len-1; i >= 0; i--)
  {
    if (ptr[i] == c)
      return pos::found(i);
  }
  return pos::notFound();
}

/* ----------------------------------------------------------------------------
 */
String::pos String::findFirstNot(u8 c) const
{
  for (s32 i = 0; i < len; ++i)
  {
    if (ptr[i] != c)
      return pos::found(i);
  }
  return pos::notFound();
}

/* ----------------------------------------------------------------------------
 */
String::pos String::findLastNot(u8 c) const
{
  for (s32 i = len-1; i >= 0; --i)
  {
    if (ptr[i] != c)
      return pos::found(i);
  }
  return pos::notFound();
}

/* ----------------------------------------------------------------------------
 */
String::pos String::findString(String s) const
{
  if (s.len > len)
    return pos::notFound();

  for (s32 i = 0; i <= len - s.len; ++i)
  {
    if (sub(i).startsWith(s))
      return pos::found(i);
  }

  return pos::notFound();
}

/* ----------------------------------------------------------------------------
 */
b8 String::startsWith(String s) const
{
  if (s.len > len)
    return false;
  for (s32 i = 0; i < s.len; i++)
  {
    if (ptr[i] != s.ptr[i])
      return false;
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 String::endsWith(String s) const
{
  if (s.len > len)
    return false;
  for (s32 i = 0; i < s.len; ++i)
  {
    if (ptr[len-i-1] != s.ptr[s.len-i-1])
      return false;
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 String::isNumeric() const
{
  for (s32 i = 0; i < len; ++i)
  {
    // TODO(sushi) make unicode proper
    if (!isdigit(ptr[i]))
      return false;
  }
  return true;
}

} // namespace utf8


