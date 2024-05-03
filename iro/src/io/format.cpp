#include "io.h"
#include "format.h"

#include "stdlib.h"
#include "stdio.h"

namespace iro::io
{

s64 format(IO* io, u8 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%hhu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u16 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%hu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u32 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%u", x);
	return io->write({buffer, len});
}

s64 format(IO* io, u64 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%lu", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s8 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%hhi", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s16 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%hi", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s32 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%i", x);
	return io->write({buffer, len});
}

s64 format(IO* io, s64 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%li", x);
	return io->write({buffer, len});
}

s64 format(IO* io, f32 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%f", x);
	return io->write({buffer, len});
}

s64 format(IO* io, f64 x)
{
	u8  buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%f", x);
	return io->write({buffer, len});
}

s64 format(IO* io, str x)
{
	return io->write(x);
}

s64 format(IO* io, char x)
{
	return io->write({(u8*)&x, 1});
}

s64 format(IO* io, void* x)
{
	u8 buffer[32];
	u64 len = snprintf((char*)buffer, 32, "%p", x);
	return io->write({buffer, len});
}

s64 format(IO* io, const char* x)
{
	s64 n = 0;
	while (x[n])
	{
		if (!format(io, x[n]))
			break;
		n += 1;
	}
	return n;
}

s64 format(IO* io, const SanitizeControlCharacters& x)
{
	s64 bytes_written = 0;
	str s = x.x;
	while (!s.isempty())
	{
		utf8::Codepoint c = s.advance();

		switch (c.codepoint)
		{
			case '\a': bytes_written += io->write("\\a"_str); break;
			case '\b': bytes_written += io->write("\\b"_str); break;
			case '\e': bytes_written += io->write("\\e"_str); break;
			case '\f': bytes_written += io->write("\\f"_str); break;
			case '\n': bytes_written += io->write("\\n"_str); break;
			case '\r': bytes_written += io->write("\\r"_str); break;
			case '\t': bytes_written += io->write("\\t"_str); break;
			case '\v': bytes_written += io->write("\\v"_str); break;
			default: {
				// lol
				utf8::Char ch = utf8::encodeCharacter(c.codepoint);
				bytes_written += io->write({ch.bytes, ch.count});
			} break;
		}
	}
	return bytes_written;
}

}
