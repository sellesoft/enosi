#include "unicode.h"
#include "logger.h"

#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

namespace iro::utf8
{

Logger logger = Logger::create("utf8"_str, Logger::Verbosity::Notice);

/* ------------------------------------------------------------------------------------------------ utf8::bytesNeededToEncodeCharacter
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

/* ------------------------------------------------------------------------------------------------ utf8::isContinuationByte
 */
b8 isContinuationByte(u8 c)
{
	return ((c) & 0xc0) == 0x80;
}

/* ------------------------------------------------------------------------------------------------ utf8::encodeCharacter
 */
Char encodeCharacter(u32 codepoint)
{
	Char c = {};

	c.count = bytesNeededToEncodeCharacter(codepoint);

	switch (c.count)
	{
		default:
			ERROR("utf8::encodeCharacter(): could not resolve a proper number of bytes needed to encode codepoint: ", codepoint, "\n");
			return Char::invalid();

		case 1: 
			c.bytes[0] = (u8)codepoint;
			return c;

		case 2:
			c.bytes[0] = (u8)(0b11000000 + (codepoint >> 6));
			c.bytes[1] = (u8)(0b10000000 + (codepoint & 0b00111111));
			return c;

		// NOTE(sushi) just like the lib I'm referencing, utf8proc, the range 
		//             0xd800 - 0xdfff is encoded here, but this is not valid utf8 as it is 
		//             reserved for utf16.
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

/* ------------------------------------------------------------------------------------------------ utf8::decodeCharacter
 */
Codepoint decodeCharacter(u8* s, s32 slen)
{
	assert(s);

#define FERROR(...) ERROR("utf8::decodeCharacter(): "_str, __VA_ARGS__)
#define FWARN(...)  WARN("utf8::decodeCharacter(): "_str, __VA_ARGS__)

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
			FERROR("encountered 2 byte character but byte 2 is not a continuation byte\n");
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
			FERROR("encountered 3 byte character but one of the trailing bytes is not a continuation character\n");
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
			FERROR("c->codepoint wound up being < 0x800 which is wrong for some reason idk yet look into it maybe???\n");
			return nil; 
		}

		return {c, 3};
	}

	if (slen < 4)
	{
		FERROR("encountered 4 byte character but was given slen < 4\n");
		return nil;
	}

	if (!isContinuationByte(s[1]) || !isContinuationByte(s[2]) || !isContinuationByte(s[3]))
	{
		FERROR("encountered 4 byte character but one of the trailing bytes is not a continuation character\n");
		return nil;
	}

	if (s[0] == 0xf0)
	{
		if (s[1] < 0x90)
		{
			FERROR("encountered a 4 byte character but the codepoint is less than the valid range (0x10000 - 0x10ffff)");
			return nil;
		}	
	}
	else if (s[0] == 0xf4)
	{
		if (s[1] > 0x8f)
		{
			FERROR("encountered a 4 byte character but the codepoint is greater than the valid range (0x10000 - 0x10ffff)");
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

/* ------------------------------------------------------------------------------------------------ utf8::str::advance
 */
Codepoint str::advance(s32 n)
{
	Codepoint c;

	for (s32 i = 0; i < n; i++)
	{
		c = decodeCharacter(bytes, len);
		if (isnil(c))
			return nil;
		bytes += c.advance;
		len -= c.advance;
	}

	return c;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::operator ==
 */
b8 str::operator==(str s)
{
	if (len != s.len)
		return false;
	
	for (s32 i = 0; i < len; i++)
	{
		if (bytes[i] != s.bytes[i])
			return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::hash
 *  REMEMBER: this needs to be exactly the same as staticStringHash!!!!
 */
u64 str::hash()
{
	u64 seed = 14695981039;
	for (s32 i = 0; i < len; i++)
	{
		seed ^= (u8)bytes[i];
		seed *= 1099511628211; //64bit FNV_prime
	}
	return seed;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::nullTerminate
 */
b8 str::nullTerminate(u8* buffer, s32 buffer_len)
{
	if (buffer_len <= len)
		return false;

	mem::copy(buffer, bytes, len);
	buffer[len] = 0;
	return true;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::countCharacters
 */
u64 str::countCharacters()
{
	str scan = *this;
	u64 accum = 0;
	while (notnil(scan.advance()))
		accum += 1;
	return accum;
}

/* ------------------------------------------------------------------------------------------------ utf8::str::findFirst
 */
str::pos str::findFirst(u8 c)
{
	for (s32 i = 0; i < len; i++)
	{
		if (bytes[i] == c)
			return pos::found(i);
	}
	return pos::notFound();
}

/* ------------------------------------------------------------------------------------------------ utf8::str::findLast
 */
str::pos str::findLast(u8 c)
{
	for (s32 i = len-1; i >= 0; i--)
	{
		if (bytes[i] == c)
			return pos::found(i);
	}
	return pos::notFound();
}

/* ------------------------------------------------------------------------------------------------ utf8::str::startsWith
 */
b8 str::startsWith(str s)
{
	if (s.len > len)
		return false;
	for (s32 i = 0; i < s.len; i++)
	{
		if (bytes[i] != s.bytes[i])
			return false;
	}
	return true;
}

} // namespace utf8


