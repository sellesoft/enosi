#include "io.h"
#include "format.h"

#include "../containers/stackarray.h"

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
  u64 len = snprintf((char*)buffer, 32, "%g", x);
  return io->write({buffer, len});
}

s64 format(IO* io, f64 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%g", x);
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

s64 format(IO* io, const void* x)
{
  u8 buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%p", x);
  return io->write({buffer, len});
}

s64 format(IO* io, const char* x)
{
  Bytes slice = {(u8*)x, 0};
  while (x[slice.len])
    slice.len += 1;
  return io->write(slice);
}

s64 format(IO* io, const SanitizeControlCharacters& x)
{
  StackArray<u8, 255> buffer;
  
  auto flush = [&buffer, io]() { io->write(buffer.asSlice()); buffer.len = 0; };

  s64 bytes_written = 0;
  auto write = [&buffer, &bytes_written, flush](str s)
  {
    if (buffer.len + s.len > buffer.capacity())
      flush();

    for (u64 i = 0; i < s.len; ++i)
      *buffer.push() = s.ptr[i];
    
    bytes_written += s.len;
  };

  str s = x.x;
  while (!s.isEmpty())
  {
    utf8::Codepoint c = s.advance();

    switch (c.codepoint)
    {
      case '\a': write("\\a"_str); break;
      case '\b': write("\\b"_str); break;
      case '\e': write("\\e"_str); break;
      case '\f': write("\\f"_str); break;
      case '\n': write("\\n"_str); break;
      case '\r': write("\\r"_str); break;
      case '\t': write("\\t"_str); break;
      case '\v': write("\\v"_str); break;
      case '\0': write("\\0"_str); break;
      default: {
        // lol
        utf8::Char ch = utf8::encodeCharacter(c.codepoint);
        write({ch.bytes, ch.count});
      } break;
    }
  }

  if (buffer.len)
    flush();

  return bytes_written;
}

}
